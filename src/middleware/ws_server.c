/**
 * @file ws_server.c
 * @brief WebSocket服务器实现 - 使用ESP-IDF HTTP Server WebSocket
 */

#include "ws_server.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_server.h"

static const char *TAG = "WS_SRVR";

// WebSocket客户端最大数量
#define MAX_WS_CLIENTS  4

// WebSocket客户端信息
typedef struct {
    int fd;                         // 文件描述符
    httpd_handle_t hd;               // HTTPD实例
    httpd_req_t *hd_req;            // 请求句柄
} ws_client_t;

// WebSocket服务器上下文
typedef struct {
    bool running;                   // 运行状态
    uint16_t port;                  // 端口号
    ws_event_callback_t event_cb;     // 事件回调

    // HTTP服务器
    httpd_handle_t httpd;
    httpd_config_t httpd_config;

    // 客户端管理
    ws_client_t clients[MAX_WS_CLIENTS];
    int client_count;

    // 互斥锁
    SemaphoreHandle_t mutex;
} ws_server_t;

static ws_server_t *g_ws_server = NULL;

// ==================== WebSocket处理器 ====================

/**
 * @brief WebSocket接收处理器
 */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket握手请求");

        // 设置WebSocket握手头
        httpd_handle_t hd = req->handle;
        struct sockaddr_in6 addr;
        httpd_ws_get_frame_info(req, NULL, &addr);

        // 响应WebSocket升级请求
        httpd_ws_frame_t ws_pkt = {
            .final = true,
            .fragmented = false,
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = NULL,
            .len = 0,
        };

        // 通知客户端连接
        if (g_ws_server && g_ws_server->event_cb) {
            ws_event_t event = {
                .type = WS_EVENT_CONNECTED,
                .fd = httpd_req_to_sockfd(req),
                .data = NULL,
                .len = 0
            };
            g_ws_server->event_cb(&event);
        }

        return ESP_OK;
    }

    // 接收WebSocket帧
    uint8_t buf[128] = {0};
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.len = sizeof(buf);

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame失败: %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        // 通知数据接收
        if (g_ws_server && g_ws_server->event_cb) {
            ws_event_t event = {
                .type = WS_EVENT_DATA,
                .fd = httpd_req_to_sockfd(req),
                .data = ws_pkt.payload,
                .len = ws_pkt.len
            };
            g_ws_server->event_cb(&event);
        }
    }

    // 处理关闭帧
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket关闭");
        // 通知断开
        if (g_ws_server && g_ws_server->event_cb) {
            ws_event_t event = {
                .type = WS_EVENT_DISCONNECTED,
                .fd = httpd_req_to_sockfd(req),
                .data = NULL,
                .len = 0
            };
            g_ws_server->event_cb(&event);
        }
        return ESP_OK;
    }

    return ESP_OK;
}

// ==================== 辅助函数 ====================

/**
 * @brief 查找客户端索引
 */
static int find_client_index(int fd)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (g_ws_server->clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 分配客户端槽位
 */
static int allocate_client_slot(void)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (g_ws_server->clients[i].fd < 0) {
            return i;
        }
    }
    return -1;
}

// ==================== 初始化和配置 ====================

esp_err_t ws_server_start(uint16_t port, ws_event_callback_t event_cb)
{
    if (g_ws_server != NULL) {
        ESP_LOGW(TAG, "WebSocket服务器已在运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "启动WebSocket服务器，端口: %d", port ? port : WS_SERVER_PORT);

    // 分配内存
    g_ws_server = (ws_server_t *)calloc(1, sizeof(ws_server_t));
    if (g_ws_server == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 初始化
    g_ws_server->running = true;
    g_ws_server->port = port ? port : WS_SERVER_PORT;
    g_ws_server->event_cb = event_cb;
    g_ws_server->client_count = 0;

    // 初始化客户端列表
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        g_ws_server->clients[i].fd = -1;
        g_ws_server->clients[i].hd = NULL;
        g_ws_server->clients[i].hd_req = NULL;
    }

    // 创建互斥锁
    g_ws_server->mutex = xSemaphoreCreateMutex();
    if (g_ws_server->mutex == NULL) {
        ESP_LOGE(TAG, "互斥锁创建失败");
        free(g_ws_server);
        g_ws_server = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 配置HTTP服务器
    g_ws_server->httpd_config = HTTPD_DEFAULT_CONFIG();
    g_ws_server->httpd_config.server_port = g_ws_server->port;
    g_ws_server->httpd_config.ws_close_on_error = true;

    // 启动HTTP服务器
    esp_err_t ret = httpd_start(&g_ws_server->httpd, &g_ws_server->httpd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_ws_server->mutex);
        free(g_ws_server);
        g_ws_server = NULL;
        return ret;
    }

    // 注册WebSocket URI处理器
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };

    ret = httpd_register_uri_handler(g_ws_server->httpd, &ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket URI注册失败: %s", esp_err_to_name(ret));
        httpd_stop(g_ws_server->httpd);
        vSemaphoreDelete(g_ws_server->mutex);
        free(g_ws_server);
        g_ws_server = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "WebSocket服务器启动成功 (端口 %d)", g_ws_server->port);
    return ESP_OK;
}

esp_err_t ws_server_stop(void)
{
    if (g_ws_server == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止WebSocket服务器");

    xSemaphoreTake(g_ws_server->mutex, portMAX_DELAY);

    // 停止HTTP服务器
    if (g_ws_server->httpd) {
        httpd_stop(g_ws_server->httpd);
        g_ws_server->httpd = NULL;
    }

    // 清空客户端列表
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        g_ws_server->clients[i].fd = -1;
        g_ws_server->clients[i].hd = NULL;
        g_ws_server->clients[i].hd_req = NULL;
    }

    g_ws_server->running = false;
    g_ws_server->client_count = 0;

    xSemaphoreGive(g_ws_server->mutex);

    // 删除互斥锁
    vSemaphoreDelete(g_ws_server->mutex);
    g_ws_server->mutex = NULL;

    // 释放内存
    free(g_ws_server);
    g_ws_server = NULL;

    ESP_LOGI(TAG, "WebSocket服务器已停止");
    return ESP_OK;
}

// ==================== 数据发送 ====================

esp_err_t ws_server_send(int fd, const char *data, size_t len)
{
    if (g_ws_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_ws_server->mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "发送到客户端 %d: %.*s", fd, (int)len, data);

    // 注意：这里需要保存httpd_req_t指针来实现发送
    // 实际使用中需要在连接时保存req句柄
    // TODO: 实现实际的帧发送

    xSemaphoreGive(g_ws_server->mutex);
    return ESP_OK;
}

esp_err_t ws_server_broadcast(const char *data, size_t len)
{
    if (g_ws_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_ws_server->mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "广播消息到 %d 个客户端", g_ws_server->client_count);

    // 发送到所有连接的客户端
    // TODO: 实现实际的广播帧发送

    xSemaphoreGive(g_ws_server->mutex);
    return ESP_OK;
}

// ==================== 状态查询 ====================

int ws_server_get_client_count(void)
{
    if (g_ws_server == NULL) {
        return 0;
    }

    xSemaphoreTake(g_ws_server->mutex, portMAX_DELAY);
    int count = g_ws_server->client_count;
    xSemaphoreGive(g_ws_server->mutex);

    return count;
}

bool ws_server_is_running(void)
{
    return g_ws_server ? g_ws_server->running : false;
}
