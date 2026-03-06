/**
 * @file chat_module.h
 * @brief Chat对话模块 - DeepSeek API
 */

#ifndef CHAT_SERVICE_H
#define CHAT_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief 聊天消息角色
 */
typedef enum {
    CHAT_ROLE_SYSTEM = 0,      // 系统消息
    CHAT_ROLE_USER,            // 用户消息
    CHAT_ROLE_ASSISTANT        // 助手回复
} chat_role_t;

/**
 * @brief 聊天消息
 */
typedef struct {
    chat_role_t role;          // 角色
    char content[MAX_MESSAGE_LEN];  // 消息内容
} chat_message_t;

/**
 * @brief Chat事件
 */
typedef enum {
    CHAT_EVENT_START = 0,      // 开始生成
    CHAT_EVENT_DATA,           // 收到数据
    CHAT_EVENT_DONE,           // 生成完成
    CHAT_EVENT_ERROR           // 错误
} chat_event_t;

/**
 * @brief Chat事件回调
 * @param event Chat事件
 * @param data 响应数据(CHAT_EVENT_DATA时有值)
 * @param is_done 是否完成
 * @param user_data 用户数据
 */
typedef void (*chat_event_callback_t)(chat_event_t event, const char *data, bool is_done, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化Chat模块
 * @param api_key DeepSeek API密钥
 * @return ESP_OK成功
 */
esp_err_t chat_module_init(const char *api_key);

/**
 * @brief 反初始化Chat模块
 * @return ESP_OK成功
 */
esp_err_t chat_module_deinit(void);

// ==================== 对话管理 ====================

/**
 * @brief 发送消息并获取回复
 * @param message 用户消息
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t chat_module_send_message(const char *message, chat_event_callback_t event_cb, void *user_data);

/**
 * @brief 添加系统提示词
 * @param prompt 系统提示词
 * @return ESP_OK成功
 */
esp_err_t chat_module_set_system_prompt(const char *prompt);

// ==================== 历史记录 ====================

/**
 * @brief 清空对话历史
 * @return ESP_OK成功
 */
esp_err_t chat_module_clear_history(void);

/**
 * @brief 获取历史记录条数
 * @return 历史记录条数
 */
int chat_module_get_history_count(void);

/**
 * @brief 获取最后一次AI回复
 * @param buffer 输出缓冲区
 * @param buf_len 缓冲区长度
 * @return ESP_OK成功
 */
esp_err_t chat_module_get_last_response(char *buffer, size_t buf_len);

/**
 * @brief 设置temperature参数
 * @param temperature 温度值(0.0-2.0)
 * @return ESP_OK成功, ESP_ERR_NOT_SUPPORTED暂不支持
 */
esp_err_t chat_module_set_temperature(float temperature);

#ifdef __cplusplus
}
#endif

#endif // CHAT_SERVICE_H
