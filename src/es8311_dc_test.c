/**
 * @file es8311_dc_test.c
 * @brief DC输出测试 - 验证音频路径是否真的连通
 * 发送恒定值而不是音频，应该产生持续的"嗡嗡"声
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "DC_TEST";

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

static void es8311_init(void)
{
    printf("Initializing ES8311...\n");
    es8311_write_reg(0x00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));
    es8311_write_reg(0x00, 0x3F);
    es8311_write_reg(0x02, 0xF0);
    es8311_write_reg(0x03, 0x17);
    es8311_write_reg(0x09, 0x0C);
    es8311_write_reg(0x12, 0x10);
    es8311_write_reg(0x13, 0x10);
    es8311_write_reg(0x2B, 0x00);
    es8311_write_reg(0x2F, 0x2F);
    es8311_write_reg(0x32, 0x00);
    printf("Done.\n");
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

// 发送恒定DC值 - 应该产生持续的声音
static void send_dc_tone(void)
{
    const int duration_ms = 3000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) return;

    // 恒定正值 - 这应该产生持续的嗡嗡声
    for (int i = 0; i < total_samples; i++) {
        buffer[i * 2] = 16000;     // 正DC值
        buffer[i * 2 + 1] = 16000; // 右声道相同
    }

    size_t bytes_written;
    i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    printf("Sent %d bytes of DC tone\n", bytes_written);
    free(buffer);
}

// 发送方波 - 容易听到的500Hz方波
static void send_square_wave(void)
{
    const int duration_ms = 3000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) return;

    // 500Hz方波 = 每32个样本切换一次 (16000/500 = 32)
    for (int i = 0; i < total_samples; i++) {
        int16_t sample = ((i / 32) % 2) ? 30000 : -30000;
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }

    size_t bytes_written;
    i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    printf("Sent %d bytes of square wave\n", bytes_written);
    free(buffer);
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    printf("\n");
    printf("========================================\n");
    printf("  DC Output Test\n");
    printf("  Testing if audio path is working\n");
    printf("========================================\n");
    printf("\n");

    printf("This test sends:\n");
    printf("1. DC tone (constant value) - should hum\n");
    printf("2. 500Hz square wave - very audible\n");
    printf("\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    es8311_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S init failed\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    int test = 0;
    while (1) {
        printf("\n");
        printf("========================================\n");
        printf("[Test Round %d]\n", ++test);
        printf("========================================\n");
        printf("\n");

        printf("Test 1: DC tone (3 seconds) - LISTEN for hum...\n");
        send_dc_tone();
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("\nTest 2: 500Hz square wave (3 seconds) - LISTEN!\n");
        send_square_wave();
        vTaskDelay(pdMS_TO_TICKS(1000));

        printf("\nWaiting 3 seconds...\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
