/**
 * @file tts_module_new.c
 * @brief TTS语音合成模块 - 小智AI方案（HTTP通信）
 */

#include "tts_module_new.h"
#include "xiaozhi_client.h"
#include "audio_manager.h"
#include "config.h"
#include "esp_log.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "TTS_MODULE";

// ==================== TTS模块上下文 ====================

typedef struct {
    tts_state_t state;
    tts_event_callback_t event_cb;
    void *user_data;

    // HTTP客户端
    char server_url[MAX_URL_LEN];

    // TTS音频缓冲
    uint8_t *audio_buffer;
    size_t audio_buffer_size;

    // 互斥锁
    SemaphoreHandle_t mutex;
} tts_module_t;

static tts_module_t *g_tts = NULL;

// ==================== TTS事件处理 ====================

/**
 * @brief TTS事件处理函数
 */
static void tts_event_handler(xiaozhi_event_t event, const void *data, void *user_data)
{
    switch (event) {
        case XIAOZHI_EVENT_AUTHENTICATED:
            ESP_LOGI(TAG, "认证成功");
            if (g_tts->event_cb) {
                g_tts->event_cb(TTS_EVENT_START, NULL, 0, g_tts->user_data);
            }
            break;

        case XIAOZHI_EVENT_AUDIO_RECEIVED:
            // 收到TTS音频数据
            if (data) {
                // TODO: 处理音频数据
                ESP_LOGI(TAG, "收到TTS音频数据");
            }
            break;

        case XIAOZHI_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "连接断开");
            if (g_tts->event_cb) {
                g_tts->event_cb(TTS_EVENT_END, NULL, 0, g_tts->user_data);
            }
            break;

        default:
            break;
    }
}

// ==================== 初始化和配置 ====================

esp_err_t tts_module_init(tts_event_callback_t event_cb, void *user_data)
{
    if (g_tts != NULL) {
        ESP_LOGW(TAG, "TTS模块已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化TTS模块（小智AI方案）");

    // 分配内存
    g_tts = (tts_module_t *)calloc(1, sizeof(tts_module_t));
    if (g_tts == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    strncpy(g_tts->server_url, "", sizeof(g_tts->server_url) - 1);

    g_tts->event_cb = event_cb;
    g_tts->user_data = user_data;
    g_tts->state = TTS_STATE_IDLE;
    g_tts->audio_buffer = NULL;
    g_tts->audio_buffer_size = 0;

    // 创建互斥锁
    g_tts->mutex = xSemaphoreCreateMutex();
    if (g_tts->mutex == NULL) {
        ESP_LOGE(TAG, "互斥锁创建失败");
        free(g_tts);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "TTS模块初始化完成");
    return ESP_OK;
}

esp_err_t tts_module_deinit(void)
{
    if (g_tts == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化TTS模块");

    // 停止TTS（如果正在播放）
    tts_module_stop_playback();

    // 删除互斥锁
    if (g_tts->mutex) {
        vSemaphoreDelete(g_tts->mutex);
    }

    // 释放内存
    free(g_tts);
    g_tts = NULL;

    ESP_LOGI(TAG, "TTS模块反初始化完成");
    return ESP_OK;
}

// ==================== TTS播放控制 ====================

esp_err_t tts_module_start(void)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "准备TTS播放");

    // TODO: 实现TTS请求
    ESP_LOGW(TAG, "TTS功能待实现");

    return ESP_OK;
}

esp_err_t tts_module_stop(void)
{
    if (g_tts == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止TTS播放");

    // 停止音频播放
    audio_manager_stop_play();

    g_tts->state = TTS_STATE_IDLE;
    return ESP_OK;
}

esp_err_t tts_module_send_text(const char *text)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "发送TTS文本: %s", text);

    // TODO: 实现TTS请求
    ESP_LOGW(TAG, "TTS功能待实现");

    return ESP_OK;
}

// ==================== 状态查询 ====================

tts_state_t tts_module_get_state(void)
{
    return g_tts ? g_tts->state : TTS_STATE_IDLE;
}
