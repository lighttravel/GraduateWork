/**
 * @file asr_module.h
 * @brief ASR语音识别模块 - Deepgram API
 */

#ifndef ASR_MODULE_H
#define ASR_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief ASR状态
 */
typedef enum {
    ASR_STATE_IDLE = 0,        // 空闲
    ASR_STATE_CONNECTING,      // 连接中
    ASR_STATE_CONNECTED,       // 已连接
    ASR_STATE_LISTENING,       // 监听中
    ASR_STATE_PROCESSING,      // 处理中
    ASR_STATE_ERROR            // 错误
} asr_state_t;

/**
 * @brief ASR事件
 */
typedef enum {
    ASR_EVENT_CONNECTED = 0,       // 已连接
    ASR_EVENT_DISCONNECTED,        // 已断开
    ASR_EVENT_SPEECH_STARTED,      // 检测到语音开始
    ASR_EVENT_SPEECH_ENDED,        // 语音结束
    ASR_EVENT_TRANSCRIPT,          // 收到转录结果
    ASR_EVENT_ERROR                // 错误
} asr_event_t;

/**
 * @brief ASR识别结果
 */
typedef struct {
    char text[512];            // 识别文本
    bool is_final;             // 是否为最终结果
    float confidence;          // 置信度
} asr_result_t;

/**
 * @brief ASR事件回调
 * @param event ASR事件
 * @param result 识别结果(仅ASR_EVENT_TRANSCRIPT时有值)
 * @param user_data 用户数据
 */
typedef void (*asr_event_callback_t)(asr_event_t event, const asr_result_t *result, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化ASR模块
 * @param api_key Deepgram API密钥
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t asr_module_init(const char *api_key, asr_event_callback_t event_cb, void *user_data);

/**
 * @brief 反初始化ASR模块
 * @return ESP_OK成功
 */
esp_err_t asr_module_deinit(void);

// ==================== 识别控制 ====================

/**
 * @brief 开始语音识别
 * @return ESP_OK成功
 */
esp_err_t asr_module_start(void);

/**
 * @brief 停止语音识别
 * @return ESP_OK成功
 */
esp_err_t asr_module_stop(void);

/**
 * @brief 发送音频数据
 * @param data 音频数据(PCM 16-bit mono)
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t asr_module_send_audio(const uint8_t *data, size_t len);

// ==================== 状态查询 ====================

/**
 * @brief 获取ASR状态
 * @return ASR状态
 */
asr_state_t asr_module_get_state(void);

/**
 * @brief 是否正在识别
 * @return true识别中
 */
bool asr_module_is_listening(void);

// ==================== 配置 ====================

/**
 * @brief 设置语言
 * @param language 语言代码(zh-CN, en-US等)
 * @return ESP_OK成功
 */
esp_err_t asr_module_set_language(const char *language);

/**
 * @brief 启用/禁用标点符号
 * @param enable true启用
 * @return ESP_OK成功
 */
esp_err_t asr_module_set_punctuate(bool enable);

/**
 * @brief 启用/禁用智能格式化
 * @param enable true启用
 * @return ESP_OK成功
 */
esp_err_t asr_module_set_smart_format(bool enable);

#ifdef __cplusplus
}
#endif

#endif // ASR_MODULE_H
