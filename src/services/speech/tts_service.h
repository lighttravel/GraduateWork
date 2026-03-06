/**
 * @file tts_module.h
 * @brief TTS语音合成模块
 */

#ifndef TTS_MODULE_H
#define TTS_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief TTS提供商
 */
typedef enum {
    TTS_PROVIDER_DEEPGRAM = 0,    // Deepgram TTS (仅英文)
    TTS_PROVIDER_INDEX_TTS = 1,   // Index-TTS (自部署)
    TTS_PROVIDER_IFLYTEK = 2      // 科大讯飞TTS (中文)
} tts_provider_t;

/**
 * @brief TTS状态
 */
typedef enum {
    TTS_STATE_IDLE = 0,       // 空闲
    TTS_STATE_DOWNLOADING,    // 下载中
    TTS_STATE_PLAYING,        // 播放中
    TTS_STATE_ERROR           // 错误
} tts_state_t;

/**
 * @brief TTS事件
 */
typedef enum {
    TTS_EVENT_START = 0,      // 开始合成
    TTS_EVENT_DATA,           // 收到音频数据
    TTS_EVENT_DONE,           // 合成完成
    TTS_EVENT_ERROR           // 错误
} tts_event_t;

/**
 * @brief TTS音频数据回调
 * @param data 音频数据
 * @param len 数据长度
 * @param user_data 用户数据
 */
typedef void (*tts_data_callback_t)(const uint8_t *data, size_t len, void *user_data);

/**
 * @brief TTS事件回调
 * @param event TTS事件
 * @param user_data 用户数据
 */
typedef void (*tts_event_callback_t)(tts_event_t event, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化TTS模块
 * @param provider TTS提供商
 * @return ESP_OK成功
 */
esp_err_t tts_module_init(tts_provider_t provider);

/**
 * @brief 反初始化TTS模块
 * @return ESP_OK成功
 */
esp_err_t tts_module_deinit(void);

// ==================== 语音合成 ====================

/**
 * @brief 文本转语音
 * @param text 要转换的文本
 * @param data_cb 数据回调
 * @param event_cb 事件回调
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t tts_module_speak(const char *text,
                            tts_data_callback_t data_cb,
                            tts_event_callback_t event_cb,
                            void *user_data);

/**
 * @brief 停止合成
 * @return ESP_OK成功
 */
esp_err_t tts_module_stop(void);

// ==================== 配置 ====================

/**
 * @brief 设置Deepgram API密钥
 * @param api_key API密钥
 * @return ESP_OK成功
 */
esp_err_t tts_module_set_deepgram_key(const char *api_key);

/**
 * @brief 设置Index-TTS URL
 * @param url TTS服务URL
 * @return ESP_OK成功
 */
esp_err_t tts_module_set_index_tts_url(const char *url);

/**
 * @brief 设置TTS提供商
 * @param provider 提供商
 * @return ESP_OK成功
 */
esp_err_t tts_module_set_provider(tts_provider_t provider);

/**
 * @brief 设置讯飞TTS配置
 * @param appid APPID
 * @param api_key API Key
 * @param api_secret API Secret
 * @param voice 发音人名称 (如 x4_yezi)
 * @return ESP_OK成功
 */
esp_err_t tts_module_set_iflytek_config(const char *appid, const char *api_key,
                                         const char *api_secret, const char *voice);

/**
 * @brief 使用讯飞TTS合成语音
 * @param text 要转换的文本
 * @return ESP_OK成功
 */
esp_err_t tts_module_speak_iflytek(const char *text);

// ==================== 状态查询 ====================

/**
 * @brief 获取TTS状态
 * @return TTS状态
 */
tts_state_t tts_module_get_state(void);

/**
 * @brief 是否正在合成
 * @return true合成中
 */
bool tts_module_is_speaking(void);

#ifdef __cplusplus
}
#endif

#endif // TTS_MODULE_H
