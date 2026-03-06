/**
 * @file xiaozhi_client.c
 * @brief 小智AI客户端 - HTTP通信到xiaozhi-esp32-server
 *
 * 使用HTTP POST代替WebSocket，简化实现并提高可靠性
 */

#include "xiaozhi_client.h"
#include "config.h"
#include "esp_log.h"
#include "esp_http_client.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "XIAOZHI_CLIENT";

// ==================== 客户端上下文 ====================

typedef struct {
    xiaozhi_config_t config;
    xiaozhi_event_callback_t event_cb;
    void *user_data;

    // HTTP客户端
    esp_http_client_handle_t http_client;

    // 状态
    xiaozhi_state_t state;

    // 消息ID
    uint32_t msg_id;

} xiaozhi_client_t;

static xiaozhi_client_t *g_xiaozhi = NULL;

// ==================== HTTP事件处理 ====================

/**
 * @brief HTTP事件处理器
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP请求完成");

            // 触发连接成功回调
            if (g_xiaozhi && g_xiaozhi->state == XIAOZHI_STATE_CONNECTING) {
                g_xiaozhi->state = XIAOZHI_STATE_AUTHENTICATED;
                if (g_xiaozhi->event_cb) {
                    g_xiaozhi->event_cb(XIAOZHI_EVENT_AUTHENTICATED, NULL, g_xiaozhi->user_data);
                }
            }
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "HTTP连接断开");
            g_xiaozhi->state = XIAOZHI_STATE_DISCONNECTED;
            if (g_xiaozhi->event_cb) {
                g_xiaozhi->event_cb(XIAOZHI_EVENT_DISCONNECTED, NULL, g_xiaozhi->user_data);
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

// ==================== 辅助函数 ====================

/**
 * @brief 设置HTTP客户端配置
 */
static void setup_http_client_config(esp_http_client_config_t *config)
{
    config->url = g_xiaozhi->config.server_url;
    config->method = HTTP_METHOD_POST;
    config->timeout_ms = 5000;
    config->buffer_size = 4096;
    config->user_agent = "ESP32-XiaoZhi-Client/1.0";
    config->event_handler = http_event_handler;
}

// ==================== 初始化和配置 ====================

esp_err_t xiaozhi_init(const xiaozhi_config_t *config, xiaozhi_event_callback_t event_cb, void *user_data)
{
    if (g_xiaozhi != NULL) {
        ESP_LOGW(TAG, "小智客户端已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化小智AI客户端");

    // 分配内存
    g_xiaozhi = (xiaozhi_client_t *)calloc(1, sizeof(xiaozhi_client_t));
    if (g_xiaozhi == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    if (config) {
        memcpy(&g_xiaozhi->config, config, sizeof(xiaozhi_config_t));
    } else {
        // 默认配置
        strncpy(g_xiaozhi->config.server_url, XIAOZHI_SERVER_URL, sizeof(g_xiaozhi->config.server_url) - 1);
        strncpy(g_xiaozhi->config.device_id, XIAOZHI_DEVICE_ID, sizeof(g_xiaozhi->config.device_id) - 1);
        g_xiaozhi->config.reconnect_interval = 5;
        g_xiaozhi->config.auto_reconnect = true;
    }

    g_xiaozhi->event_cb = event_cb;
    g_xiaozhi->user_data = user_data;
    g_xiaozhi->state = XIAOZHI_STATE_DISCONNECTED;
    g_xiaozhi->msg_id = 0;

    // 配置HTTP客户端
    esp_http_client_config_t http_config = {0};
    setup_http_client_config(&http_config);

    g_xiaozhi->http_client = esp_http_client_init(&http_config);
    if (g_xiaozhi->http_client == NULL) {
        ESP_LOGE(TAG, "HTTP客户端初始化失败");
        free(g_xiaozhi);
        g_xiaozhi = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "小智客户端初始化完成");
    ESP_LOGI(TAG, "服务器URL: %s", g_xiaozhi->config.server_url);
    ESP_LOGI(TAG, "设备ID: %s", g_xiaozhi->config.device_id);

    return ESP_OK;
}

esp_err_t xiaozhi_deinit(void)
{
    if (g_xiaozhi == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化小智AI客户端");

    // 停止HTTP客户端
    if (g_xiaozhi->http_client) {
        esp_http_client_cleanup(g_xiaozhi->http_client);
    }

    // 释放内存
    free(g_xiaozhi);
    g_xiaozhi = NULL;

    ESP_LOGI(TAG, "小智客户端反初始化完成");
    return ESP_OK;
}

// ==================== 连接控制 ====================

esp_err_t xiaozhi_connect(void)
{
    if (g_xiaozhi == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "连接到服务器: %s", g_xiaozhi->config.server_url);

    // 打开HTTP连接
    esp_err_t err = esp_http_client_open(g_xiaozhi->http_client, 0);  // 0 = unknown length
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP连接打开失败: %s", esp_err_to_name(err));
        g_xiaozhi->state = XIAOZHI_STATE_ERROR;
        return err;
    }

    return ESP_OK;
}

esp_err_t xiaozhi_disconnect(void)
{
    if (g_xiaozhi == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "断开连接");

    // 关闭HTTP连接
    if (g_xiaozhi->http_client) {
        esp_http_client_close(g_xiaozhi->http_client);
    }

    g_xiaozhi->state = XIAOZHI_STATE_DISCONNECTED;
    return ESP_OK;
}

bool xiaozhi_is_connected(void)
{
    return (g_xiaozhi && (g_xiaozhi->state == XIAOZHI_STATE_CONNECTED ||
                                g_xiaozhi->state == XIAOZHI_STATE_AUTHENTICATED));
}

// ==================== 消息发送 ====================

esp_err_t xiaozhi_send_hello(void)
{
    if (g_xiaozhi == NULL || !xiaozhi_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "发送hello消息");

    // 发送HTTP POST
    esp_http_client_set_url(g_xiaozhi->http_client, g_xiaozhi->config.server_url);
    esp_http_client_set_method(g_xiaozhi->http_client, HTTP_METHOD_POST);
    esp_http_client_set_header(g_xiaozhi->http_client, "Content-Type", "application/json");

    const char *json_str = "{\"type\":\"hello\",\"version\":1,\"features\":{\"asr\":true,\"tts\":true}}";
    esp_http_client_set_post_field(g_xiaozhi->http_client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(g_xiaozhi->http_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发送hello失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "hello消息发送成功");
    return ESP_OK;
}

esp_err_t xiaozhi_send_listen(const char *state)
{
    if (g_xiaozhi == NULL || !xiaozhi_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (state == NULL || (strcmp(state, "start") != 0 && strcmp(state, "stop") != 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "发送listen消息: %s", state);

    // 构建listen消息JSON
    char json_str[128];
    snprintf(json_str, sizeof(json_str), "{\"type\":\"listen\",\"state\":\"%s\"}", state);

    // 发送HTTP POST
    esp_http_client_set_url(g_xiaozhi->http_client, g_xiaozhi->config.server_url);
    esp_http_client_set_method(g_xiaozhi->http_client, HTTP_METHOD_POST);
    esp_http_client_set_header(g_xiaozhi->http_client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(g_xiaozhi->http_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发送listen失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "listen消息发送成功");
    return ESP_OK;
}

esp_err_t xiaozhi_send_text(const char *text)
{
    if (g_xiaozhi == NULL || !xiaozhi_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "发送text消息: %s", text);

    // 构建text消息JSON
    char json_str[512];
    snprintf(json_str, sizeof(json_str), "{\"type\":\"text\",\"content\":\"%s\"}", text);

    // 发送HTTP POST
    esp_http_client_set_url(g_xiaozhi->http_client, g_xiaozhi->config.server_url);
    esp_http_client_set_method(g_xiaozhi->http_client, HTTP_METHOD_POST);
    esp_http_client_set_header(g_xiaozhi->http_client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(g_xiaozhi->http_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发送text失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "text消息发送成功");
    return ESP_OK;
}

esp_err_t xiaozhi_send_audio(const uint8_t *data, size_t len)
{
    if (g_xiaozhi == NULL || !xiaozhi_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "发送音频数据: %d bytes", len);

    // 使用multipart/form-data发送音频
    char boundary[32];
    snprintf(boundary, sizeof(boundary), "----WebKitFormBoundary%lu", (unsigned long)g_xiaozhi->msg_id++);

    // 发送HTTP POST
    esp_http_client_set_url(g_xiaozhi->http_client, g_xiaozhi->config.server_url);
    esp_http_client_set_method(g_xiaozhi->http_client, HTTP_METHOD_POST);

    // 设置Content-Type为multipart/form-data
    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(g_xiaozhi->http_client, "Content-Type", content_type);

    // TODO: 实现multipart数据发送
    ESP_LOGW(TAG, "音频发送功能待实现");

    return ESP_ERR_NOT_SUPPORTED;
}

// ==================== 状态查询 ====================

xiaozhi_state_t xiaozhi_get_state(void)
{
    return g_xiaozhi ? g_xiaozhi->state : XIAOZHI_STATE_DISCONNECTED;
}

// ==================== 配置 ====================

esp_err_t xiaozhi_set_server_url(const char *url)
{
    if (g_xiaozhi == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_xiaozhi->config.server_url, url, sizeof(g_xiaozhi->config.server_url) - 1);
    ESP_LOGI(TAG, "服务器URL更新为: %s", url);

    return ESP_OK;
}

esp_err_t xiaozhi_set_device_id(const char *device_id)
{
    if (g_xiaozhi == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_xiaozhi->config.device_id, device_id, sizeof(g_xiaozhi->config.device_id) - 1);
    ESP_LOGI(TAG, "设备ID更新为: %s", device_id);

    return ESP_OK;
}
