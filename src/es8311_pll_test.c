/**
 * @file es8311_pll_test.c
 * @brief ES8311 PLL启用测试 - ESP32-S3可能需要PLL
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "PLL_TEST";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DOUT_IO        GPIO_NUM_11

#define EXAMPLE_SAMPLE_RATE     16000
#define ES8311_ADDR             0x18

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));
}

// ES8311初始化 - 启用PLL
static void es8311_init_pll(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    printf("\n");
    printf("========================================\n");
    printf("  ES8311 PLL Initialization\n");
    printf("========================================\n");
    printf("\n");

    // 复位
    printf("Reset...\n");
    es8311_write_reg(0x00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 从机模式
    printf("Slave mode...\n");
    es8311_write_reg(0x00, 0x3F);
    vTaskDelay(pdMS_TO_TICKS(10));

    // *** 关键：启用PLL ***
    printf("Enabling PLL...\n");
    es8311_write_reg(0x02, 0xF0);  // 先使用MCLK
    vTaskDelay(pdMS_TO_TICKS(10));

    // PLL配置 - 16kHz采样率
    es8311_write_reg(0x01, 0x38);  // PLL N = 56
    es8311_write_reg(0x02, 0xF1);  // PLL使能
    vTaskDelay(pdMS_TO_TICKS(50));  // 等待PLL锁定

    printf("PLL enabled\n");

    // 时钟配置
    es8311_write_reg(0x03, 0x17);  // ADC OSR=64x, DAC OSR=128x
    es8311_write_reg(0x04, 0x00);
    es8311_write_reg(0x05, 0x00);
    es8311_write_reg(0x06, 0x00);

    // I2S配置
    es8311_write_reg(0x08, 0x20);
    es8311_write_reg(0x09, 0x0C);  // I2S 16-bit

    // DAC电源
    es8311_write_reg(0x2A, 0x04);
    es8311_write_reg(0x2B, 0x00);

    // 输出使能
    es8311_write_reg(0x12, 0x10);  // LOUT2
    es8311_write_reg(0x13, 0x10);  // LOUT1

    // 音量和取消静音
    es8311_write_reg(0x2F, 0x2F);  // 最大音量
    es8311_write_reg(0x32, 0x00);  // 取消静音

    printf("ES8311 init with PLL complete\n");
}

static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) return ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };

    return i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
}

static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) return ret;

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = EXAMPLE_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = 256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) return ret;

    return i2s_channel_enable(tx_handle);
}

static void generate_1khz_sine(int16_t *buffer, int samples)
{
    const float amplitude = 32000.0f;
    const float freq = 1000.0f;
    const float sample_period = 1.0f / EXAMPLE_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        int16_t sample = (int16_t)(amplitude * sinf(2.0f * M_PI * freq * t));
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }
}

static void play_1khz_tone(void)
{
    const int duration_ms = 3000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) return;

    generate_1khz_sine(buffer, total_samples);

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    if (ret == ESP_OK) {
        printf("Sent %d bytes\n", bytes_written);
    }

    free(buffer);
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    printf("\n");
    printf("========================================\n");
    printf("  ES8311 PLL Test\n");
    printf("  Testing with PLL enabled\n");
    printf("========================================\n");
    printf("\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }
    printf("[OK] I2C initialized\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    es8311_init_pll();
    vTaskDelay(pdMS_TO_TICKS(500));

    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S init failed\n");
        return;
    }
    printf("[OK] I2S initialized\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    int test = 0;
    while (1) {
        printf("\n");
        printf("========================================\n");
        printf("[Test Round %d - PLL ENABLED]\n", ++test);
        printf("========================================\n");
        printf("\n");

        printf("Playing 1kHz for 3 seconds...\n");
        printf("LISTEN!\n");
        play_1khz_tone();

        printf("\nWaiting 3 seconds...\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
