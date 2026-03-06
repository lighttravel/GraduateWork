/**
 * @file audio_manager.h
 * @brief 音频管理器 - 处理录音和播放
 */

#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief 音频状态
 */
typedef enum {
    AUDIO_STATE_IDLE = 0,         // 空闲
    AUDIO_STATE_RECORDING,        // 录音中
    AUDIO_STATE_PLAYING,         // 播放中
    AUDIO_STATE_PROCESSING,       // 处理中
} audio_state_t;

/**
 * @brief 音频事件
 */
typedef enum {
    AUDIO_EVENT_RECORD_START = 0,   // 开始录音
    AUDIO_EVENT_RECORD_STOP,        // 停止录音
    AUDIO_EVENT_PLAY_START,         // 开始播放
    AUDIO_EVENT_PLAY_STOP,          // 停止播放
    AUDIO_EVENT_VOLUME_CHANGED,     // 音量改变
    AUDIO_EVENT_ERROR               // 错误
} audio_event_t;

/**
 * @brief 音频数据回调函数类型
 * @param data 音频数据指针
 * @param len 数据长度
 * @param user_data 用户数据
 */
typedef void (*audio_data_callback_t)(uint8_t *data, size_t len, void *user_data);

/**
 * @brief 音频事件回调函数类型
 * @param event 音频事件
 * @param user_data 用户数据
 */
typedef void (*audio_event_callback_t)(audio_event_t event, void *user_data);

/**
 * @brief 音频配置结构体
 */
typedef struct {
    uint32_t sample_rate;            // 采样率
    uint8_t volume;                 // 音量 (0-100)
    bool vad_enabled;               // 是否启用VAD
    uint16_t vad_threshold;         // VAD阈值
    audio_data_callback_t data_cb;  // 数据回调
    audio_event_callback_t event_cb; // 事件回调
    void *user_data;                // 用户数据
} audio_config_t;

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化音频管理器
 * @param config 音频配置
 * @return ESP_OK成功
 */
esp_err_t audio_manager_init(const audio_config_t *config);

/**
 * @brief 反初始化音频管理器
 * @return ESP_OK成功
 */
esp_err_t audio_manager_deinit(void);

// ==================== 录音控制 ====================

/**
 * @brief 开始录音
 * @return ESP_OK成功
 */
esp_err_t audio_manager_start_record(void);

/**
 * @brief 停止录音
 * @return ESP_OK成功
 */
esp_err_t audio_manager_stop_record(void);

/**
 * @brief 暂停录音
 * @return ESP_OK成功
 */
esp_err_t audio_manager_pause_record(void);

/**
 * @brief 恢复录音
 * @return ESP_OK成功
 */
esp_err_t audio_manager_resume_record(void);

// ==================== 播放控制 ====================

/**
 * @brief 开始播放音频数据
 * @param data 音频数据
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t audio_manager_start_play(const uint8_t *data, size_t len);

/**
 * @brief 停止播放
 * @return ESP_OK成功
 */
esp_err_t audio_manager_stop_play(void);

/**
 * @brief 暂停播放
 * @return ESP_OK成功
 */
esp_err_t audio_manager_pause_play(void);

/**
 * @brief 恢复播放
 * @return ESP_OK成功
 */
esp_err_t audio_manager_resume_play(void);

// ==================== TTS音频播放接口 ====================

/**
 * @brief 启动TTS播放模式
 * @return ESP_OK成功
 */
esp_err_t audio_manager_start_tts_playback(void);

/**
 * @brief 停止TTS播放
 * @return ESP_OK成功
 */
esp_err_t audio_manager_stop_tts_playback(void);

/**
 * @brief 启动播放（不带参数版本，用于TTS模块）
 */
esp_err_t audio_manager_start_playback(void);

/**
 * @brief 停止播放（不带参数版本，用于TTS模块）
 */
esp_err_t audio_manager_stop_playback(void);

/**
 * @brief 播放TTS音频数据（流式播放）
 * @param data TTS音频数据(PCM 16-bit mono, 16kHz或24kHz)
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t audio_manager_play_tts_audio(const uint8_t *data, size_t len);

// ==================== 音量控制 ====================

/**
 * @brief 设置音量
 * @param volume 音量 (0-100)
 * @return ESP_OK成功
 */
esp_err_t audio_manager_set_volume(uint8_t volume);

/**
 * @brief 获取当前音量
 * @return 音量值 (0-100)
 */
uint8_t audio_manager_get_volume(void);

// ==================== 状态查询 ====================

/**
 * @brief 获取音频状态
 * @return 当前状态
 */
audio_state_t audio_manager_get_state(void);

/**
 * @brief 是否正在录音
 * @return true录音中
 */
bool audio_manager_is_recording(void);

/**
 * @brief 是否正在播放
 * @return true播放中
 */
bool audio_manager_is_playing(void);

// ==================== VAD控制 ====================

/**
 * @brief 启用/禁用VAD
 * @param enabled true启用
 * @return ESP_OK成功
 */
esp_err_t audio_manager_set_vad(bool enabled);

/**
 * @brief 设置VAD阈值
 * @param threshold VAD阈值
 * @return ESP_OK成功
 */
esp_err_t audio_manager_set_vad_threshold(uint16_t threshold);

// ==================== 诊断功能 ====================

/**
 * @brief 打印ES8311寄存器诊断信息
 */
void audio_manager_dump_codec_registers(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MANAGER_H
