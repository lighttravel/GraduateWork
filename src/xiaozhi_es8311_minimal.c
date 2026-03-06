#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "es8311.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "XIAOZHI_MIN";

#define I2C_PORT         I2C_NUM_0
#define I2C_SCL_PIN      GPIO_NUM_4
#define I2C_SDA_PIN      GPIO_NUM_5
#define I2C_SPEED_HZ     100000

#define I2S_PORT         I2S_NUM_0
#define I2S_MCLK_PIN     GPIO_NUM_6
#define I2S_BCLK_PIN     GPIO_NUM_14
#define I2S_WS_PIN       GPIO_NUM_12
#define I2S_DOUT_PIN     GPIO_NUM_11

#define SAMPLE_RATE_HZ   24000
#define TONE_FREQ_HZ     1000
#define TONE_AMPLITUDE   12000.0f

static i2s_chan_handle_t s_tx = NULL;
static es8311_handle_t s_codec = NULL;
static uint8_t s_codec_addr = 0;

static esp_err_t codec_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(I2C_PORT, s_codec_addr, &reg, 1, val, 1, pdMS_TO_TICKS(200));
}

static esp_err_t probe_addr(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t init_i2c(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_SPEED_HZ,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_PORT, &cfg), TAG, "i2c_param_config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0), TAG, "i2c_driver_install failed");
    ESP_LOGI(TAG, "I2C init ok: port=%d SDA=%d SCL=%d @ %dHz",
             (int)I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN, I2C_SPEED_HZ);
    return ESP_OK;
}

static esp_err_t init_codec(void)
{
    const uint8_t addrs[] = {ES8311_ADDRESS_0, ES8311_ADDRESS_1};

    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); ++i) {
        uint8_t addr = addrs[i];
        esp_err_t ret = probe_addr(addr);
        if (ret != ESP_OK) {
            continue;
        }

        s_codec_addr = addr;
        uint8_t reg00 = 0x00;
        ret = codec_read_reg(0x00, &reg00);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "ES8311 ACK at 0x%02X (REG00=0x%02X)", s_codec_addr, reg00);
        } else {
            ESP_LOGW(TAG, "ES8311 ACK at 0x%02X but REG00 read failed: %s",
                     s_codec_addr, esp_err_to_name(ret));
        }
        break;
    }

    ESP_RETURN_ON_FALSE(s_codec_addr != 0, ESP_FAIL, TAG, "ES8311 not found at 0x18/0x19");

    s_codec = es8311_create(I2C_PORT, s_codec_addr);
    ESP_RETURN_ON_FALSE(s_codec != NULL, ESP_FAIL, TAG, "es8311_create failed");

    es8311_clock_config_t clk_cfg = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = 12288000,
        .sample_frequency = SAMPLE_RATE_HZ,
    };

    ESP_RETURN_ON_ERROR(es8311_init(s_codec, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16),
                        TAG, "es8311_init failed");
    ESP_RETURN_ON_ERROR(es8311_voice_mute(s_codec, false), TAG, "es8311_voice_mute failed");

    int vol_set = 0;
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(s_codec, 100, &vol_set), TAG, "es8311_voice_volume_set failed");
    ESP_LOGI(TAG, "ES8311 init ok");

    uint8_t r09 = 0, r31 = 0, r32 = 0;
    (void)codec_read_reg(0x09, &r09);
    (void)codec_read_reg(0x31, &r31);
    (void)codec_read_reg(0x32, &r32);
    ESP_LOGI(TAG, "REG dump: 0x09=0x%02X 0x31=0x%02X 0x32=0x%02X", r09, r31, r32);
    (void)vol_set;
    return ESP_OK;
}

static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, NULL), TAG, "i2s_new_channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE_HZ,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_512,
#ifdef I2S_HW_VERSION_2
            .ext_clk_freq_hz = 0,
#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
#ifdef I2S_HW_VERSION_2
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
#endif
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK_PIN,
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), TAG, "i2s_channel_init_std_mode failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "i2s_channel_enable failed");

    ESP_LOGI(TAG, "I2S init ok: %dHz MCLK=%d BCLK=%d WS=%d DOUT=%d",
             SAMPLE_RATE_HZ, I2S_MCLK_PIN, I2S_BCLK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    return ESP_OK;
}

static void play_continuous_tone(void)
{
    const uint32_t frames = 480; // 20 ms @ 24 kHz
    const size_t samples = frames * 2U;
    const size_t bytes = samples * sizeof(int16_t);
    int16_t *buf = (int16_t *)calloc(samples, sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "tone buffer alloc failed");
        return;
    }

    const float step = (2.0f * (float)M_PI * (float)TONE_FREQ_HZ) / (float)SAMPLE_RATE_HZ;
    float phase = 0.0f;
    uint32_t chunks = 0;

    ESP_LOGW(TAG, "Continuous 1kHz started");
    while (1) {
        for (uint32_t i = 0; i < frames; ++i) {
            int16_t v = (int16_t)(TONE_AMPLITUDE * sinf(phase));
            buf[i * 2U] = v;
            buf[i * 2U + 1U] = v;
            phase += step;
            if (phase >= 2.0f * (float)M_PI) {
                phase -= 2.0f * (float)M_PI;
            }
        }

        size_t written = 0;
        esp_err_t ret = i2s_channel_write(s_tx, buf, bytes, &written, pdMS_TO_TICKS(2000));
        if (ret != ESP_OK || written != bytes) {
            ESP_LOGE(TAG, "i2s write failed ret=%s written=%u expected=%u",
                     esp_err_to_name(ret), (unsigned)written, (unsigned)bytes);
        }

        ++chunks;
        if ((chunks % 50U) == 0U) {
            ESP_LOGI(TAG, "tone running ... last_bytes=%u", (unsigned)written);
        }
    }
}

void app_main(void)
{
    printf("\n========================================\n");
    printf(" XIAOZHI-STYLE ES8311 MINIMAL TEST\n");
    printf(" I2C(4/5) I2S(6/14/12/11) 24kHz MCLK=12.288MHz\n");
    printf("========================================\n");

    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(init_codec());
    ESP_ERROR_CHECK(init_i2s());
    play_continuous_tone();
}
