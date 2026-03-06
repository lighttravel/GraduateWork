/**
 * @file ws_server.h
 * @brief WebSocket服务器 - 实时通信
 */

#ifndef WS_SERVER_H
#define WS_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief WebSocket事件类型
 */
typedef enum {
    WS_EVENT_CONNECTED = 0,    // 客户端连接
    WS_EVENT_DISCONNECTED,     // 客户端断开
    WS_EVENT_DATA,             // 接收到数据
    WS_EVENT_ERROR             // 错误
} ws_event_type_t;

/**
 * @brief WebSocket事件
 */
typedef struct {
    ws_event_type_t type;      // 事件类型
    int fd;                    // 客户端文件描述符
    void *data;                // 数据指针
    size_t len;                // 数据长度
    void *user_data;           // 用户数据
} ws_event_t;

/**
 * @brief WebSocket事件回调
 * @param event WebSocket事件
 */
typedef void (*ws_event_callback_t)(ws_event_t *event);

// ==================== 初始化和配置 ====================

/**
 * @brief 启动WebSocket服务器
 * @param port 端口号(0=使用默认端口81)
 * @param event_cb 事件回调函数
 * @return ESP_OK成功
 */
esp_err_t ws_server_start(uint16_t port, ws_event_callback_t event_cb);

/**
 * @brief 停止WebSocket服务器
 * @return ESP_OK成功
 */
esp_err_t ws_server_stop(void);

// ==================== 数据发送 ====================

/**
 * @brief 发送消息到指定客户端
 * @param fd 客户端文件描述符
 * @param data 数据
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t ws_server_send(int fd, const char *data, size_t len);

/**
 * @brief 广播消息到所有客户端
 * @param data 数据
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t ws_server_broadcast(const char *data, size_t len);

// ==================== 状态查询 ====================

/**
 * @brief 获取连接的客户端数量
 * @return 客户端数量
 */
int ws_server_get_client_count(void);

/**
 * @brief 服务器是否运行
 * @return true运行中
 */
bool ws_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // WS_SERVER_H
