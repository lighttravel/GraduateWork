/**
 * @file es8311_swap_gpio_test.c
 * @brief 简化版DOUT引脚测试 - 避免看门狗问题
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char *TAG = "SWAP_GPIO";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12

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
    printf("ES8311 init...\n");
    es8311_write_reg(0x00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));
    es8311_write_reg(0x00, 0x3F);
    es8311_write_reg(0x02, 0xF0);
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

// 初始化I2S一次，然后切换DOUT引脚
static esp_err_t init_i2s_base(void)
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
    if (ret != ESP_OK) {
        printf("Failed to create channel: %d\n", ret);
        return ret;
    }

    printf("I2S channel created\n");
    return ESP_OK;
}

// 用指定的DOUT引脚配置I2S
static esp_err_t config_i2s_dout(int dout_pin)
{
    if (tx_handle == NULL) {
        printf("Error: tx_handle is NULL\n");
        return ESP_FAIL;
    }

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
            .dout = dout_pin,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    esp_err_t ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        printf("Failed to init I2S std mode: %d\n", ret);
        return ret;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret == ESP_OK) {
        printf("I2S enabled with DOUT = GPIO%d\n", dout_pin);
    }

    return ret;
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
    const int duration_ms = 2000;
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
        printf("Sent %d bytes - LISTEN!\n", bytes_written);
    } else {
        printf("Write failed: %d\n", ret);
    }

    free(buffer);
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  DOUT Pin Swap Test (Simplified)\n");
    printf("========================================\n");
    printf("\n");

    printf("IMPORTANT: Speaker makes sound on power-up!\n");
    printf("This means hardware is OK.\n");
    printf("\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }
    printf("[OK] I2C initialized\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    es8311_init();
    printf("[OK] ES8311 initialized\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    if (init_i2s_base() != ESP_OK) {
        printf("[ERROR] I2S base init failed\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    int test = 0;
    int use_gpio11 = 1;  // Start with GPIO11

    while (1) {
        printf("\n");
        printf("========================================\n");
        printf("[Test Round %d]\n", ++test);
        printf("========================================\n");
        printf("\n");

        // 切换DOUT引脚
        int dout_pin = use_gpio11 ? GPIO_NUM_11 : GPIO_NUM_13;
        const char* pin_name = use_gpio11 ? "GPIO11" : "GPIO13";

        printf("Testing DOUT = %s (%s)\n", pin_name, use_gpio11 ? "your wiring" : "swapped");

        // 删除旧的channel并重新创建
        if (tx_handle != NULL) {
            i2s_channel_disable(tx_handle);
            i2s_del_channel(tx_handle);
            tx_handle = NULL;
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        // 重新创建channel
        if (init_i2s_base() != ESP_OK) {
            printf("Failed to recreate I2S channel\n");
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(200));

        // 配置新的DOUT引脚
        if (config_i2s_dout(dout_pin) != ESP_OK) {
            printf("Failed to configure I2S with DOUT = %s\n", pin_name);
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(500));

        // 播放测试音
        printf("Playing 1kHz tone for 2 seconds...\n");
        printf("LISTEN CAREFULLY!\n");
        play_1khz_tone();

        vTaskDelay(pdMS_TO_TICKS(2000));

        // 切换到下一个引脚
        use_gpio11 = !use_gpio11;

        printf("\nWaiting 2 seconds before next test...\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
