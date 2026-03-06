/**
 * @file asr_module_new.c
 * @brief ASR语音识别模块 - 小智AI方案（HTTP通信）
 */

#include "asr_module_new.h"
#include "xiaozhi_client.h"
#include "audio_manager.h"
#include "config.h"
#include "esp_log.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "ASR_MODULE";

// ==================== ASR模块上下文 ====================

typedef struct {
    asr_state_t state;
    asr_event_callback_t event_cb;
    void *user_data;

    // HTTP客户端
    char server_url[MAX_URL_LEN];

    // 录音缓冲
    uint8_t *record_buffer;
    size_t record_buffer_size;
    size_t record_buffer_offset;

    // 互斥锁
    SemaphoreHandle_t mutex;
} asr_module_t;

static asr_module_t *g_asr = NULL;

// ==================== 辅助函数 ====================

/**
 * @brief ASR事件处理函数
 */
static void asr_event_handler(xiaozhi_event_t event, const void *data, void *user_data)
{
    switch (event) {
        case XIAOZHI_EVENT_AUTHENTICATED:
            ESP_LOGI(TAG, "认证成功");
            if (g_asr->event_cb) {
                asr_result_t result = {0};
                result.is_final = false;
                g_asr->event_cb(ASR_EVENT_CONNECTED, &result, g_asr->user_data);
            }
            break;

        case XIAOZHI_EVENT_TEXT_RECEIVED:
            // 收到服务器文本消息
            if (data) {
                xiaozhi_text_data_t *text_data = (xiaozhi_text_data_t *)data;
                ESP_LOGI(TAG, "收到文本: %s", text_data->text);

                if (g_asr->event_cb) {
                    asr_result_t result = {0};
                    strncpy(result.text, text_data->text, sizeof(result.text) - 1);
                    result.is_final = text_data->is_final;
                    g_asr->event_cb(ASR_EVENT_TRANSCRIPT, &result, g_asr->user_data);
                }
            }
            break;

        case XIAOZHI_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "连接断开");
            if (g_asr->event_cb) {
                g_asr->event_cb(ASR_EVENT_DISCONNECTED, NULL, g_asr->user_data);
            }
            break;

        default:
            break;
    }
}

// ==================== 初始化和配置 ====================

esp_err_t asr_module_init(const char *server_url, asr_event_callback_t event_cb, void *user_data)
{
    if (g_asr != NULL) {
        ESP_LOGW(TAG, "ASR模块已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化ASR模块（小智AI方案）");

    // 分配内存
    g_asr = (asr_module_t *)calloc(1, sizeof(asr_module_t));
    if (g_asr == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    if (server_url) {
        strncpy(g_asr->server_url, server_url, sizeof(g_asr->server_url) - 1);
    } else {
        strncpy(g_asr->server_url, "", sizeof(g_asr->server_url) - 1);
    }

    g_asr->event_cb = event_cb;
    g_asr->user_data = user_data;
    g_asr->state = ASR_STATE_IDLE;
    g_asr->record_buffer = NULL;
    g_asr->record_buffer_size = 0;
    g_asr->record_buffer_offset = 0;

    // 创建互斥锁
    g_asr->mutex = xSemaphoreCreateMutex();
    if (g_asr->mutex == NULL) {
        ESP_LOGE(TAG, "互斥锁创建失败");
        free(g_asr);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ASR模块初始化完成");
    return ESP_OK;
}

esp_err_t asr_module_deinit(void)
{
    if (g_asr == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化ASR模块");

    // 停止录音
    asr_module_stop();

    // 释放缓冲区
    if (g_asr->record_buffer) {
        free(g_asr->record_buffer);
    }

    // 删除互斥锁
    if (g_asr->mutex) {
        vSemaphoreDelete(g_asr->mutex);
    }

    // 释放内存
    free(g_asr);
    g_asr = NULL;

    ESP_LOGI(TAG, "ASR模块反初始化完成");
    return ESP_OK;
}

// ==================== 识别控制 ====================

esp_err_t asr_module_connect(void)
{
    if (g_asr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "连接到ASR服务器: %s", g_asr->server_url);

    // 使用xiaozhi_client连接
    xiaozhi_config_t config = {0};
    strncpy(config.server_url, g_asr->server_url, sizeof(config.server_url) - 1);
    strncpy(config.device_id, XIAOZHI_DEVICE_ID, sizeof(config.device_id) - 1);

    esp_err_t ret = xiaozhi_init(&config, asr_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "xiaozhi客户端初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = xiaozhi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "连接失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_asr->state = ASR_STATE_CONNECTING;
    return ESP_OK;
}

esp_err_t asr_module_disconnect(void)
{
    if (g_asr == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "断开ASR连接");

    esp_err_t ret = xiaozhi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "断开连接失败: %s", esp_err_to_name(ret));
    }

    g_asr->state = ASR_STATE_IDLE;
    return ESP_OK;
}

esp_err_t asr_module_start(void)
{
    if (g_asr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_asr->state == ASR_STATE_LISTENING) {
        ESP_LOGW(TAG, "已在监听中");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "开始ASR识别");

    // 分配录音缓冲区
    if (g_asr->record_buffer == NULL) {
        g_asr->record_buffer_size = 16000; // 16kHz * 1秒
        g_asr->record_buffer = (uint8_t *)malloc(g_asr->record_buffer_size);
        if (g_asr->record_buffer == NULL) {
            ESP_LOGE(TAG, "录音缓冲区分配失败");
            return ESP_ERR_NO_MEM;
        }
        g_asr->record_buffer_offset = 0;
    }

    // 发送listen start
    esp_err_t ret = xiaozhi_send_listen("start");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送listen start失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 开始录音
    ret = audio_manager_start_record();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动录音失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_asr->state = ASR_STATE_LISTENING;
    return ESP_OK;
}

esp_err_t asr_module_stop(void)
{
    if (g_asr == NULL) {
        return ESP_OK;
    }

    if (g_asr->state != ASR_STATE_LISTENING) {
        ESP_LOGW(TAG, "未在监听中");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止ASR识别");

    // 发送listen stop
    xiaozhi_send_listen("stop");

    // 停止录音
    audio_manager_stop_record();

    g_asr->state = ASR_STATE_IDLE;
    return ESP_OK;
}

esp_err_t asr_module_send_audio(const uint8_t *data, size_t len)
{
    if (g_asr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // 使用HTTP POST发送音频数据
    esp_err_t ret = xiaozhi_send_audio(data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送音频失败: %s", esp_err_to_name(ret));
    return ret;
    }

    return ESP_OK;
}

// ==================== 状态查询 ====================

asr_state_t asr_module_get_state(void)
{
    return g_asr ? g_asr->state : ASR_STATE_IDLE;
}

bool asr_module_is_listening(void)
{
    return g_asr ? (g_asr->state == ASR_STATE_LISTENING) : false;
}

bool asr_module_is_connected(void)
{
    return xiaozhi_is_connected();
}
