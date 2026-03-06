#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SPK_DIAG";

// 9-pin ES8311 board mapping (no external PA_EN pin)
// Current wiring confirmed by user:
// SDA->GPIO5, SCL->GPIO4, MCLK->GPIO6, SCLK->GPIO14, DIN->GPIO11, LRCK->GPIO12, DOUT->GPIO13
#define I2C_SCL_PIN_MAIN  GPIO_NUM_4
#define I2C_SDA_PIN_MAIN  GPIO_NUM_5
#define I2C_SCL_PIN_ALT   GPIO_NUM_8
#define I2C_SDA_PIN_ALT   GPIO_NUM_9
// Locked by latest audible result: use SEGMENT A mapping on this module/wiring.
#define I2S_BCLK_PIN      GPIO_NUM_14
#define I2S_LRCK_PIN      GPIO_NUM_12
#define I2S_DOUT_PIN      GPIO_NUM_11 // ESP32-S3 -> ES8311 DIN
#define I2S_DIN_PIN       GPIO_NUM_13 // ES8311 DOUT -> ESP32-S3
#define I2S_MCLK_PIN      GPIO_NUM_6

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_FREQ_HZ     1000
#define PI_F              3.14159265358979323846f

#define ES8311_REG_RESET          0x00
#define ES8311_REG_CLK_MGR_01     0x01
#define ES8311_REG_CLK_MGR_02     0x02
#define ES8311_REG_CLK_MGR_03     0x03
#define ES8311_REG_CLK_MGR_04     0x04
#define ES8311_REG_CLK_MGR_05     0x05
#define ES8311_REG_DAC_IFACE      0x09
#define ES8311_REG_ADC_IFACE      0x0A
#define ES8311_REG_SYSTEM_0D      0x0D
#define ES8311_REG_SYSTEM_0E      0x0E
#define ES8311_REG_SYSTEM_10      0x10
#define ES8311_REG_SYSTEM_11      0x11
#define ES8311_REG_SYSTEM_12      0x12
#define ES8311_REG_SYSTEM_14      0x14
#define ES8311_REG_ADC_15         0x15
#define ES8311_REG_GPIO_17        0x17
#define ES8311_REG_DAC_POWER_2B   0x2B
#define ES8311_REG_DAC_VOL_2F     0x2F
#define ES8311_REG_DAC_MUTE_32    0x32
#define ES8311_REG_DAC_37         0x37
#define ES8311_REG_GPIO_44        0x44
#define ES8311_REG_GPIO_45        0x45

static const uint8_t k_probe_addresses[] = {0x18, 0x19};

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_codec = NULL;
static uint8_t s_codec_addr = 0;
static i2c_port_num_t s_i2c_port = I2C_NUM_0;
static gpio_num_t s_i2c_scl = GPIO_NUM_NC;
static gpio_num_t s_i2c_sda = GPIO_NUM_NC;
static uint32_t s_i2c_speed_hz = 0;
static i2s_chan_handle_t s_tx_chan = NULL;
static bool s_tx_enabled = false;

typedef struct {
    const char *name;
    gpio_num_t bclk_pin;
    gpio_num_t ws_pin;
    bool use_mclk;
    uint32_t sample_rate_hz;
    i2s_slot_bit_width_t slot_bit_width;
    bool ws_pol;
    bool bit_shift;
    bool left_align;
    uint8_t dac_iface;
} audio_case_t;

static esp_err_t codec_read_reg_handle(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, pdMS_TO_TICKS(500));
}

static esp_err_t codec_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_codec, buf, sizeof(buf), pdMS_TO_TICKS(500));
}

static esp_err_t codec_read_reg(uint8_t reg, uint8_t *val)
{
    return codec_read_reg_handle(s_codec, reg, val);
}

static esp_err_t codec_write_reg_checked(uint8_t reg, uint8_t val)
{
    esp_err_t ret = codec_write_reg(reg, val);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "REG[0x%02X] write 0x%02X failed: %s", reg, val, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t codec_write_verify(uint8_t reg, uint8_t expected)
{
    uint8_t actual = 0;
    esp_err_t ret = codec_write_reg(reg, expected);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "REG[0x%02X] write failed: %s", reg, esp_err_to_name(ret));
        return ret;
    }

    ret = codec_read_reg(reg, &actual);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "REG[0x%02X] readback failed: %s", reg, esp_err_to_name(ret));
        return ret;
    }

    if (actual != expected) {
        ESP_LOGE(TAG, "REG[0x%02X] mismatch expected=0x%02X actual=0x%02X", reg, expected, actual);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "REG[0x%02X] verify ok: 0x%02X", reg, actual);
    return ESP_OK;
}

static void i2c_deinit_bus(void)
{
    if (s_codec) {
        i2c_master_bus_rm_device(s_codec);
        s_codec = NULL;
    }
    if (s_i2c_bus) {
        i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }
    s_codec_addr = 0;
    s_i2c_port = I2C_NUM_0;
    s_i2c_scl = GPIO_NUM_NC;
    s_i2c_sda = GPIO_NUM_NC;
    s_i2c_speed_hz = 0;
}

static void i2c_log_line_levels(gpio_num_t scl, gpio_num_t sda)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << scl) | (1ULL << sda),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    int scl_level = gpio_get_level(scl);
    int sda_level = gpio_get_level(sda);
    ESP_LOGI(TAG, "I2C line idle level (with internal PU): SCL=%d SDA=%d", scl_level, sda_level);
}

static void i2c_recover_bus(gpio_num_t scl, gpio_num_t sda)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << scl),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << sda),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    // 9 pulses on SCL can release a slave that is stuck waiting for clocks.
    gpio_set_level(scl, 1);
    esp_rom_delay_us(5);
    for (int i = 0; i < 9; ++i) {
        gpio_set_level(scl, 0);
        esp_rom_delay_us(5);
        gpio_set_level(scl, 1);
        esp_rom_delay_us(5);
    }

    ESP_LOGI(TAG, "I2C bus recovery pulses sent on SCL=%d", scl);
}

static esp_err_t i2c_init_bus(i2c_port_num_t port, gpio_num_t scl, gpio_num_t sda, uint32_t speed_hz)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    i2c_log_line_levels(scl, sda);
    i2c_recover_bus(scl, sda);
    i2c_log_line_levels(scl, sda);

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_i2c_port = port;
    s_i2c_scl = scl;
    s_i2c_sda = sda;
    s_i2c_speed_hz = speed_hz;
    ESP_LOGI(
        TAG,
        "I2C init ok: port=%d SCL=%d SDA=%d %luHz",
        (int)s_i2c_port,
        s_i2c_scl,
        s_i2c_sda,
        (unsigned long)s_i2c_speed_hz);
    return ESP_OK;
}

static esp_err_t codec_probe_and_attach(void)
{
    for (size_t i = 0; i < sizeof(k_probe_addresses); ++i) {
        uint8_t addr = k_probe_addresses[i];
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = s_i2c_speed_hz,
        };

        i2c_master_dev_handle_t dev = NULL;
        esp_err_t ret = i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &dev);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Add I2C dev 0x%02X failed: %s", addr, esp_err_to_name(ret));
            continue;
        }

        uint8_t reg00 = 0;
        ret = codec_read_reg_handle(dev, ES8311_REG_RESET, &reg00);
        if (ret == ESP_OK) {
            s_codec = dev;
            s_codec_addr = addr;
            ESP_LOGI(TAG, "ES8311 detected at 0x%02X (reg00=0x%02X)", addr, reg00);
            return ESP_OK;
        }

        ESP_LOGW(TAG, "No ACK at 0x%02X: %s", addr, esp_err_to_name(ret));
        i2c_master_bus_rm_device(dev);
    }

    ESP_LOGE(TAG, "ES8311 not found at 0x18/0x19");
    ESP_LOGE(TAG, "Check wiring: SDA/SCL, 5V power, shared GND");
    return ESP_FAIL;
}

static int i2c_scan_all_addrs(void)
{
    int found = 0;
    ESP_LOGI(TAG, "I2C scan start: 0x08..0x77");
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        esp_err_t ret = i2c_master_probe(s_i2c_bus, addr, 20);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "I2C scan done: found=%d", found);
    return found;
}

static void i2s_deinit(void)
{
    if (s_tx_chan) {
        if (s_tx_enabled) {
            i2s_channel_disable(s_tx_chan);
            s_tx_enabled = false;
        }
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
    }
}

static esp_err_t i2s_init_tx(
    bool use_mclk,
    gpio_num_t tx_dout_pin,
    gpio_num_t bclk_pin,
    gpio_num_t ws_pin,
    uint32_t sample_rate_hz,
    i2s_slot_bit_width_t slot_bit_width,
    bool ws_pol,
    bool bit_shift,
    bool left_align)
{
    i2s_deinit();

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate_hz,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
            .ext_clk_freq_hz = 0,
#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = slot_bit_width,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = ws_pol,
            .bit_shift = bit_shift,
#ifdef I2S_HW_VERSION_2
            .left_align = left_align,
            .big_endian = false,
            .bit_order_lsb = false,
#endif
        },
        .gpio_cfg = {
            .mclk = use_mclk ? I2S_MCLK_PIN : GPIO_NUM_NC,
            .bclk = bclk_pin,
            .ws = ws_pin,
            .dout = tx_dout_pin,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        i2s_deinit();
        return ret;
    }

    ESP_LOGI(
        TAG,
        "I2S init ok: %luHz/16bit/master, MCLK=%s, TX_DOUT=%d, BCLK=%d, LRCK=%d, slot=%s, ws_pol=%d, bit_shift=%d",
        (unsigned long)sample_rate_hz,
        use_mclk ? "configured GPIO" : "NC",
        tx_dout_pin,
        bclk_pin,
        ws_pin,
        slot_bit_width == I2S_SLOT_BIT_WIDTH_16BIT ? "16" :
        slot_bit_width == I2S_SLOT_BIT_WIDTH_24BIT ? "24" :
        slot_bit_width == I2S_SLOT_BIT_WIDTH_32BIT ? "32" : "AUTO",
        ws_pol ? 1 : 0,
        bit_shift ? 1 : 0);
    return ESP_OK;
}

static esp_err_t codec_config_for_playback(uint8_t dac_volume, uint8_t dac_iface, bool use_mclk)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg09 = (uint8_t)(dac_iface & 0xBF);

    // Reset
    ret |= codec_write_reg_checked(ES8311_REG_RESET, 0x80);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Minimal working-style sequence (closer to known-good WROOM setup).
    ret |= codec_write_reg_checked(ES8311_REG_RESET, 0x3F);  // slave mode
    ret |= codec_write_reg_checked(ES8311_REG_CLK_MGR_01, 0x00);
    ret |= codec_write_reg_checked(ES8311_REG_CLK_MGR_02, use_mclk ? 0xF0 : 0x00);
    ret |= codec_write_reg_checked(ES8311_REG_CLK_MGR_03, 0x17);
    ret |= codec_write_reg_checked(ES8311_REG_CLK_MGR_04, 0x00);
    ret |= codec_write_reg_checked(ES8311_REG_CLK_MGR_05, 0x00);
    ret |= codec_write_reg_checked(0x06, 0x00);
    ret |= codec_write_reg_checked(0x08, 0x20);
    ret |= codec_write_reg_checked(ES8311_REG_ADC_IFACE, 0x0C);
    ret |= codec_write_reg_checked(0x2A, 0x04);
    ret |= codec_write_reg_checked(0x2C, 0x10);
    ret |= codec_write_reg_checked(0x2D, 0x10);
    ret |= codec_write_reg_checked(0x12, 0x10);
    ret |= codec_write_reg_checked(0x13, 0x10);
    ret |= codec_write_reg_checked(ES8311_REG_GPIO_45, 0x00);
    ret |= codec_write_reg_checked(ES8311_REG_GPIO_44, 0x58);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 base config failed");
        return ret;
    }

    // Force DAC serial interface format and keep DAC interface enabled (bit6 = 0).
    ret = codec_write_reg_checked(ES8311_REG_DAC_IFACE, reg09);
    if (ret != ESP_OK) {
        return ret;
    }

    // Required strong verification chain
    ret = codec_write_verify(ES8311_REG_DAC_IFACE, reg09);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = codec_write_verify(ES8311_REG_DAC_POWER_2B, 0x00);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = codec_write_verify(ES8311_REG_DAC_MUTE_32, 0x00);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = codec_write_verify(ES8311_REG_DAC_VOL_2F, dac_volume);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(
        TAG,
        "ES8311 configured at 0x%02X, DAC volume=0x%02X, DAC_IFACE(0x09)=0x%02X, MCLK=%s",
        s_codec_addr,
        dac_volume,
        reg09,
        use_mclk ? "ON" : "NC");
    return ESP_OK;
}

static esp_err_t play_stereo_tone_1khz_ms(uint32_t sample_rate_hz, uint32_t duration_ms)
{
    const uint32_t frame_count = (sample_rate_hz * duration_ms) / 1000;
    const size_t sample_count = (size_t)frame_count * 2U;
    const size_t expected_bytes = sample_count * sizeof(int16_t);

    int16_t *pcm = (int16_t *)calloc(sample_count, sizeof(int16_t));
    if (!pcm) {
        ESP_LOGE(TAG, "PCM alloc failed");
        return ESP_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < frame_count; ++i) {
        float phase = (2.0f * PI_F * (float)AUDIO_FREQ_HZ * (float)i) / (float)sample_rate_hz;
        int16_t v = (int16_t)(12000.0f * sinf(phase));
        pcm[(size_t)i * 2U] = v;
        pcm[(size_t)i * 2U + 1U] = v;
    }

    esp_err_t ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        free(pcm);
        return ret;
    }
    s_tx_enabled = true;

    size_t total_written = 0;
    while (total_written < expected_bytes) {
        size_t just_written = 0;
        ret = i2s_channel_write(
            s_tx_chan,
            ((uint8_t *)pcm) + total_written,
            expected_bytes - total_written,
            &just_written,
            pdMS_TO_TICKS(2000));

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2s_channel_write failed: %s", esp_err_to_name(ret));
            i2s_channel_disable(s_tx_chan);
            s_tx_enabled = false;
            free(pcm);
            return ret;
        }

        if (just_written == 0) {
            ESP_LOGE(TAG, "i2s_channel_write timeout (0 bytes)");
            i2s_channel_disable(s_tx_chan);
            s_tx_enabled = false;
            free(pcm);
            return ESP_ERR_TIMEOUT;
        }

        total_written += just_written;
    }

    i2s_channel_disable(s_tx_chan);
    s_tx_enabled = false;
    free(pcm);

    ESP_LOGI(TAG, "I2S write ok: bytes_written=%u expected=%u", (unsigned)total_written, (unsigned)expected_bytes);
    return ESP_OK;
}

static esp_err_t run_volume_step_test(bool use_mclk)
{
    const uint8_t levels[] = {0x10, 0x20, 0x2F};

    ESP_LOGI(TAG, "---- Volume step test start ----");
    for (size_t i = 0; i < sizeof(levels); ++i) {
        uint8_t level = levels[i];
        ESP_LOGI(TAG, "Set volume step %u/%u -> 0x%02X", (unsigned)(i + 1), (unsigned)sizeof(levels), level);

        esp_err_t ret = codec_write_verify(ES8311_REG_DAC_VOL_2F, level);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = i2s_init_tx(
            use_mclk,
            I2S_DOUT_PIN,
            I2S_BCLK_PIN,
            I2S_LRCK_PIN,
            AUDIO_SAMPLE_RATE,
            I2S_SLOT_BIT_WIDTH_AUTO,
            false,
            true,
            true);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = play_stereo_tone_1khz_ms(AUDIO_SAMPLE_RATE, 1000);
        i2s_deinit();
        if (ret != ESP_OK) {
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, "---- Volume step test done ----");
    return ESP_OK;
}

static void __attribute__((unused)) run_continuous_tone_stream(
    gpio_num_t tx_dout_pin,
    gpio_num_t bclk_pin,
    gpio_num_t ws_pin,
    bool use_mclk,
    int segment_ms)
{
    const uint32_t chunk_frames = 320; // 20 ms @ 16kHz
    const size_t sample_count = chunk_frames * 2; // stereo
    const size_t write_bytes = sample_count * sizeof(int16_t);
    int16_t *pcm = (int16_t *)calloc(sample_count, sizeof(int16_t));
    if (!pcm) {
        ESP_LOGE(TAG, "Continuous stream alloc failed");
        return;
    }

    float phase = 0.0f;
    const float phase_step = (2.0f * PI_F * (float)AUDIO_FREQ_HZ) / (float)AUDIO_SAMPLE_RATE;
    const float amp = 9000.0f; // lower amplitude to reduce crackle/clipping

    if (i2s_init_tx(
            use_mclk,
            tx_dout_pin,
            bclk_pin,
            ws_pin,
            AUDIO_SAMPLE_RATE,
            I2S_SLOT_BIT_WIDTH_AUTO,
            false,
            true,
            true) != ESP_OK) {
        ESP_LOGE(TAG, "Continuous stream: i2s_init_tx failed");
        free(pcm);
        return;
    }

    if (i2s_channel_enable(s_tx_chan) != ESP_OK) {
        ESP_LOGE(TAG, "Continuous stream: enable TX failed");
        free(pcm);
        return;
    }
    s_tx_enabled = true;

    ESP_LOGI(TAG, "Continuous tone stream started (1kHz).");
    bool infinite = (segment_ms <= 0);
    int chunks = segment_ms / 20;
    if (!infinite && chunks < 1) {
        chunks = 1;
    }
    for (int c = 0; infinite || c < chunks; ++c) {
        for (uint32_t i = 0; i < chunk_frames; ++i) {
            int16_t v = (int16_t)(amp * sinf(phase));
            pcm[i * 2] = v;
            pcm[i * 2 + 1] = v;
            phase += phase_step;
            if (phase >= 2.0f * PI_F) {
                phase -= 2.0f * PI_F;
            }
        }

        size_t written = 0;
        esp_err_t ret = i2s_channel_write(s_tx_chan, pcm, write_bytes, &written, pdMS_TO_TICKS(2000));
        if (ret != ESP_OK || written != write_bytes) {
            ESP_LOGE(TAG, "Continuous stream write failed ret=%s written=%u expected=%u",
                     esp_err_to_name(ret), (unsigned)written, (unsigned)write_bytes);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    i2s_channel_disable(s_tx_chan);
    s_tx_enabled = false;
    free(pcm);
}

static bool run_mclk_pass(const char *name, bool use_mclk)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Run %s", name);
    ESP_LOGI(TAG, "========================================");

    // Configure codec first to verify I2C remains stable before enabling I2S clocks.
    esp_err_t ret = codec_config_for_playback(0x2F, 0x0C, use_mclk);
    if (ret != ESP_OK) {
        return false;
    }

    ret = i2s_init_tx(
        use_mclk,
        I2S_DOUT_PIN,
        I2S_BCLK_PIN,
        I2S_LRCK_PIN,
        AUDIO_SAMPLE_RATE,
        I2S_SLOT_BIT_WIDTH_AUTO,
        false,
        true,
        true);
    if (ret != ESP_OK) {
        return false;
    }

    ret = play_stereo_tone_1khz_ms(AUDIO_SAMPLE_RATE, 2000);
    i2s_deinit();
    return ret == ESP_OK;
}

static esp_err_t play_case_marker(uint32_t sample_rate_hz, int count)
{
    for (int i = 0; i < count; ++i) {
        esp_err_t ret = play_stereo_tone_1khz_ms(sample_rate_hz, 120);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}

static esp_err_t run_audio_case(const audio_case_t *c, int index, int total)
{
    ESP_LOGW(TAG, "----------------------------------------");
    ESP_LOGW(
        TAG,
        "CASE %d/%d %s | BCLK=%d LRCK=%d MCLK=%s SR=%lu slot=%s ws_pol=%d bit_shift=%d reg09=0x%02X",
        index + 1,
        total,
        c->name,
        c->bclk_pin,
        c->ws_pin,
        c->use_mclk ? "ON" : "NC",
        (unsigned long)c->sample_rate_hz,
        c->slot_bit_width == I2S_SLOT_BIT_WIDTH_16BIT ? "16" :
        c->slot_bit_width == I2S_SLOT_BIT_WIDTH_24BIT ? "24" :
        c->slot_bit_width == I2S_SLOT_BIT_WIDTH_32BIT ? "32" : "AUTO",
        c->ws_pol ? 1 : 0,
        c->bit_shift ? 1 : 0,
        c->dac_iface);

    esp_err_t ret = codec_config_for_playback(0x2F, c->dac_iface, c->use_mclk);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2s_init_tx(
        c->use_mclk,
        I2S_DOUT_PIN,
        c->bclk_pin,
        c->ws_pin,
        c->sample_rate_hz,
        c->slot_bit_width,
        c->ws_pol,
        c->bit_shift,
        c->left_align);
    if (ret != ESP_OK) {
        return ret;
    }

    // Marker beeps help identify case number by ear before main tone.
    int marker_count = index + 1;
    ret = play_case_marker(c->sample_rate_hz, marker_count);
    if (ret != ESP_OK) {
        i2s_deinit();
        return ret;
    }

    ret = play_stereo_tone_1khz_ms(c->sample_rate_hz, 2500);
    i2s_deinit();
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(700));
    return ESP_OK;
}

static void run_format_sweep_loop(void)
{
    const audio_case_t cases[] = {
        {"SWAP_BASE", I2S_BCLK_PIN, I2S_LRCK_PIN, false, 16000, I2S_SLOT_BIT_WIDTH_AUTO, false, true, true, 0x0C},
        {"SWAP_NOBITSHIFT", I2S_BCLK_PIN, I2S_LRCK_PIN, false, 16000, I2S_SLOT_BIT_WIDTH_AUTO, false, false, true, 0x0C},
        {"SWAP_WSPOL1", I2S_BCLK_PIN, I2S_LRCK_PIN, false, 16000, I2S_SLOT_BIT_WIDTH_AUTO, true, true, true, 0x0C},
        {"SWAP_MCLK_ON", I2S_BCLK_PIN, I2S_LRCK_PIN, true, 16000, I2S_SLOT_BIT_WIDTH_AUTO, false, true, true, 0x0C},
        {"ORIG_BASE", I2S_LRCK_PIN, I2S_BCLK_PIN, false, 16000, I2S_SLOT_BIT_WIDTH_AUTO, false, true, true, 0x0C},
        {"SWAP_SR48K", I2S_BCLK_PIN, I2S_LRCK_PIN, false, 48000, I2S_SLOT_BIT_WIDTH_AUTO, false, true, true, 0x0C},
        {"SWAP_REG09_00", I2S_BCLK_PIN, I2S_LRCK_PIN, false, 16000, I2S_SLOT_BIT_WIDTH_AUTO, false, true, true, 0x00},
        {"SWAP_REG09_10", I2S_BCLK_PIN, I2S_LRCK_PIN, false, 16000, I2S_SLOT_BIT_WIDTH_AUTO, false, true, true, 0x10},
    };

    ESP_LOGW(TAG, "Entering format sweep loop. Listen for the clearest pure 1kHz tone case.");
    while (1) {
        for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
            esp_err_t ret = run_audio_case(&cases[i], i, (int)(sizeof(cases) / sizeof(cases[0])));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "CASE %d failed: %s", i + 1, esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(600));
            }
        }
        ESP_LOGW(TAG, "Sweep round complete. Restarting from CASE 1.");
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

void app_main(void)
{
    printf("\n");
    printf("===============================================\n");
    printf(" ESP32-S3 ES8311 Speaker Diagnostic (9-pin)\n");
    printf(" I2S / AMP / Volume / MCLK A-B\n");
    printf("===============================================\n");

    ESP_LOGW(TAG, "Power requirement: this 9-pin module must be powered by 5V and share GND.");
    ESP_LOGI(TAG, "No external PA_EN control is used in this firmware (9-pin board).");

    const uint32_t speed_candidates[] = {100000, 50000, 10000};
    const i2c_port_num_t port_candidates[] = {I2C_NUM_0, I2C_NUM_1};
    const gpio_num_t scl_candidates[] = {I2C_SCL_PIN_MAIN, I2C_SCL_PIN_ALT};
    const gpio_num_t sda_candidates[] = {I2C_SDA_PIN_MAIN, I2C_SDA_PIN_ALT};

    bool codec_found = false;
    for (size_t pi = 0; pi < 2 && !codec_found; ++pi) {
        for (size_t p = 0; p < 2 && !codec_found; ++p) {
            for (size_t s = 0; s < sizeof(speed_candidates) / sizeof(speed_candidates[0]); ++s) {
                i2c_deinit_bus();
                ESP_LOGI(
                    TAG,
                    "Try I2C port=%d pinset %u/%u: SCL=%d SDA=%d @ %luHz",
                    (int)port_candidates[pi],
                    (unsigned)(p + 1),
                    2u,
                    scl_candidates[p],
                    sda_candidates[p],
                    (unsigned long)speed_candidates[s]);

                esp_err_t ret = i2c_init_bus(port_candidates[pi], scl_candidates[p], sda_candidates[p], speed_candidates[s]);
                if (ret != ESP_OK) {
                    continue;
                }

                if (s == 0) {
                    i2c_scan_all_addrs();
                }

                ret = codec_probe_and_attach();
                if (ret == ESP_OK) {
                    codec_found = true;
                    break;
                }
            }
        }
    }

    if (!codec_found) {
        ESP_LOGE(TAG, "All I2C pin/speed probes failed.");
        ESP_LOGE(TAG, "Hardware checklist:");
        ESP_LOGE(TAG, "1) Module VCC really on 5V, not 3.3V");
        ESP_LOGE(TAG, "2) ESP32-S3 GND and module GND are common");
        ESP_LOGE(TAG, "3) SDA/SCL are not swapped and have continuity");
        ESP_LOGE(TAG, "4) ES8311 board is not damaged");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGW(TAG, "Clock mapping locked: BCLK=GPIO14, LRCK=GPIO12");
    ESP_LOGW(TAG, "Running staged audio diagnostics: MCLK off/on, volume steps, then format sweep.");

    bool mclk_off_ok = run_mclk_pass("PASS 1: MCLK OFF", false);
    ESP_LOGW(TAG, "PASS 1 complete: config=%s", mclk_off_ok ? "OK" : "FAILED");
    if (mclk_off_ok) {
        esp_err_t ret = run_volume_step_test(false);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Volume step test (MCLK OFF) failed: %s", esp_err_to_name(ret));
        }
    }

    bool mclk_on_ok = run_mclk_pass("PASS 2: MCLK ON", true);
    ESP_LOGW(TAG, "PASS 2 complete: config=%s", mclk_on_ok ? "OK" : "FAILED");
    if (mclk_on_ok) {
        esp_err_t ret = run_volume_step_test(true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Volume step test (MCLK ON) failed: %s", esp_err_to_name(ret));
        }
    }

    ESP_LOGW(TAG, "Entering continuous format sweep. Listen for any audible difference between cases.");
    run_format_sweep_loop();
}
