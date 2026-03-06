/**
 * @file http_server.h
 * @brief HTTP服务器 - 提供Web配置界面和API接口
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief HTTP请求处理函数
 * @param url 请求URL
 * @param body 请求体
 * @param body_len 请求体长度
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
typedef esp_err_t (*http_handler_t)(const char *url, const char *body, size_t body_len, void *user_data);

/**
 * @brief WebSocket数据接收回调
 * @param data 数据指针
 * @param len 数据长度
 * @param user_data 用户数据
 */
typedef void (*ws_data_callback_t)(const char *data, size_t len, void *user_data);

/**
 * @brief WebSocket事件回调 (HTTP服务器简化版)
 * @param connected true连接，false断开
 * @param user_data 用户数据
 */
typedef void (*http_ws_event_callback_t)(bool connected, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 启动HTTP服务器
 * @return ESP_OK成功
 */
esp_err_t http_server_start(void);

/**
 * @brief 停止HTTP服务器
 * @return ESP_OK成功
 */
esp_err_t http_server_stop(void);

/**
 * @brief 判断服务器是否运行
 * @return true运行中
 */
bool http_server_is_running(void);

// ==================== WebSocket通信 ====================

/**
 * @brief 广播消息到所有WebSocket客户端
 * @param data 数据
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t http_server_ws_broadcast(const char *data, size_t len);

/**
 * @brief 发送消息到指定WebSocket客户端
 * @param fd 客户端文件描述符
 * @param data 数据
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t http_server_ws_send(int fd, const char *data, size_t len);

/**
 * @brief 设置WebSocket数据回调
 * @param callback 回调函数
 * @return ESP_OK成功
 */
esp_err_t http_server_ws_set_data_callback(ws_data_callback_t callback);

/**
 * @brief 设置WebSocket事件回调
 * @param callback 回调函数
 * @return ESP_OK成功
 */
esp_err_t http_server_ws_set_event_callback(http_ws_event_callback_t callback);

// ==================== 辅助功能 ====================

/**
 * @brief 获取服务器URL
 * @param url URL缓冲区
 * @param url_size URL缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t http_server_get_url(char *url, size_t url_size);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H
