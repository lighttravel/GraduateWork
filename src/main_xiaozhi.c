#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "config.h"
#include "middleware/audio_manager.h"
#include "middleware/wifi_manager.h"
#include "modules/chat_module.h"
#include "modules/iflytek_asr.h"
#include "modules/tts_module.h"
#include "modules/wakenet_module.h"

static const char *TAG = "XIAOZHI";

#define EVT_WIFI_CONNECTED BIT0
#define EVT_WAKE_WORD      BIT1
#define EVT_ASR_FINAL      BIT2
#define EVT_ASR_ERROR      BIT3
#define EVT_CHAT_DONE      BIT4
#define EVT_CHAT_ERROR     BIT5
#define EVT_TTS_DONE       BIT6
#define EVT_TTS_ERROR      BIT7
#define EVT_AUDIO_ERROR    BIT8

#define ASR_RECORD_SECONDS   6
#define ASR_FINAL_TIMEOUT_MS 12000
#define CHAT_TIMEOUT_MS      30000
#define TTS_TIMEOUT_MS       60000

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_WAIT_WAKE,
    APP_STATE_LISTENING,
    APP_STATE_THINKING,
    APP_STATE_SPEAKING,
} app_state_t;

static EventGroupHandle_t g_events;
static volatile bool g_capture_enabled;
static bool g_chat_ready;
static app_state_t g_app_state;
static char g_asr_text[sizeof(((iflytek_asr_result_t *)0)->text)];
static char g_reply_text[1024];

static esp_err_t init_nvs_storage(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t init_sntp_time(void)
{
    ESP_LOGI(TAG, "SNTP sync starting");

    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.ntp.org.cn");
    esp_sntp_setservername(2, "ntp.tencent.com");
    esp_sntp_init();

    for (int retry = 0; retry < 60; ++retry) {
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            time_t now;
            struct tm timeinfo;
            char time_buf[32];

            time(&now);
            localtime_r(&now, &timeinfo);
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
            ESP_LOGI(TAG, "SNTP synced: %s", time_buf);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGW(TAG, "SNTP timeout, TLS auth may be unstable");
    return ESP_ERR_TIMEOUT;
}

static void wifi_event_callback(wifi_mgr_event_t event, void *user_data)
{
    (void)user_data;

    if (event == WIFI_MGR_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(g_events, EVT_WIFI_CONNECTED);
    } else if (event == WIFI_MGR_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(g_events, EVT_WIFI_CONNECTED);
    }
}

static esp_err_t init_wifi_connection(void)
{
    wifi_sta_config_t sta_config = {0};

    ESP_RETURN_ON_ERROR(wifi_manager_init(wifi_event_callback, NULL), TAG, "wifi_manager_init failed");

    strncpy((char *)sta_config.ssid, DEFAULT_WIFI_SSID, sizeof(sta_config.ssid) - 1);
    strncpy((char *)sta_config.password, DEFAULT_WIFI_PASSWORD, sizeof(sta_config.password) - 1);

    ESP_RETURN_ON_ERROR(wifi_manager_start_sta(&sta_config), TAG, "wifi_manager_start_sta failed");

    EventBits_t bits = xEventGroupWaitBits(
        g_events,
        EVT_WIFI_CONNECTED,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if ((bits & EVT_WIFI_CONNECTED) == 0) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
    return ESP_OK;
}

static void audio_event_callback(audio_event_t event, void *user_data)
{
    (void)user_data;

    switch (event) {
        case AUDIO_EVENT_RECORD_START:
            ESP_LOGI(TAG, "record start");
            break;
        case AUDIO_EVENT_RECORD_STOP:
            ESP_LOGI(TAG, "record stop");
            break;
        case AUDIO_EVENT_PLAY_START:
            ESP_LOGI(TAG, "play start");
            break;
        case AUDIO_EVENT_PLAY_STOP:
            ESP_LOGI(TAG, "play stop");
            break;
        case AUDIO_EVENT_ERROR:
            ESP_LOGE(TAG, "audio error");
            xEventGroupSetBits(g_events, EVT_AUDIO_ERROR);
            break;
        default:
            break;
    }
}

static void wakenet_event_callback(wakenet_event_t event, const wakenet_result_t *result, void *user_data)
{
    (void)user_data;

    if (event == WAKENET_EVENT_DETECTED) {
        ESP_LOGI(TAG, "wake detected score=%.2f", result ? result->score : 0.0f);
        xEventGroupSetBits(g_events, EVT_WAKE_WORD);
    } else if (event == WAKENET_EVENT_ERROR) {
        ESP_LOGE(TAG, "wake module error");
        xEventGroupSetBits(g_events, EVT_AUDIO_ERROR);
    }
}

static void asr_event_callback(iflytek_asr_event_t event,
                               const iflytek_asr_result_t *result,
                               void *user_data)
{
    (void)user_data;

    switch (event) {
        case IFLYTEK_ASR_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ASR connected");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_START:
            ESP_LOGI(TAG, "ASR listening started");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_STOP:
            ESP_LOGI(TAG, "ASR listening stopped");
            break;

        case IFLYTEK_ASR_EVENT_RESULT_PARTIAL:
            if (result != NULL && result->text[0] != '\0') {
                ESP_LOGI(TAG, "ASR partial: %s", result->text);
            }
            break;

        case IFLYTEK_ASR_EVENT_RESULT_FINAL:
            if (result != NULL) {
                strlcpy(g_asr_text, result->text, sizeof(g_asr_text));
                ESP_LOGI(TAG, "ASR final: %s", g_asr_text);
            } else {
                g_asr_text[0] = '\0';
            }
            xEventGroupSetBits(g_events, EVT_ASR_FINAL);
            break;

        case IFLYTEK_ASR_EVENT_ERROR:
            ESP_LOGE(TAG, "ASR error");
            xEventGroupSetBits(g_events, EVT_ASR_ERROR);
            break;

        case IFLYTEK_ASR_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "ASR disconnected");
            break;

        default:
            break;
    }
}

static void chat_event_callback(chat_event_t event, const char *data, bool is_done, void *user_data)
{
    (void)is_done;
    (void)user_data;

    switch (event) {
        case CHAT_EVENT_START:
            g_reply_text[0] = '\0';
            ESP_LOGI(TAG, "chat start");
            break;

        case CHAT_EVENT_DATA:
            if (data != NULL && data[0] != '\0') {
                strlcat(g_reply_text, data, sizeof(g_reply_text));
            }
            break;

        case CHAT_EVENT_DONE:
            ESP_LOGI(TAG, "chat done: %s", g_reply_text);
            xEventGroupSetBits(g_events, EVT_CHAT_DONE);
            break;

        case CHAT_EVENT_ERROR:
            ESP_LOGE(TAG, "chat error");
            xEventGroupSetBits(g_events, EVT_CHAT_ERROR);
            break;

        default:
            break;
    }
}

static void tts_event_callback(tts_event_t event, void *user_data)
{
    (void)user_data;

    switch (event) {
        case TTS_EVENT_START:
            ESP_LOGI(TAG, "TTS start");
            break;
        case TTS_EVENT_DONE:
            ESP_LOGI(TAG, "TTS done");
            xEventGroupSetBits(g_events, EVT_TTS_DONE);
            break;
        case TTS_EVENT_ERROR:
            ESP_LOGE(TAG, "TTS error");
            xEventGroupSetBits(g_events, EVT_TTS_ERROR);
            break;
        default:
            break;
    }
}

static void tts_data_callback(const uint8_t *data, size_t len, void *user_data)
{
    (void)user_data;

    if (data == NULL || len == 0) {
        return;
    }

    esp_err_t ret = audio_manager_play_tts_audio(data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_manager_play_tts_audio failed: %s", esp_err_to_name(ret));
        xEventGroupSetBits(g_events, EVT_TTS_ERROR);
    }
}

static void audio_data_callback(uint8_t *data, size_t len, void *user_data)
{
    (void)user_data;

    if (data == NULL || len == 0) {
        return;
    }

    if (g_app_state == APP_STATE_WAIT_WAKE && wakenet_is_listening()) {
        (void)wakenet_process_audio((const int16_t *)data, len / sizeof(int16_t));
        return;
    }

    if (g_app_state != APP_STATE_LISTENING || !g_capture_enabled || !iflytek_asr_is_listening()) {
        return;
    }

    for (size_t offset = 0; offset < len; offset += IFLYTEK_FRAME_SIZE) {
        size_t chunk_len = len - offset;
        if (chunk_len > IFLYTEK_FRAME_SIZE) {
            chunk_len = IFLYTEK_FRAME_SIZE;
        }

        esp_err_t ret = iflytek_asr_send_audio(data + offset, chunk_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "send audio failed: %s", esp_err_to_name(ret));
            xEventGroupSetBits(g_events, EVT_ASR_ERROR);
            return;
        }
    }
}

static esp_err_t init_audio_pipeline(void)
{
    audio_config_t audio_cfg = {
        .sample_rate = I2S_SAMPLE_RATE,
        .volume = 55,
        .vad_enabled = false,
        .vad_threshold = VAD_THRESHOLD,
        .data_cb = audio_data_callback,
        .event_cb = audio_event_callback,
        .user_data = NULL,
    };

    return audio_manager_init(&audio_cfg);
}

static esp_err_t init_wake_module(void)
{
    wakenet_config_t config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .vad_enable = true,
        .vad_threshold = 0.2f,
        .aec_enable = false,
        .aec_filter = false,
    };

    strlcpy(config.wake_word, "nihaoxiaozhi", sizeof(config.wake_word));
    return wakenet_init(&config, wakenet_event_callback, NULL);
}

static esp_err_t init_asr_module(void)
{
    iflytek_asr_config_t asr_cfg = {
        .appid = IFLYTEK_APPID,
        .api_key = IFLYTEK_API_KEY,
        .api_secret = IFLYTEK_API_SECRET,
        .language = IFLYTEK_ASR_LANGUAGE,
        .domain = IFLYTEK_ASR_DOMAIN,
        .enable_punctuation = true,
        .enable_nlp = false,
        .sample_rate = I2S_SAMPLE_RATE,
    };

    return iflytek_asr_init(&asr_cfg, asr_event_callback, NULL);
}

static void init_chat_module_optional(void)
{
    esp_err_t ret;

    g_chat_ready = false;
    if (DEEPSEEK_API_KEY[0] == '\0') {
        ESP_LOGW(TAG, "chat disabled: no DeepSeek API key");
        return;
    }

    ret = chat_module_init(DEEPSEEK_API_KEY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "chat init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = chat_module_set_system_prompt("你是小智，一个简洁自然的中文语音助手。回答简短、直接，适合语音播报。");
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "chat prompt setup failed: %s", esp_err_to_name(ret));
    }

    g_chat_ready = true;
}

static esp_err_t init_tts_module(void)
{
    return tts_module_init(TTS_PROVIDER_IFLYTEK);
}

static esp_err_t wait_for_wake_word(void)
{
    EventBits_t bits;

    g_app_state = APP_STATE_WAIT_WAKE;
    xEventGroupClearBits(g_events, EVT_WAKE_WORD | EVT_AUDIO_ERROR);

    ESP_RETURN_ON_ERROR(wakenet_start(), TAG, "wakenet_start failed");
    ESP_RETURN_ON_ERROR(audio_manager_start_record(), TAG, "audio_manager_start_record failed");

    ESP_LOGI(TAG, "waiting wake word");
    bits = xEventGroupWaitBits(
        g_events,
        EVT_WAKE_WORD | EVT_AUDIO_ERROR,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    (void)audio_manager_stop_record();
    (void)wakenet_stop();

    if ((bits & EVT_AUDIO_ERROR) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t run_asr_session(void)
{
    EventBits_t bits;

    g_app_state = APP_STATE_LISTENING;
    g_capture_enabled = false;
    g_asr_text[0] = '\0';
    xEventGroupClearBits(g_events, EVT_ASR_FINAL | EVT_ASR_ERROR | EVT_AUDIO_ERROR);

    ESP_RETURN_ON_ERROR(iflytek_asr_connect(), TAG, "iflytek_asr_connect failed");
    ESP_RETURN_ON_ERROR(iflytek_asr_start_listening(), TAG, "iflytek_asr_start_listening failed");

    vTaskDelay(pdMS_TO_TICKS(200));
    g_capture_enabled = true;
    ESP_RETURN_ON_ERROR(audio_manager_start_record(), TAG, "audio_manager_start_record failed");

    ESP_LOGW(TAG, "Speak now for %d seconds", ASR_RECORD_SECONDS);
    vTaskDelay(pdMS_TO_TICKS(ASR_RECORD_SECONDS * 1000));

    g_capture_enabled = false;
    (void)audio_manager_stop_record();
    (void)iflytek_asr_stop_listening();

    bits = xEventGroupWaitBits(
        g_events,
        EVT_ASR_FINAL | EVT_ASR_ERROR | EVT_AUDIO_ERROR,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(ASR_FINAL_TIMEOUT_MS));

    (void)iflytek_asr_disconnect();

    if ((bits & (EVT_ASR_ERROR | EVT_AUDIO_ERROR)) != 0) {
        return ESP_FAIL;
    }
    if ((bits & EVT_ASR_FINAL) == 0) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void build_fallback_reply(void)
{
    if (g_asr_text[0] == '\0') {
        strlcpy(g_reply_text, "我没听清，请再说一遍。", sizeof(g_reply_text));
    } else {
        snprintf(g_reply_text, sizeof(g_reply_text), "你刚才说的是：%s", g_asr_text);
    }
}

static void build_reply_text(void)
{
    EventBits_t bits;

    if (g_asr_text[0] == '\0') {
        build_fallback_reply();
        return;
    }

    if (!g_chat_ready) {
        build_fallback_reply();
        return;
    }

    g_app_state = APP_STATE_THINKING;
    g_reply_text[0] = '\0';
    xEventGroupClearBits(g_events, EVT_CHAT_DONE | EVT_CHAT_ERROR);

    if (chat_module_send_message(g_asr_text, chat_event_callback, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "chat send failed, fallback to echo reply");
        build_fallback_reply();
        return;
    }

    bits = xEventGroupWaitBits(
        g_events,
        EVT_CHAT_DONE | EVT_CHAT_ERROR,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(CHAT_TIMEOUT_MS));

    if ((bits & EVT_CHAT_DONE) == 0 || g_reply_text[0] == '\0') {
        ESP_LOGW(TAG, "chat reply unavailable, fallback to echo reply");
        build_fallback_reply();
    }
}

static esp_err_t speak_reply(void)
{
    EventBits_t bits;

    if (g_reply_text[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    g_app_state = APP_STATE_SPEAKING;
    xEventGroupClearBits(g_events, EVT_TTS_DONE | EVT_TTS_ERROR | EVT_AUDIO_ERROR);

    ESP_RETURN_ON_ERROR(audio_manager_start_tts_playback(), TAG, "audio_manager_start_tts_playback failed");
    ESP_RETURN_ON_ERROR(tts_module_speak(g_reply_text, tts_data_callback, tts_event_callback, NULL),
                        TAG,
                        "tts_module_speak failed");

    bits = xEventGroupWaitBits(
        g_events,
        EVT_TTS_DONE | EVT_TTS_ERROR | EVT_AUDIO_ERROR,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(TTS_TIMEOUT_MS));

    (void)tts_module_stop();
    (void)audio_manager_stop_tts_playback();

    if ((bits & EVT_TTS_DONE) == 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void app_main(void)
{
    printf("\n========================================\n");
    printf(" XiaoZhi Main Flow: Wake -> ASR -> Reply\n");
    printf("========================================\n");

    g_events = xEventGroupCreate();
    if (g_events == NULL) {
        ESP_LOGE(TAG, "failed to create event group");
        return;
    }

    g_app_state = APP_STATE_BOOT;
    g_capture_enabled = false;
    g_asr_text[0] = '\0';
    g_reply_text[0] = '\0';

    ESP_ERROR_CHECK(init_nvs_storage());
    ESP_ERROR_CHECK(init_wifi_connection());
    (void)init_sntp_time();
    ESP_ERROR_CHECK(init_audio_pipeline());
    ESP_ERROR_CHECK(init_wake_module());
    ESP_ERROR_CHECK(init_asr_module());
    init_chat_module_optional();
    ESP_ERROR_CHECK(init_tts_module());

    ESP_LOGI(TAG, "main flow ready");

    while (1) {
        if (wait_for_wake_word() != ESP_OK) {
            ESP_LOGE(TAG, "wake stage failed");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (run_asr_session() != ESP_OK) {
            ESP_LOGW(TAG, "ASR session failed or timed out");
            strlcpy(g_reply_text, "我没听清，请再说一遍。", sizeof(g_reply_text));
        } else {
            build_reply_text();
        }

        ESP_LOGI(TAG, "reply text: %s", g_reply_text);
        if (speak_reply() != ESP_OK) {
            ESP_LOGE(TAG, "speak reply failed");
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        g_app_state = APP_STATE_WAIT_WAKE;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
