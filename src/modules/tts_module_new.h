/**
 * @file tts_module_new.h
 * @brief TTS语音合成模块 - 小智AI方案（接收服务器音频流）
 *
 * 功能：
 * - 通过xiaozhi_client接收TTS音频流
 * - 将音频数据传递给audio_manager进行播放
 * - 管理播放状态
 */

#ifndef TTS_MODULE_NEW_H
#define TTS_MODULE_NEW_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief TTS状态
 */
typedef enum {
    TTS_STATE_IDLE = 0,         // 空闲
    TTS_STATE_RECEIVING,       // 接收音频中
    TTS_STATE_PLAYING,         // 播放中
    TTS_STATE_ERROR           // 错误
} tts_state_t;

/**
 * @brief TTS事件
 */
typedef enum {
    TTS_EVENT_START = 0,         // 开始接收/播放
    TTS_EVENT_DATA,             // 收到音频数据
    TTS_EVENT_END,              // 音频结束
    TTS_EVENT_ERROR             // 错误
} tts_event_t;

/**
 * @brief TTS事件回调
 * @param event TTS事件
 * @param data 音频数据(仅TTS_EVENT_DATA时有值)
 * @param len 数据长度
 * @param user_data 用户数据
 */
typedef void (*tts_event_callback_t)(tts_event_t event, const uint8_t *data, size_t len, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化TTS模块（小智AI方案）
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 *
 * 注意：此模块不直接发送TTS请求
 *       TTS由服务器端处理，音频通过WebSocket发送
 */
esp_err_t tts_module_init(tts_event_callback_t event_cb, void *user_data);

/**
 * @brief 反初始化TTS模块
 * @return ESP_OK成功
 */
esp_err_t tts_module_deinit(void);

// ==================== 音频接收接口 ====================

/**
 * @brief 处理接收到的TTS音频数据（由xiaozhi_client调用）
 * @param data 音频数据
 * @param len 数据长度
 * @param is_end 是否为音频结束
 * @return ESP_OK成功
 */
esp_err_t tts_module_process_audio(const uint8_t *data, size_t len, bool is_end);

// ==================== 播放控制 ====================

/**
 * @brief 开始播放TTS音频
 * @return ESP_OK成功
 */
esp_err_t tts_module_start_playback(void);

/**
 * @brief 停止播放TTS音频
 * @return ESP_OK成功
 */
esp_err_t tts_module_stop_playback(void);

// ==================== 状态查询 ====================

/**
 * @brief 获取TTS状态
 * @return TTS状态
 */
tts_state_t tts_module_get_state(void);

/**
 * @brief 是否正在播放
 * @return true播放中
 */
bool tts_module_is_playing(void);

/**
 * @brief 是否正在接收音频
 * @return true接收中
 */
bool tts_module_is_receiving(void);

#ifdef __cplusplus
}
#endif

#endif // TTS_MODULE_NEW_H
