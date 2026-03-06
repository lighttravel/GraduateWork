/**
 * @file tts_module.c
 * @brief TTS语音合成模块 - 统一接口层
 *
 * 支持的TTS服务:
 * 1. 科大讯飞TTS (推荐中文) - WebSocket流式
 * 2. Deepgram TTS - 仅英文
 * 3. Index-TTS - 用户自部署服务
 */

#include "tts_module.h"
#include "iflytek_tts.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"

static const char *TAG = "TTS_MODULE";

// TTS模块上下文
typedef struct {
    tts_provider_t provider;
    tts_state_t state;

    // 讯飞TTS配置
    char iflytek_appid[32];
    char iflytek_api_key[64];
    char iflytek_api_secret[128];
    char iflytek_voice[32];

    // Deepgram配置
    char deepgram_key[MAX_API_KEY_LEN];

    // Index-TTS配置
    char index_tts_url[MAX_URL_LEN];

    // 回调
    tts_data_callback_t data_cb;
    tts_event_callback_t event_cb;
    void *user_data;

    // 停止标志
    volatile bool stop_requested;

    // 互斥锁
    SemaphoreHandle_t mutex;
} tts_module_t;

static tts_module_t *g_tts = NULL;

// ==================== 讯飞TTS回调 ====================

static void iflytek_tts_event_handler(iflytek_tts_event_t event,
                                       const iflytek_tts_audio_t *audio,
                                       void *user_data)
{
    if (!g_tts) {
        ESP_LOGE(TAG, "iflytek_tts_event_handler: g_tts is NULL!");
        return;
    }

    ESP_LOGI(TAG, "========== iflytek_tts_event_handler 被调用, event=%d ==========", event);

    switch (event) {
        case IFLYTEK_TTS_EVENT_SYNTHESIZING:
            ESP_LOGI(TAG, "iFlytek TTS: 开始合成");
            g_tts->state = TTS_STATE_DOWNLOADING;
            break;

        case IFLYTEK_TTS_EVENT_AUDIO_DATA:
            g_tts->state = TTS_STATE_PLAYING;
            ESP_LOGI(TAG, "========== iFlytek TTS: 收到AUDIO_DATA事件 ==========");
            if (audio) {
                ESP_LOGI(TAG, "  audio=%p, audio_data=%p, len=%zu, is_final=%d",
                         (void*)audio, (void*)audio->audio_data, audio->audio_len, audio->is_final);
            } else {
                ESP_LOGW(TAG, "  audio指针为NULL!");
            }
            if (audio && audio->audio_data && audio->audio_len > 0) {
                ESP_LOGI(TAG, "iFlytek TTS: 收到音频 %d 字节，准备回调", (int)audio->audio_len);
                // 回调音频数据
                if (g_tts->data_cb && !g_tts->stop_requested) {
                    ESP_LOGI(TAG, "  调用data_cb回调 (data_cb=%p)...", (void*)g_tts->data_cb);
                    g_tts->data_cb(audio->audio_data, audio->audio_len, g_tts->user_data);
                    ESP_LOGI(TAG, "  回调完成");
                } else {
                    ESP_LOGW(TAG, "  data_cb=%p, stop=%d - 无法回调!", (void*)g_tts->data_cb, g_tts->stop_requested);
                }
            } else if (audio && audio->is_final) {
                ESP_LOGI(TAG, "  收到最终标志 (is_final=true)");
            } else {
                ESP_LOGW(TAG, "  音频数据无效，跳过回调");
            }
            break;

        case IFLYTEK_TTS_EVENT_COMPLETE:
            ESP_LOGI(TAG, "iFlytek TTS: 合成完成");
            g_tts->state = TTS_STATE_IDLE;
            if (g_tts->event_cb && !g_tts->stop_requested) {
                g_tts->event_cb(TTS_EVENT_DONE, g_tts->user_data);
            }
            break;

        case IFLYTEK_TTS_EVENT_ERROR:
            ESP_LOGE(TAG, "iFlytek TTS: 合成错误");
            g_tts->state = TTS_STATE_ERROR;
            if (g_tts->event_cb) {
                g_tts->event_cb(TTS_EVENT_ERROR, g_tts->user_data);
            }
            break;

        case IFLYTEK_TTS_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "iFlytek TTS: 连接断开");
            break;

        default:
            ESP_LOGW(TAG, "iFlytek TTS: 未知事件 %d", event);
            break;
    }
}

// ==================== 公共接口实现 ====================

esp_err_t tts_module_init(tts_provider_t provider)
{
    if (g_tts != NULL) {
        ESP_LOGW(TAG, "TTS模块已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化TTS模块，提供商: %d", provider);

    // 分配内存
    g_tts = (tts_module_t *)calloc(1, sizeof(tts_module_t));
    if (g_tts == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    g_tts->provider = provider;
    g_tts->state = TTS_STATE_IDLE;
    g_tts->stop_requested = false;

    // 默认讯飞TTS配置
    strlcpy(g_tts->iflytek_appid, IFLYTEK_APPID, sizeof(g_tts->iflytek_appid));
    strlcpy(g_tts->iflytek_api_key, IFLYTEK_API_KEY, sizeof(g_tts->iflytek_api_key));
    strlcpy(g_tts->iflytek_api_secret, IFLYTEK_API_SECRET, sizeof(g_tts->iflytek_api_secret));
    strlcpy(g_tts->iflytek_voice, "xiaoyan", sizeof(g_tts->iflytek_voice));  // 通用中文女声

    // 创建互斥锁
    g_tts->mutex = xSemaphoreCreateMutex();
    if (g_tts->mutex == NULL) {
        free(g_tts);
        g_tts = NULL;
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

    tts_module_stop();

    // 如果使用讯飞TTS，反初始化
    if (g_tts->provider == TTS_PROVIDER_IFLYTEK) {
        iflytek_tts_deinit();
    }

    if (g_tts->mutex) {
        vSemaphoreDelete(g_tts->mutex);
    }

    free(g_tts);
    g_tts = NULL;

    return ESP_OK;
}

esp_err_t tts_module_speak(const char *text,
                           tts_data_callback_t data_cb,
                           tts_event_callback_t event_cb,
                           void *user_data)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (text == NULL || strlen(text) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);

    // 检查是否正在播放
    if (g_tts->state == TTS_STATE_DOWNLOADING || g_tts->state == TTS_STATE_PLAYING) {
        ESP_LOGW(TAG, "TTS正在工作中，请稍后再试");
        xSemaphoreGive(g_tts->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "开始TTS合成: %.50s%s", text, strlen(text) > 50 ? "..." : "");

    // 保存回调
    g_tts->data_cb = data_cb;
    g_tts->event_cb = event_cb;
    g_tts->user_data = user_data;
    g_tts->state = TTS_STATE_DOWNLOADING;
    g_tts->stop_requested = false;

    xSemaphoreGive(g_tts->mutex);

    // 触发开始事件
    if (event_cb) {
        event_cb(TTS_EVENT_START, user_data);
    }

    esp_err_t ret = ESP_FAIL;

    // 根据提供商选择TTS服务
    switch (g_tts->provider) {
        case TTS_PROVIDER_IFLYTEK:
            ret = tts_module_speak_iflytek(text);
            break;

        case TTS_PROVIDER_INDEX_TTS:
            ESP_LOGW(TAG, "Index-TTS暂未实现");
            ret = ESP_ERR_NOT_SUPPORTED;
            break;

        case TTS_PROVIDER_DEEPGRAM:
            ESP_LOGW(TAG, "Deepgram不支持中文，请使用讯飞TTS");
            ret = ESP_ERR_NOT_SUPPORTED;
            break;

        default:
            ESP_LOGE(TAG, "未知的TTS提供商: %d", g_tts->provider);
            ret = ESP_ERR_INVALID_ARG;
            break;
    }

    if (ret != ESP_OK) {
        g_tts->state = TTS_STATE_ERROR;
        if (event_cb) {
            event_cb(TTS_EVENT_ERROR, user_data);
        }
    }

    return ret;
}

esp_err_t tts_module_speak_iflytek(const char *text)
{
    if (!g_tts) {
        ESP_LOGE(TAG, "tts_module_speak_iflytek: g_tts is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "使用讯飞TTS合成");

    // 检查是否已初始化讯飞TTS
    if (!iflytek_tts_is_connected()) {
        ESP_LOGI(TAG, "讯飞TTS未连接，开始初始化...");

        // 配置讯飞TTS
        iflytek_tts_config_t config = {
            .sample_rate = 16000,
            .speed = 50,
            .volume = 50,
            .pitch = 50
        };
        strlcpy(config.appid, g_tts->iflytek_appid, sizeof(config.appid));
        strlcpy(config.api_key, g_tts->iflytek_api_key, sizeof(config.api_key));
        strlcpy(config.api_secret, g_tts->iflytek_api_secret, sizeof(config.api_secret));
        strlcpy(config.voice_name, g_tts->iflytek_voice, sizeof(config.voice_name));

        ESP_LOGI(TAG, "讯飞TTS配置 - APPID: %s, Voice: %s", config.appid, config.voice_name);

        // 初始化讯飞TTS
        esp_err_t ret = iflytek_tts_init(&config, iflytek_tts_event_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "讯飞TTS初始化失败: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "讯飞TTS初始化成功");
    } else {
        ESP_LOGI(TAG, "讯飞TTS已连接，直接合成");
    }

    // 开始合成
    ESP_LOGI(TAG, "调用iflytek_tts_synthesize...");
    esp_err_t ret = iflytek_tts_synthesize(text);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "iflytek_tts_synthesize失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t tts_module_stop(void)
{
    if (g_tts == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止TTS合成");

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);
    g_tts->stop_requested = true;
    g_tts->state = TTS_STATE_IDLE;

    // 停止讯飞TTS
    if (g_tts->provider == TTS_PROVIDER_IFLYTEK) {
        iflytek_tts_stop();
    }

    xSemaphoreGive(g_tts->mutex);

    return ESP_OK;
}

// ==================== 配置接口 ====================

esp_err_t tts_module_set_deepgram_key(const char *api_key)
{
    if (g_tts == NULL || api_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);
    strncpy(g_tts->deepgram_key, api_key, sizeof(g_tts->deepgram_key) - 1);
    xSemaphoreGive(g_tts->mutex);

    ESP_LOGI(TAG, "Deepgram API密钥已设置");
    return ESP_OK;
}

esp_err_t tts_module_set_index_tts_url(const char *url)
{
    if (g_tts == NULL || url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);
    strncpy(g_tts->index_tts_url, url, sizeof(g_tts->index_tts_url) - 1);
    xSemaphoreGive(g_tts->mutex);

    ESP_LOGI(TAG, "Index-TTS URL已设置: %s", url);
    return ESP_OK;
}

esp_err_t tts_module_set_provider(tts_provider_t provider)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);
    g_tts->provider = provider;
    xSemaphoreGive(g_tts->mutex);

    const char *provider_name = "Unknown";
    switch (provider) {
        case TTS_PROVIDER_DEEPGRAM: provider_name = "Deepgram"; break;
        case TTS_PROVIDER_INDEX_TTS: provider_name = "Index-TTS"; break;
        case TTS_PROVIDER_IFLYTEK: provider_name = "iFlytek"; break;
    }
    ESP_LOGI(TAG, "TTS提供商设置为: %s", provider_name);

    return ESP_OK;
}

esp_err_t tts_module_set_iflytek_config(const char *appid, const char *api_key,
                                         const char *api_secret, const char *voice)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);

    if (appid) {
        strncpy(g_tts->iflytek_appid, appid, sizeof(g_tts->iflytek_appid) - 1);
    }
    if (api_key) {
        strncpy(g_tts->iflytek_api_key, api_key, sizeof(g_tts->iflytek_api_key) - 1);
    }
    if (api_secret) {
        strncpy(g_tts->iflytek_api_secret, api_secret, sizeof(g_tts->iflytek_api_secret) - 1);
    }
    if (voice) {
        strncpy(g_tts->iflytek_voice, voice, sizeof(g_tts->iflytek_voice) - 1);
    }

    xSemaphoreGive(g_tts->mutex);

    ESP_LOGI(TAG, "讯飞TTS配置已更新");
    return ESP_OK;
}

// ==================== 状态查询 ====================

tts_state_t tts_module_get_state(void)
{
    return g_tts ? g_tts->state : TTS_STATE_IDLE;
}

bool tts_module_is_speaking(void)
{
    if (g_tts == NULL) return false;
    return (g_tts->state == TTS_STATE_DOWNLOADING || g_tts->state == TTS_STATE_PLAYING);
}

esp_err_t tts_module_get_audio_data(uint8_t **data, size_t *len)
{
    // 此函数已不推荐使用，音频通过回调实时返回
    if (g_tts == NULL || data == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *data = NULL;
    *len = 0;

    return ESP_ERR_NOT_SUPPORTED;
}
