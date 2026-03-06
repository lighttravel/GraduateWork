#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "config.h"
#include "es8311_direct_tone.h"
#include "tts_module.h"
#include "wifi_manager.h"

static const char *TAG = "TTS_SMOKE";

#define EVENT_WIFI_CONNECTED BIT0
#define EVENT_TTS_DONE       BIT1
#define EVENT_TTS_ERROR      BIT2

#define TEST_TEXT "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C\xE5\x8D\x97\xE5\xBC\x80"
#define TTS_TASK_STACK_SIZE   (16 * 1024)
#define TTS_TASK_PRIORITY     6
#define TTS_PCM_CHUNK_MAX     16000
#define TTS_PCM_CHUNK_COUNT   8
#define TTS_PCM_QUEUE_WAIT_MS 200

typedef struct {
    uint8_t data[TTS_PCM_CHUNK_MAX];
    size_t len;
} tts_pcm_chunk_t;

static EventGroupHandle_t g_events;
static QueueHandle_t g_tts_free_queue;
static QueueHandle_t g_tts_ready_queue;
static tts_pcm_chunk_t *g_tts_chunks;
static size_t g_tts_chunk_count;
static size_t g_tts_byte_count;

static void flush_tts_pcm_queue(void)
{
    if (g_tts_ready_queue == NULL) {
        return;
    }

    int index = -1;
    while (xQueueReceive(g_tts_ready_queue, &index, 0) == pdTRUE) {
        if (g_tts_free_queue != NULL) {
            (void)xQueueSend(g_tts_free_queue, &index, 0);
        }
    }
}

static void wifi_event_callback(wifi_mgr_event_t event, void *user_data)
{
    (void)user_data;

    if (event == WIFI_MGR_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(g_events, EVENT_WIFI_CONNECTED);
    } else if (event == WIFI_MGR_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(g_events, EVENT_WIFI_CONNECTED);
    }
}

static esp_err_t init_nvs_storage(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
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
        EVENT_WIFI_CONNECTED,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(30000)
    );

    if ((bits & EVENT_WIFI_CONNECTED) == 0) {
        ESP_LOGE(TAG, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());

    // Improve DNS/TLS stability for outbound WebSocket calls.
    esp_err_t ps_ret = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ps_ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi power save disabled for TTS");
    } else {
        ESP_LOGW(TAG, "Failed to disable WiFi power save: %s", esp_err_to_name(ps_ret));
    }

    ESP_LOGI(TAG, "Use DHCP-provided DNS (no override)");

    return ESP_OK;
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

    ESP_LOGW(TAG, "SNTP timeout, TTS may fail");
    return ESP_ERR_TIMEOUT;
}

static void tts_event_callback(tts_event_t event, void *user_data)
{
    (void)user_data;

    switch (event) {
        case TTS_EVENT_START:
            ESP_LOGI(TAG, "TTS_EVENT_START");
            break;

        case TTS_EVENT_DONE:
            ESP_LOGI(TAG, "TTS_EVENT_DONE");
            xEventGroupSetBits(g_events, EVENT_TTS_DONE);
            break;

        case TTS_EVENT_ERROR:
            ESP_LOGE(TAG, "TTS_EVENT_ERROR");
            xEventGroupSetBits(g_events, EVENT_TTS_ERROR);
            break;

        default:
            break;
    }
}

static bool enqueue_tts_pcm_chunk(const uint8_t *data, size_t len)
{
    int index = -1;

    if (data == NULL || len == 0) {
        return true;
    }

    if (xQueueReceive(g_tts_free_queue, &index, pdMS_TO_TICKS(TTS_PCM_QUEUE_WAIT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "tts free queue empty");
        xEventGroupSetBits(g_events, EVENT_TTS_ERROR);
        return false;
    }

    g_tts_chunks[index].len = len;
    memcpy(g_tts_chunks[index].data, data, len);

    if (xQueueSend(g_tts_ready_queue, &index, pdMS_TO_TICKS(TTS_PCM_QUEUE_WAIT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "tts ready queue full");
        (void)xQueueSend(g_tts_free_queue, &index, 0);
        xEventGroupSetBits(g_events, EVENT_TTS_ERROR);
        return false;
    }

    g_tts_chunk_count++;
    g_tts_byte_count += len;
    if (g_tts_chunk_count == 1U || (g_tts_chunk_count % 10U) == 0U) {
        ESP_LOGI(TAG, "tts chunk=%u total_bytes=%u",
                 (unsigned)g_tts_chunk_count, (unsigned)g_tts_byte_count);
    }

    return true;
}

static void tts_data_callback(const uint8_t *data, size_t len, void *user_data)
{
    (void)user_data;

    if (data == NULL || len == 0) {
        return;
    }
    if (g_tts_free_queue == NULL || g_tts_ready_queue == NULL || g_tts_chunks == NULL) {
        xEventGroupSetBits(g_events, EVENT_TTS_ERROR);
        return;
    }

    for (size_t offset = 0; offset < len; offset += TTS_PCM_CHUNK_MAX) {
        size_t chunk_len = len - offset;
        if (chunk_len > TTS_PCM_CHUNK_MAX) {
            chunk_len = TTS_PCM_CHUNK_MAX;
        }

        if (!enqueue_tts_pcm_chunk(data + offset, chunk_len)) {
            return;
        }
    }
}

static esp_err_t run_tts_test_once(void)
{
    esp_err_t ret;
    EventBits_t bits;
    TickType_t deadline;
    bool done_seen = false;

    g_tts_chunk_count = 0;
    g_tts_byte_count = 0;
    xEventGroupClearBits(g_events, EVENT_TTS_DONE | EVENT_TTS_ERROR);

    g_tts_chunks = (tts_pcm_chunk_t *)heap_caps_malloc(
        sizeof(tts_pcm_chunk_t) * TTS_PCM_CHUNK_COUNT,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (g_tts_chunks == NULL) {
        g_tts_chunks = (tts_pcm_chunk_t *)malloc(sizeof(tts_pcm_chunk_t) * TTS_PCM_CHUNK_COUNT);
    }
    if (g_tts_chunks == NULL) {
        ESP_LOGE(TAG, "failed to alloc tts chunk pool");
        return ESP_ERR_NO_MEM;
    }

    g_tts_free_queue = xQueueCreate(TTS_PCM_CHUNK_COUNT, sizeof(int));
    g_tts_ready_queue = xQueueCreate(TTS_PCM_CHUNK_COUNT, sizeof(int));
    if (g_tts_free_queue == NULL || g_tts_ready_queue == NULL) {
        ESP_LOGE(TAG, "failed to create tts queues");
        if (g_tts_free_queue != NULL) {
            vQueueDelete(g_tts_free_queue);
        }
        if (g_tts_ready_queue != NULL) {
            vQueueDelete(g_tts_ready_queue);
        }
        free(g_tts_chunks);
        g_tts_free_queue = NULL;
        g_tts_ready_queue = NULL;
        g_tts_chunks = NULL;
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < TTS_PCM_CHUNK_COUNT; ++i) {
        (void)xQueueSend(g_tts_free_queue, &i, 0);
    }

    ret = ESP_FAIL;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        ret = es8311_direct_init_playback();
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "direct playback init failed (attempt %d/3): %s",
                 attempt, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "direct playback init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = tts_module_init(TTS_PROVIDER_IFLYTEK);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tts init failed: %s", esp_err_to_name(ret));
        es8311_direct_deinit_playback();
        return ret;
    }

    ESP_LOGI(TAG, "start speak: %s", TEST_TEXT);
    ret = tts_module_speak(TEST_TEXT, tts_data_callback, tts_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tts_module_speak failed: %s", esp_err_to_name(ret));
        flush_tts_pcm_queue();
        vQueueDelete(g_tts_free_queue);
        vQueueDelete(g_tts_ready_queue);
        free(g_tts_chunks);
        g_tts_free_queue = NULL;
        g_tts_ready_queue = NULL;
        g_tts_chunks = NULL;
        tts_module_deinit();
        es8311_direct_deinit_playback();
        return ret;
    }

    deadline = xTaskGetTickCount() + pdMS_TO_TICKS(90000);
    ret = ESP_ERR_TIMEOUT;

    while (1) {
        int index = -1;
        if (xQueueReceive(g_tts_ready_queue, &index, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (index >= 0 && index < TTS_PCM_CHUNK_COUNT && g_tts_chunks[index].len > 0U) {
                esp_err_t play_ret = es8311_direct_write_tts_pcm(g_tts_chunks[index].data, g_tts_chunks[index].len);
                g_tts_chunks[index].len = 0U;
                if (play_ret != ESP_OK) {
                    ESP_LOGE(TAG, "direct TTS playback failed: %s", esp_err_to_name(play_ret));
                    xEventGroupSetBits(g_events, EVENT_TTS_ERROR);
                }
            }
            (void)xQueueSend(g_tts_free_queue, &index, 0);
        }

        bits = xEventGroupGetBits(g_events);
        if ((bits & EVENT_TTS_ERROR) != 0) {
            ret = ESP_FAIL;
            break;
        }

        if ((bits & EVENT_TTS_DONE) != 0) {
            done_seen = true;
            if (uxQueueMessagesWaiting(g_tts_ready_queue) == 0) {
                ret = ESP_OK;
                break;
            }
        }

        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            ret = ESP_ERR_TIMEOUT;
            break;
        }
    }

    tts_module_stop();
    tts_module_deinit();
    flush_tts_pcm_queue();
    vQueueDelete(g_tts_free_queue);
    vQueueDelete(g_tts_ready_queue);
    free(g_tts_chunks);
    g_tts_free_queue = NULL;
    g_tts_ready_queue = NULL;
    g_tts_chunks = NULL;
    es8311_direct_deinit_playback();

    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "TTS_TEST_ERROR");
        return ESP_FAIL;
    }

    if (ret == ESP_OK && done_seen) {
        ESP_LOGI(TAG, "TTS_TEST_OK chunks=%u bytes=%u",
                 (unsigned)g_tts_chunk_count, (unsigned)g_tts_byte_count);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "TTS_TEST_TIMEOUT");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t run_tts_test(void)
{
    ESP_LOGI(TAG, "TTS test attempt 1/1");
    return run_tts_test_once();
}

static void tts_test_task(void *arg)
{
    (void)arg;
    esp_err_t ret = run_tts_test();
    ESP_LOGI(TAG, "tts task done: %s", esp_err_to_name(ret));
    vTaskDelete(NULL);
}

void app_main(void)
{
    bool wifi_ok = false;

    printf("\n========================================\n");
    printf(" TTS smoke test: tone -> WiFi -> SNTP -> TTS\n");
    printf(" Speak: %s\n", TEST_TEXT);
    printf("========================================\n");

    g_events = xEventGroupCreate();
    if (g_events == NULL) {
        ESP_LOGE(TAG, "failed to create event group");
        return;
    }

    ESP_ERROR_CHECK(init_nvs_storage());

    wifi_ok = (init_wifi_connection() == ESP_OK);
    ESP_LOGI(TAG, "WIFI_STATUS=%s", wifi_ok ? "CONNECTED" : "DISCONNECTED");

    if (wifi_ok) {
        (void)init_sntp_time();
        BaseType_t task_ok = xTaskCreate(
            tts_test_task,
            "tts_test_task",
            TTS_TASK_STACK_SIZE,
            NULL,
            TTS_TASK_PRIORITY,
            NULL
        );
        if (task_ok != pdPASS) {
            ESP_LOGE(TAG, "failed to create tts test task");
        }
    } else {
        ESP_LOGW(TAG, "Skipping TTS because WiFi is not connected");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
