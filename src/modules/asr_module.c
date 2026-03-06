/**
 * @file asr_module.c
 * @brief ASR模块实现 - 存根实现(WebSocket客户端需要外部库)
 *
 * 注意: ESP-IDF 5.x 没有内置的 WebSocket 客户端组件。
 * 完整实现需要使用第三方 WebSocket 库或基于 HTTP 重新实现。
 */

#include "asr_module.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ASR_MODULE_STUB";

// ASR模块上下文
typedef struct {
    asr_state_t state;
    asr_event_callback_t event_cb;
    void *user_data;

    // API配置
    char api_key[MAX_API_KEY_LEN];
    char language[16];  // zh-CN, en-US等

    // 配置
    bool punctuate;
    bool smart_format;

    // 互斥锁
    SemaphoreHandle_t mutex;
} asr_module_t;

static asr_module_t *g_asr = NULL;

// ==================== 初始化和配置 ====================

esp_err_t asr_module_init(const char *api_key, asr_event_callback_t event_cb, void *user_data)
{
    if (g_asr != NULL) {
        ESP_LOGW(TAG, "ASR模块已初始化");
        return ESP_OK;
    }

    if (api_key == NULL || strlen(api_key) == 0) {
        ESP_LOGE(TAG, "API密钥为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "初始化ASR模块(STUB模式 - WebSocket功能不可用)");

    // 分配内存
    g_asr = (asr_module_t *)calloc(1, sizeof(asr_module_t));
    if (g_asr == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    strncpy(g_asr->api_key, api_key, sizeof(g_asr->api_key) - 1);
    g_asr->event_cb = event_cb;
    g_asr->user_data = user_data;
    g_asr->state = ASR_STATE_IDLE;
    strncpy(g_asr->language, "zh-CN", sizeof(g_asr->language) - 1);
    g_asr->punctuate = true;
    g_asr->smart_format = true;

    // 创建互斥锁
    g_asr->mutex = xSemaphoreCreateMutex();
    if (g_asr->mutex == NULL) {
        free(g_asr);
        g_asr = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ASR模块初始化完成(STUB模式)");
    return ESP_OK;
}

esp_err_t asr_module_deinit(void)
{
    if (g_asr == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化ASR模块");

    asr_module_stop();

    if (g_asr->mutex) {
        vSemaphoreDelete(g_asr->mutex);
    }

    free(g_asr);
    g_asr = NULL;

    return ESP_OK;
}

// ==================== 识别控制 ====================

esp_err_t asr_module_start(void)
{
    if (g_asr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_asr->state == ASR_STATE_CONNECTED || g_asr->state == ASR_STATE_LISTENING) {
        ESP_LOGW(TAG, "ASR已在运行");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "ASR启动失败 - WebSocket客户端需要外部库支持");
    ESP_LOGW(TAG, "请参考: https://github.com/espressif/esp-protocol-websocket");

    g_asr->state = ASR_STATE_IDLE;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t asr_module_stop(void)
{
    if (g_asr == NULL || g_asr->state == ASR_STATE_IDLE) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止ASR识别");
    g_asr->state = ASR_STATE_IDLE;

    return ESP_OK;
}

esp_err_t asr_module_send_audio(const uint8_t *data, size_t len)
{
    if (g_asr == NULL || g_asr->state != ASR_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // STUB模式 - 不发送数据
    return ESP_ERR_NOT_SUPPORTED;
}

// ==================== 状态查询 ====================

asr_state_t asr_module_get_state(void)
{
    return g_asr ? g_asr->state : ASR_STATE_IDLE;
}

bool asr_module_is_listening(void)
{
    asr_state_t state = asr_module_get_state();
    return (state == ASR_STATE_CONNECTED || state == ASR_STATE_LISTENING);
}

// ==================== 配置 ====================

esp_err_t asr_module_set_language(const char *language)
{
    if (g_asr && language) {
        strncpy(g_asr->language, language, sizeof(g_asr->language) - 1);
        ESP_LOGI(TAG, "语言设置为: %s", language);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

esp_err_t asr_module_set_punctuate(bool enable)
{
    if (g_asr) {
        g_asr->punctuate = enable;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

esp_err_t asr_module_set_smart_format(bool enable)
{
    if (g_asr) {
        g_asr->smart_format = enable;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

// ==================== TODO说明 ====================

/*
 * 完整实现需要添加 ESP Protocol WebSocket 组件：
 *
 * 1. 添加组件到项目:
 *   idf.py add-dependency "espressif/esp_protocol_components^1.0.0"
 *
 * 2. 使用正确的头文件:
 *    #include "esp_websocket_client.h"
 *
 * 3. 参考官方文档:
 *    https://github.com/espressif/esp-protocol-websocket
 *
 * Deepgram API参考：
 * - URL: wss://api.deepgram.com/v1/listen
 * - Headers: Authorization: Token YOUR_API_KEY
 * - 配置选项通过JSON发送: {"punctuate": true, "smart_format": true, "language": "zh-CN"}
 */
