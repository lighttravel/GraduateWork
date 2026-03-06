#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ES8311_MIN";

#define I2C_PORT_NUM         I2C_NUM_0
#define I2C_SCL_PIN          GPIO_NUM_4
#define I2C_SDA_PIN          GPIO_NUM_5
#define I2C_FREQ_HZ          100000
#define ES8311_ADDR          0x18

#define I2S_PORT_NUM         I2S_NUM_0
#define I2S_MCLK_PIN         GPIO_NUM_6
#define I2S_BCLK_PIN         GPIO_NUM_14
#define I2S_WS_PIN           GPIO_NUM_12
#define I2S_DOUT_PIN         GPIO_NUM_11

#define SAMPLE_RATE_HZ       24000
#define MCLK_FREQ_HZ         12288000
#define TONE_FREQ_HZ         1000
#define TONE_AMPLITUDE       8000.0f
#define DAC_VOLUME_REG       0xFF
#define PI_F                 3.14159265358979323846f

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_i2c_dev = NULL;
static i2s_chan_handle_t s_tx = NULL;
static float s_phase = 0.0f;

static esp_err_t codec_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    esp_err_t ret = i2c_master_transmit(s_i2c_dev, buf, sizeof(buf), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        printf("[ERROR] write reg 0x%02X failed: %d\n", reg, (int)ret);
    }
    return ret;
}

static esp_err_t codec_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_i2c_dev, &reg, 1, value, 1, pdMS_TO_TICKS(1000));
}

static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_i2c_dev);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t reg00 = 0;
    ret = codec_read_reg(0x00, &reg00);
    if (ret != ESP_OK) {
        return ret;
    }

    printf("ES8311 ACK at 0x%02X (REG00=0x%02X)\n", ES8311_ADDR, reg00);
    return ESP_OK;
}

static esp_err_t init_codec(void)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg02 = 0;
    uint8_t value = 0;

    printf("[1] Reset codec\n");
    ret |= codec_write_reg(0x00, 0x1F);
    vTaskDelay(pdMS_TO_TICKS(20));
    ret |= codec_write_reg(0x00, 0x00);
    ret |= codec_write_reg(0x00, 0x80);

    printf("[2] Configure clock for %d Hz with MCLK=%d\n", SAMPLE_RATE_HZ, MCLK_FREQ_HZ);
    ret |= codec_write_reg(0x01, 0x3F);
    ret |= codec_read_reg(0x02, &reg02);
    reg02 = (uint8_t)((reg02 & 0x07U) | 0x20U);
    ret |= codec_write_reg(0x02, reg02);
    ret |= codec_write_reg(0x03, 0x10);
    ret |= codec_write_reg(0x04, 0x10);
    ret |= codec_write_reg(0x05, 0x00);
    ret |= codec_write_reg(0x06, 0x03);
    ret |= codec_write_reg(0x07, 0x00);
    ret |= codec_write_reg(0x08, 0xFF);

    printf("[3] Configure 16-bit I2S format\n");
    ret |= codec_write_reg(0x09, 0x0C);
    ret |= codec_write_reg(0x0A, 0x0C);

    printf("[4] Power up playback path\n");
    ret |= codec_write_reg(0x0D, 0x01);
    ret |= codec_write_reg(0x0E, 0x02);
    ret |= codec_write_reg(0x12, 0x00);
    ret |= codec_write_reg(0x13, 0x10);
    ret |= codec_write_reg(0x1C, 0x6A);
    ret |= codec_write_reg(0x37, 0x08);

    printf("[5] Unmute and set volume\n");
    ret |= codec_write_reg(0x31, 0x00);
    ret |= codec_write_reg(0x32, DAC_VOLUME_REG);

    if (ret != ESP_OK) {
        return ret;
    }

    codec_read_reg(0x09, &value);
    printf("REG09=0x%02X\n", value);
    codec_read_reg(0x13, &value);
    printf("REG13=0x%02X\n", value);
    codec_read_reg(0x31, &value);
    printf("REG31=0x%02X\n", value);
    codec_read_reg(0x32, &value);
    printf("REG32=0x%02X\n", value);
    return ESP_OK;
}

static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_PORT_NUM,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE_HZ,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_512,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
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

    ret = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2s_channel_enable(s_tx);
    if (ret == ESP_OK) {
        printf("I2S init ok: %dHz MCLK=%d BCLK=%d WS=%d DOUT=%d\n",
               SAMPLE_RATE_HZ, I2S_MCLK_PIN, I2S_BCLK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    }
    return ret;
}

static void fill_tone_chunk(int16_t *buffer, uint32_t frames)
{
    const float step = (2.0f * PI_F * (float)TONE_FREQ_HZ) / (float)SAMPLE_RATE_HZ;

    for (uint32_t i = 0; i < frames; ++i) {
        int16_t sample = (int16_t)(TONE_AMPLITUDE * sinf(s_phase));
        buffer[i * 2U] = sample;
        buffer[i * 2U + 1U] = sample;
        s_phase += step;
        if (s_phase >= 2.0f * PI_F) {
            s_phase -= 2.0f * PI_F;
        }
    }
}

static void play_continuous_tone(void)
{
    const uint32_t frames = 480;
    const size_t sample_count = frames * 2U;
    const size_t bytes = sample_count * sizeof(int16_t);
    int16_t *buffer = (int16_t *)calloc(sample_count, sizeof(int16_t));
    uint32_t chunks = 0;

    if (!buffer) {
        printf("[ERROR] tone buffer alloc failed\n");
        return;
    }

    printf("Continuous 1kHz started\n");
    while (1) {
        fill_tone_chunk(buffer, frames);

        size_t written = 0;
        esp_err_t ret = i2s_channel_write(s_tx, buffer, bytes, &written, pdMS_TO_TICKS(2000));
        if (ret != ESP_OK || written != bytes) {
            printf("[ERROR] i2s write failed ret=%d written=%u expected=%u\n",
                   (int)ret, (unsigned)written, (unsigned)bytes);
        }

        ++chunks;
        if ((chunks % 50U) == 0U) {
            printf("tone running ... last_bytes=%u\n", (unsigned)written);
        }
    }
}

void app_main(void)
{
    printf("\n========================================\n");
    printf(" ES8311 CLEAN 1kHz TEST\n");
    printf(" 24kHz, 16-bit, MCLK=12.288MHz\n");
    printf("========================================\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    if (init_codec() != ESP_OK) {
        printf("[ERROR] codec init failed\n");
        return;
    }

    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S init failed\n");
        return;
    }

    ESP_LOGI(TAG, "Starting continuous tone");
    play_continuous_tone();
}
