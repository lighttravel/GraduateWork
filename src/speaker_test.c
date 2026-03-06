/**
 * @file speaker_test.c
 * @brief 扬声器测试程序 - 播放测试音乐
 *
 * 功能:
 * 1. 初始化ES8311编解码器
 * 2. 生成测试音频(正弦波音乐)
 * 3. 通过I2S播放到扬声器
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

// 包含配置
#include "../config.h"

static const char *TAG = "SPEAKER_TEST";

// ==================== 音频配置 ====================

#define SAMPLE_RATE      16000       // 采样率 16kHz
#define SAMPLE_BIT_WIDTH 16          // 16-bit PCM
#define VOLUME           80          // 音量 0-100

// ==================== ES8311 I2C地址和寄存器 ====================

#define ES8311_ADDR      0x18
#define ES8311_REG_RESET 0x00
#define ES8311_REG_CLK   0x01
#define ES8311_REG_DAC   0x2F
#define ES8311_REG_DAC2  0x02

// ==================== 生成测试音频 ====================

// 生成正弦波
void generate_sine_wave(int16_t *buffer, int samples, int frequency, int volume) {
    const float amplitude = (volume / 100.0f) * 16000.0f; // 最大音量

    for (int i = 0; i < samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        buffer[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
    }
}

// 生成音符频率
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

// 简单的旋律 - 小星星
typedef struct {
    int note;
    int duration_ms;
} melody_note_t;

// 小星星旋律
static const melody_note_t twinkle_star[] = {
    {NOTE_C4, 500}, {NOTE_C4, 500}, {NOTE_G4, 500}, {NOTE_G4, 500},
    {NOTE_A4, 500}, {NOTE_A4, 500}, {NOTE_G4, 1000},
    {NOTE_F4, 500}, {NOTE_F4, 500}, {NOTE_E4, 500}, {NOTE_E4, 500},
    {NOTE_D4, 500}, {NOTE_D4, 500}, {NOTE_C4, 1000},
};
#define MELODY_LENGTH (sizeof(twinkle_star) / sizeof(melody_note_t))

// ==================== I2C操作 ====================

esp_err_t i2c_master_init(void) {
    ESP_LOGI(TAG, "初始化I2C主设备...");

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_pullup = true,
    };

    i2c_master_config_t i2c_mst_conf = {
        .clk_speed = 100000,  // 100kHz
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &i2c_mst_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2C初始化成功");
    return ESP_OK;
}

esp_err_t es8311_write_reg(uint8_t reg, uint8_t val) {
    uint8_t write_buf[2] = {reg, val};

    esp_err_t ret = i2c_master_transmit(I2C_NUM_0, ES8311_ADDR, write_buf, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311写寄存器失败 [0x%02X]: %s", reg, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t es8311_init_codec(void) {
    ESP_LOGI(TAG, "初始化ES8311编解码器...");

    vTaskDelay(pdMS_TO_TICKS(100));  // 等待芯片上电稳定

    // 软复位
    ESP_ERROR_CHECK(es8311_write_reg(ES8311_REG_RESET, 0x3F));
    vTaskDelay(pdMS_TO_TICKS(50));

    // 配置时钟
    ESP_ERROR_CHECK(es8311_write_reg(ES8311_REG_CLK, 0x3F));
    ESP_ERROR_CHECK(es8311_write_reg(0x02, 0x00));

    // 配置DAC
    ESP_ERROR_CHECK(es8311_write_reg(ES8311_REG_DAC, 0x00));  // 左右声道同时开启
    ESP_ERROR_CHECK(es8311_write_reg(ES8311_REG_DAC2, 0x00));

    ESP_LOGI(TAG, "✅ ES8311初始化完成");
    return ESP_OK;
}

// ==================== I2S操作 ====================

i2s_chan_handle_t tx_handle;

esp_err_t i2s_init(void) {
    ESP_LOGI(TAG, "初始化I2S...");

    // I2S通道配置
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S新建通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // I2S标准配置
    i2s_std_config_t std_cfg = I2S0_STD_CFG_DEFAULT_CLA_MCK(
        SAMPLE_RATE,
        I2S_BITS_PER_SAMPLE_16BIT,
        I2S_SLOT_MODE_STEREO
    );

    // GPIO配置
    std_cfg_gpio_cfg_t gpio_cfg = {
        .mclk = I2S_MCLK_PIN,
        .bclk = I2S_BCLK_PIN,
        .ws = I2S_LRCK_PIN,
        .dout = I2S_DOUT_PIN,
        .din = I2S_DIN_PIN,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    };

    // 配置I2S
    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg, &gpio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启用I2S通道
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S启用失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2S初始化成功");
    return ESP_OK;
}

// ==================== 播放音频 ====================

esp_err_t play_tone(int frequency, int duration_ms, int volume) {
    const int samples_per_ms = SAMPLE_RATE / 1000;
    const int total_samples = samples_per_ms * duration_ms;
    const int buffer_size = total_samples * sizeof(int16_t);

    int16_t *buffer = (int16_t *)malloc(buffer_size);
    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 生成立体声音频(复制到左右声道)
    generate_sine_wave(buffer, total_samples, frequency, volume);

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle, buffer, buffer_size, &bytes_written, 1000);

    free(buffer);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S写入失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 等待播放完成
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    return ESP_OK;
}

void play_melody(void) {
    ESP_LOGI(TAG, "🎵 播放旋律: 小星星");

    for (int i = 0; i < MELODY_LENGTH; i++) {
        int note = twinkle_star[i].note;
        int duration = twinkle_star[i].duration_ms;

        ESP_LOGI(TAG, "播放音符 %d (频率=%dHz, 时长=%dms)", i+1, note, duration);
        play_tone(note, duration, VOLUME);

        // 音符之间的小停顿
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "✅ 旋律播放完成");
}

// ==================== 主程序 ====================

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("    扬声器测试程序\n");
    printf("    路小班ES8311+NS4150B模块\n");
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "开始初始化硬件...");

    // 初始化I2C
    ESP_ERROR_CHECK(i2c_master_init());

    // 初始化ES8311
    ESP_ERROR_CHECK(es8311_init_codec());

    // 初始化I2S
    ESP_ERROR_CHECK(i2s_init());

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "硬件初始化完成");
    ESP_LOGI(TAG, "准备播放测试音频...");
    ESP_LOGI(TAG, "========================================");

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 测试1: 单个音调
    ESP_LOGI(TAG, "\n📢 测试1: 播放440Hz音调 (A4音) 2秒");
    play_tone(440, 2000, VOLUME);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 测试2: 频率扫描
    ESP_LOGI(TAG, "\n📢 测试2: 频率扫描 (200Hz-2000Hz)");
    for (int freq = 200; freq <= 2000; freq += 100) {
        ESP_LOGI(TAG, "播放 %dHz", freq);
        play_tone(freq, 200, VOLUME / 2);
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    // 测试3: 旋律
    ESP_LOGI(TAG, "\n🎵 测试3: 播放旋律 - 小星星");
    play_melody();

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "所有测试完成!");
    ESP_LOGI(TAG, "========================================");

    // 保持运行
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "系统运行中... (每10秒播放一次提示音)");

        // 每10秒播放一次短提示音
        for (int i = 0; i < 10; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        play_tone(880, 100, VOLUME / 2);  // 提示音
    }
}
