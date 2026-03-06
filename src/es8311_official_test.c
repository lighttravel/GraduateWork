/**
 * @file es8311_official_test.c
 * @brief 使用ESP-IDF官方ES8311驱动进行音频测试
 *
 * 参考立创例程和ESP-IDF官方ES8311驱动
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "es8311.h"

static const char *TAG = "ES8311_TEST";

// Moji小智AI官方引脚配置
#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2C_NUM            I2C_NUM_0
#define I2C_FREQ_HZ        100000

#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DO_IO          GPIO_NUM_13    // DIN - 播放

#define EXAMPLE_SAMPLE_RATE     16000
#define EXAMPLE_MCLK_MULTIPLE   256
#define EXAMPLE_VOICE_VOLUME    70

static i2s_chan_handle_t tx_handle = NULL;
static es8311_handle_t es8311_handle = NULL;

// 初始化I2C（旧版API，ES8311驱动需要）
static esp_err_t init_i2c(void)
{
    ESP_LOGI(TAG, "初始化I2C...");
    ESP_LOGI(TAG, "SCL = GPIO%d, SDA = GPIO%d", I2C_SCL_PIN, I2C_SDA_PIN);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2C初始化成功");
    return ESP_OK;
}

// 初始化ES8311（使用官方驱动）
static esp_err_t init_es8311(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "初始化ES8311（使用官方驱动）");
    ESP_LOGI(TAG, "========================================");

    // 创建ES8311句柄
    es8311_handle = es8311_create(I2C_NUM, ES8311_ADDRRES_0);
    if (es8311_handle == NULL) {
        ESP_LOGE(TAG, "❌ ES8311创建失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ ES8311句柄创建成功");

    // 配置时钟
    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,  // 从MCLK引脚输入
        .mclk_frequency = EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE,
        .sample_frequency = EXAMPLE_SAMPLE_RATE
    };

    // 初始化ES8311
    esp_err_t ret = es8311_init(
        es8311_handle,
        &es_clk,
        ES8311_RESOLUTION_16,
        ES8311_RESOLUTION_16
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ ES8311初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ ES8311初始化成功");

    // 配置采样率
    ret = es8311_sample_frequency_config(
        es8311_handle,
        EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE,
        EXAMPLE_SAMPLE_RATE
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 采样率配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ 采样率配置成功: %dHz", EXAMPLE_SAMPLE_RATE);

    // 设置音量
    ret = es8311_voice_volume_set(es8311_handle, EXAMPLE_VOICE_VOLUME, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 音量设置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ 音量设置成功: %d", EXAMPLE_VOICE_VOLUME);

    // 配置麦克风（禁用，只测试扬声器）
    ret = es8311_microphone_config(es8311_handle, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 麦克风配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ 麦克风配置成功（已禁用）");

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ ES8311配置完成！");
    ESP_LOGI(TAG, "========================================\n");

    return ESP_OK;
}

// 初始化I2S
static esp_err_t init_i2s(void)
{
    ESP_LOGI(TAG, "初始化I2S...");

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

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = EXAMPLE_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = EXAMPLE_MCLK_MULTIPLE,
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
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
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

    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2S初始化成功\n");
    return ESP_OK;
}

// 生成正弦波
static void generate_sine_wave(int16_t *buffer, int samples, int frequency) {
    const float amplitude = 20000.0f;
    const float sample_period = 1.0f / EXAMPLE_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        buffer[i * 2] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
        buffer[i * 2 + 1] = buffer[i * 2];
    }
}

// 播放测试音
static void play_test_tone(int frequency, int duration_ms) {
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "播放测试音: %dHz, %dms", frequency, duration_ms);
    ESP_LOGI(TAG, "========================================");

    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    generate_sine_wave(buffer, total_samples, frequency);

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 写入 %d 字节音频数据", bytes_written);
        ESP_LOGI(TAG, "🔊 应该听到清晰的%dHz音调!", frequency);
    } else {
        ESP_LOGE(TAG, "❌ I2S写入失败: %s", esp_err_to_name(ret));
    }

    free(buffer);
    vTaskDelay(pdMS_TO_TICKS(duration_ms + 100));
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311官方驱动测试\n");
    printf("  使用ESP-IDF官方es8311驱动\n");
    printf("========================================\n");
    printf("\n");

    // 初始化I2C
    if (init_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "I2C初始化失败，程序终止");
        return;
    }

    // 初始化ES8311
    if (init_es8311() != ESP_OK) {
        ESP_LOGE(TAG, "ES8311初始化失败，程序终止");
        return;
    }

    // 初始化I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败，程序终止");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 测试不同频率
    int frequencies[] = {440, 880, 1000, 2000};
    int durations[] = {2000, 1000, 2000, 1000};

    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "\n【测试 %d/%d】", i+1, 4);
        play_test_tone(frequencies[i], durations[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "所有测试完成!");
    ESP_LOGI(TAG, "========================================");

    // 循环播放
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "\n循环测试: 播放1000Hz音调...");
        play_test_tone(1000, 1000);
    }
}
