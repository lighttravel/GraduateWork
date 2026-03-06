/**
 * @file i2s_audio_test_no_i2c.c
 * @brief I2S音频测试 - 不使用I2C配置ES8311
 * 
 * 直接通过I2S发送音频数据，测试ES8311是否有默认配置
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "I2S_TEST";

// Moji小智AI官方I2S引脚配置
#define I2S_BCLK_PIN      GPIO_NUM_14    // BCLK
#define I2S_LRCK_PIN      GPIO_NUM_12    // LRCK/WS
#define I2S_DIN_PIN       GPIO_NUM_13    // DIN (播放)
#define I2S_MCLK_PIN      GPIO_NUM_6     // MCLK

static i2s_chan_handle_t tx_handle = NULL;

// 生成正弦波
static void generate_sine_wave(int16_t *buffer, int samples, int frequency) {
    const float amplitude = 16000.0f;  // 较小振幅，避免过载
    const float sample_period = 1.0f / 16000.0f;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        buffer[i * 2] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));      // 左声道
        buffer[i * 2 + 1] = buffer[i * 2];                                           // 右声道（相同）
    }
}

// 初始化I2S
static esp_err_t init_i2s(void) {
    ESP_LOGI(TAG, "初始化I2S...");
    ESP_LOGI(TAG, "MCLK = GPIO%d", I2S_MCLK_PIN);
    ESP_LOGI(TAG, "BCLK = GPIO%d", I2S_BCLK_PIN);
    ESP_LOGI(TAG, "LRCK = GPIO%d", I2S_LRCK_PIN);
    ESP_LOGI(TAG, "DIN  = GPIO%d", I2S_DIN_PIN);

    // 配置GPIO
    gpio_reset_pin(I2S_MCLK_PIN);
    gpio_set_direction(I2S_MCLK_PIN, GPIO_MODE_OUTPUT);

    // 创建I2S通道
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
        ESP_LOGE(TAG, "创建I2S通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置I2S标准模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // 启用MCLK
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK_PIN,
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_LRCK_PIN,
            .dout = I2S_DIN_PIN,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化I2S失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2S初始化成功");
    return ESP_OK;
}

// 播放测试音
static void play_test_tone(int frequency, int duration_ms) {
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "播放测试音: %dHz, %dms", frequency, duration_ms);
    ESP_LOGI(TAG, "========================================");

    const int sample_rate = 16000;
    const int total_samples = (sample_rate * duration_ms) / 1000;

    // 分配立体声缓冲区
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));
    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    // 生成正弦波
    generate_sine_wave(buffer, total_samples, frequency);

    // 启用I2S
    esp_err_t ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S失败: %s", esp_err_to_name(ret));
        free(buffer);
        return;
    }

    // 写入音频数据
    size_t bytes_written;
    ret = i2s_channel_write(tx_handle, (uint8_t *)buffer, total_samples * 2 * sizeof(int16_t), &bytes_written, pdMS_TO_TICKS(5000));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 写入 %d 字节音频数据", bytes_written);
        ESP_LOGI(TAG, "🔊 如果ES8311有默认配置，应该听到%dHz音调!", frequency);
    } else {
        ESP_LOGE(TAG, "❌ I2S写入失败: %s", esp_err_to_name(ret));
    }

    free(buffer);

    // 等待播放完成
    vTaskDelay(pdMS_TO_TICKS(duration_ms + 100));

    // 禁用I2S
    i2s_channel_disable(tx_handle);
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2S音频测试（不使用I2C配置）\n");
    printf("  测试ES8311是否有默认配置\n");
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "说明：");
    ESP_LOGI(TAG, "  本程序不通过I2C配置ES8311");
    ESP_LOGI(TAG, "  直接发送I2S音频数据");
    ESP_LOGI(TAG, "  如果ES8311有默认配置，应该能听到声音");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  如果听到声音：ES8311有默认配置");
    ESP_LOGI(TAG, "  如果没声音：需要I2C配置ES8311");
    printf("\n");

    // 初始化I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 测试不同频率
    int frequencies[] = {440, 880, 1000, 2000};
    int durations[] = {2000, 1000, 2000, 1000};

    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "\n【测试 %d/%d】", i+1, 4);
        play_test_tone(frequencies[i], durations[i]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "测试完成!");
    ESP_LOGI(TAG, "========================================");

    // 循环播放1000Hz测试音
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "\n循环测试: 播放1000Hz音调...");
        play_test_tone(1000, 1000);
    }
}
