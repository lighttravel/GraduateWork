/**
 * @file xiaozhi_client.h
 * @brief 小智AI客户端 - HTTP通信到xiaozhi-esp32-server
 *
 * 使用HTTP POST代替WebSocket，简化实现并提高可靠性
 */

#ifndef XIAOZHI_CLIENT_H
#define XIAOZHI_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief 小智客户端状态
 */
typedef enum {
    XIAOZHI_STATE_DISCONNECTED = 0,  // 未连接
    XIAOZHI_STATE_CONNECTING,          // 连接中
    XIAOZHI_STATE_CONNECTED,           // 已连接
    XIAOZHI_STATE_AUTHENTICATED,       // 已认证
    XIAOZHI_STATE_ERROR                // 错误
} xiaozhi_state_t;

/**
 * @brief 小智客户端事件
 */
typedef enum {
    XIAOZHI_EVENT_CONNECTED = 0,         // 连接成功
    XIAOZHI_EVENT_DISCONNECTED,          // 断开连接
    XIAOZHI_EVENT_AUTHENTICATED,         // 认证成功
    XIAOZHI_EVENT_TEXT_RECEIVED,         // 收到文本消息（ASR结果或LLM回复）
    XIAOZHI_EVENT_AUDIO_RECEIVED,         // 收到TTS音频数据
    XIAOZHI_EVENT_ERROR                  // 错误
} xiaozhi_event_t;

/**
 * @brief TTS音频数据
 */
typedef struct {
    uint8_t *data;               // 音频数据
    size_t len;                  // 数据长度
    bool is_end;                 // 是否为音频结束
} xiaozhi_audio_data_t;

/**
 * @brief 文本消息数据
 */
typedef struct {
    char text[512];             // 文本内容
    bool is_final;              // 是否为最终结果
} xiaozhi_text_data_t;

/**
 * @brief 小智客户端配置
 */
typedef struct {
    char server_url[256];         // 服务器URL (http://或https://)
    char device_id[64];           // 设备ID
    int reconnect_interval;        // 重连间隔(秒)
    bool auto_reconnect;          // 是否自动重连
} xiaozhi_config_t;

/**
 * @brief 小智客户端事件回调函数
 * @param event 事件类型
 * @param data 数据(根据事件类型不同)
 * @param user_data 用户数据
 */
typedef void (*xiaozhi_event_callback_t)(xiaozhi_event_t event, const void *data, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化小智客户端
 * @param config 客户端配置
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_init(const xiaozhi_config_t *config, xiaozhi_event_callback_t event_cb, void *user_data);

/**
 * @brief 反初始化小智客户端
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_deinit(void);

// ==================== 连接控制 ====================

/**
 * @brief 连接到服务器
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_connect(void);

/**
 * @brief 断开连接
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_disconnect(void);

/**
 * @brief 是否已连接
 * @return true已连接
 */
bool xiaozhi_is_connected(void);

// ==================== 消息发送 ====================

/**
 * @brief 发送hello消息（认证）
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_send_hello(void);

/**
 * @brief 发送listen消息（控制语音识别）
 * @param state "start" 或 "stop"
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_send_listen(const char *state);

/**
 * @brief 发送文本消息（聊天内容或ASR结果）
 * @param text 文本内容
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_send_text(const char *text);

/**
 * @brief 发送TTS音频数据
 * @param data 音频数据(PCM 16-bit mono)
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_send_audio(const uint8_t *data, size_t len);

// ==================== 状态查询 ====================

/**
 * @brief 获取客户端状态
 * @return 客户端状态
 */
xiaozhi_state_t xiaozhi_get_state(void);

// ==================== 配置 ====================

/**
 * @brief 设置服务器URL
 * @param url 服务器URL
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_set_server_url(const char *url);

/**
 * @brief 设置设备ID
 * @param device_id 设备ID
 * @return ESP_OK成功
 */
esp_err_t xiaozhi_set_device_id(const char *device_id);

#ifdef __cplusplus
}
#endif

#endif // XIAOZHI_CLIENT_H
