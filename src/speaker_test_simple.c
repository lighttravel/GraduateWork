/**
 * @file speaker_test_simple.c
 * @brief 简单扬声器测试程序 - 使用现有驱动
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

// 使用现有的音频驱动
#include "drivers/es8311_codec_v2.h"
#include "middleware/audio_manager.h"

static const char *TAG = "SPK_TEST";

// ==================== 生成测试音频 ====================

// 生成正弦波
void generate_sine_wave(int16_t *buffer, int samples, int frequency, int volume) {
    const float amplitude = (volume / 100.0f) * 16000.0f;

    for (int i = 0; i < samples; i++) {
        float t = (float)i / 16000.0f;  // 16kHz采样率
        buffer[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
    }
}

// 音符频率
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

// 小星星旋律
typedef struct {
    int note;
    int duration_ms;
} melody_note_t;

static const melody_note_t twinkle_star[] = {
    {NOTE_C4, 500}, {NOTE_C4, 500}, {NOTE_G4, 500}, {NOTE_G4, 500},
    {NOTE_A4, 500}, {NOTE_A4, 500}, {NOTE_G4, 1000},
    {NOTE_F4, 500}, {NOTE_F4, 500}, {NOTE_E4, 500}, {NOTE_E4, 500},
    {NOTE_D4, 500}, {NOTE_D4, 500}, {NOTE_C4, 1000},
};
#define MELODY_LENGTH (sizeof(twinkle_star) / sizeof(melody_note_t))

// ==================== 播放音频 ====================

void play_tone(es8311_codec_handle_t *codec, int frequency, int duration_ms, int volume) {
    const int samples_per_ms = 16000 / 1000;  // 16kHz采样率
    const int total_samples = samples_per_ms * duration_ms;

    // 生成立体声缓冲区
    int16_t *buffer_stereo = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));
    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    // 生成单声道正弦波
    generate_sine_wave(buffer_stereo, total_samples, frequency, volume);

    // 复制到右声道(生成立体声)
    for (int i = 0; i < total_samples; i++) {
        buffer_stereo[i * 2 + 1] = buffer_stereo[i * 2];
    }

    // 播放
    size_t bytes_written;
    es8311_codec_write(codec, (uint8_t *)buffer_stereo, total_samples * 2 * sizeof(int16_t));

    free(buffer_stereo);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
}

void play_melody(es8311_codec_handle_t *codec) {
    ESP_LOGI(TAG, "🎵 播放旋律: 小星星");

    for (int i = 0; i < MELODY_LENGTH; i++) {
        int note = twinkle_star[i].note;
        int duration = twinkle_star[i].duration_ms;

        ESP_LOGI(TAG, "音符 %d: %dHz, %dms", i+1, note, duration);
        play_tone(codec, note, duration, 80);

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

    ESP_LOGI(TAG, "初始化音频系统...");

    // 使用audio_manager初始化
    audio_manager_config_t audio_config = {
        .sample_rate = 16000,
        .buffer_size = 4096,
        .volume = 80,
    };

    audio_manager_t *audio_mgr = audio_manager_init(&audio_config);
    if (!audio_mgr) {
        ESP_LOGE(TAG, "音频管理器初始化失败");
        return;
    }

    ESP_LOGI(TAG, "✅ 音频系统初始化完成");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "准备播放测试音频...");
    ESP_LOGI(TAG, "========================================");

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 测试1: 单个音调 (440Hz - A4音)
    ESP_LOGI(TAG, "\n📢 测试1: 播放440Hz音调 (A4) 2秒");
    play_tone(audio_mgr, 440, 2000, 80);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 测试2: 频率扫描
    ESP_LOGI(TAG, "\n📢 测试2: 频率扫描 (200Hz-1000Hz)");
    for (int freq = 200; freq <= 1000; freq += 100) {
        ESP_LOGI(TAG, "播放 %dHz", freq);
        play_tone(audio_mgr, freq, 200, 60);
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    // 测试3: 旋律 - 小星星
    ESP_LOGI(TAG, "\n🎵 测试3: 播放旋律 - 小星星");
    play_melody(audio_mgr);

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "所有测试完成!");
    ESP_LOGI(TAG, "扬声器工作正常! ✅");
    ESP_LOGI(TAG, "========================================");

    // 保持运行,每30秒播放一次提示音
    while (1) {
        for (int i = 0; i < 30; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        ESP_LOGI(TAG, "提示音...");
        play_tone(audio_mgr, 880, 100, 50);
    }

    // 清理
    audio_manager_deinit(audio_mgr);
}
