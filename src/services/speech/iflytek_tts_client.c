#include "iflytek_tts_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_transport_ws.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"

static const char *TAG = "IFLYTEK_TTS";

#define WS_EVT_CONNECTED     BIT0
#define WS_EVT_ERROR         BIT1
#define WS_EVT_DISCONNECTED  BIT2

#define WS_CONNECT_TIMEOUT_MS    45000
#define WS_CONNECT_RETRY_PER_HOST 2
#define WS_SEND_TIMEOUT_TICKS    pdMS_TO_TICKS(5000)
#define WS_TASK_STACK            12288
#define WS_BUFFER_SIZE           4096
#define AUDIO_BUFFER_INIT_SIZE   (64 * 1024)
#define JSON_BUFFER_HARD_LIMIT   (64 * 1024)

typedef struct {
    iflytek_tts_config_t config;
    iflytek_tts_event_cb_t event_cb;
    void *user_data;

    esp_websocket_client_handle_t ws_client;

    iflytek_tts_state_t state;
    bool stop_requested;

    SemaphoreHandle_t mutex;
    EventGroupHandle_t ws_events;
    bool suppress_error_event;
    bool ws_continuation_is_text;

    uint8_t *audio_buffer;
    size_t audio_buffer_size;

    char *json_buffer;
    size_t json_buffer_size;
    size_t json_len;
} iflytek_tts_ctx_t;

static iflytek_tts_ctx_t *g_tts = NULL;

static void parse_tts_response(const char *json_str, size_t json_len);

static void log_tls_heap(const char *stage)
{
    ESP_LOGI(TAG,
             "heap %s: free=%u largest=%u internal_free=%u internal_largest=%u",
             stage,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

static uint8_t ws_opcode_normalize(int op_code)
{
    return (uint8_t)(op_code & 0x0F);
}

static void log_close_frame(const esp_websocket_event_data_t *data)
{
    if (data == NULL || data->data_ptr == NULL || data->data_len <= 0) {
        ESP_LOGW(TAG, "WebSocket close frame with empty payload");
        return;
    }

    const uint8_t *payload = (const uint8_t *)data->data_ptr;
    int close_code = 0;
    if (data->data_len >= 2) {
        close_code = ((int)payload[0] << 8) | (int)payload[1];
    }

    char reason[96];
    size_t reason_len = 0;
    if (data->data_len > 2) {
        reason_len = (size_t)(data->data_len - 2);
        if (reason_len > sizeof(reason) - 1U) {
            reason_len = sizeof(reason) - 1U;
        }
        for (size_t i = 0; i < reason_len; ++i) {
            unsigned char c = payload[i + 2];
            reason[i] = (c >= 32U && c <= 126U) ? (char)c : '.';
        }
    }
    reason[reason_len] = '\0';

    ESP_LOGE(TAG, "WebSocket close: code=%d payload_len=%d reason=%s",
             close_code, data->data_len, reason_len > 0 ? reason : "<none>");
}

static void get_rfc1123_date(char *date_str, size_t max_len)
{
    struct timeval tv;
    time_t now;
    struct tm timeinfo;

    gettimeofday(&tv, NULL);
    now = tv.tv_sec;
    gmtime_r(&now, &timeinfo);
    strftime(date_str, max_len, "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
}

static int hmac_sha256(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *output)
{
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        return -1;
    }
    return mbedtls_md_hmac(md_info, key, key_len, data, data_len, output);
}

static void base64_encode_standard(const uint8_t *src, size_t src_len, char *dst, size_t dst_len)
{
    size_t output_len = dst_len;
    (void)mbedtls_base64_encode((unsigned char *)dst, dst_len, &output_len, src, src_len);
    if (output_len < dst_len) {
        dst[output_len] = '\0';
    } else if (dst_len > 0) {
        dst[dst_len - 1] = '\0';
    }
}

static char *url_encode(const char *src)
{
    size_t len;
    char *encoded;
    size_t j = 0;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src);
    encoded = (char *)malloc(len * 3 + 1);
    if (encoded == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < len; ++i) {
        char c = src[i];
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[j++] = c;
        } else {
            snprintf(&encoded[j], 4, "%%%02X", (unsigned char)c);
            j += 3;
        }
    }

    encoded[j] = '\0';
    return encoded;
}

static esp_err_t ensure_audio_buffer(size_t needed)
{
    uint8_t *new_buf;
    size_t new_size;

    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (needed <= g_tts->audio_buffer_size) {
        return ESP_OK;
    }

    new_size = g_tts->audio_buffer_size;
    while (new_size < needed) {
        new_size *= 2;
    }

    new_buf = (uint8_t *)realloc(g_tts->audio_buffer, new_size);
    if (new_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    g_tts->audio_buffer = new_buf;
    g_tts->audio_buffer_size = new_size;
    return ESP_OK;
}

static esp_err_t ensure_json_buffer(size_t needed)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (needed <= g_tts->json_buffer_size) {
        return ESP_OK;
    }

    return ESP_ERR_NO_MEM;
}

static void emit_error_event(void)
{
    if (g_tts == NULL) {
        return;
    }
    g_tts->state = IFLYTEK_TTS_STATE_ERROR;
    if (g_tts->event_cb) {
        g_tts->event_cb(IFLYTEK_TTS_EVENT_ERROR, NULL, g_tts->user_data);
    }
}

static void reset_json_stream_buffer(void)
{
    if (g_tts == NULL || g_tts->json_buffer == NULL) {
        return;
    }
    g_tts->json_len = 0;
    g_tts->json_buffer[0] = '\0';
}

static void parse_tts_response(const char *json_str, size_t json_len)
{
    cJSON *root;
    cJSON *code_item;
    cJSON *data_obj;
    cJSON *audio_item;
    cJSON *status_item;
    int status_value = -1;

    if (g_tts == NULL || g_tts->stop_requested || json_str == NULL || json_str[0] != '{') {
        return;
    }

    root = cJSON_ParseWithLength(json_str, json_len);
    if (root == NULL) {
        size_t dump_len = json_len < 32U ? json_len : 32U;
        char hex_dump[32U * 3U + 1U];
        char ascii_dump[32U + 1U];
        size_t pos = 0;
        for (size_t i = 0; i < dump_len; ++i) {
            unsigned char c = (unsigned char)json_str[i];
            pos += (size_t)snprintf(hex_dump + pos, sizeof(hex_dump) - pos, "%02X ", c);
            ascii_dump[i] = (c >= 32U && c <= 126U) ? (char)c : '.';
        }
        hex_dump[pos < sizeof(hex_dump) ? pos : sizeof(hex_dump) - 1U] = '\0';
        ascii_dump[dump_len] = '\0';
        ESP_LOGW(TAG, "Invalid JSON frame len=%u hex=%s ascii=%s",
                 (unsigned)json_len, hex_dump, ascii_dump);
        return;
    }

    code_item = cJSON_GetObjectItem(root, "code");
    if (cJSON_IsNumber(code_item) && code_item->valueint != 0) {
        ESP_LOGE(TAG, "Server error code: %d", code_item->valueint);
        cJSON_Delete(root);
        emit_error_event();
        return;
    }

    data_obj = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsObject(data_obj)) {
        audio_item = cJSON_GetObjectItem(data_obj, "audio");
        status_item = cJSON_GetObjectItem(data_obj, "status");
    } else {
        audio_item = cJSON_GetObjectItem(root, "audio");
        status_item = cJSON_GetObjectItem(root, "status");
    }

    if (cJSON_IsString(audio_item) && audio_item->valuestring != NULL) {
        size_t audio_base64_len = strlen(audio_item->valuestring);
        size_t decoded_max = (audio_base64_len / 4U) * 3U + 4U;
        size_t actual_len = 0;

        if (ensure_audio_buffer(decoded_max) == ESP_OK) {
            int ret = mbedtls_base64_decode(
                g_tts->audio_buffer,
                g_tts->audio_buffer_size,
                &actual_len,
                (const unsigned char *)audio_item->valuestring,
                audio_base64_len
            );
            if (ret == 0 && actual_len > 0 && g_tts->event_cb && !g_tts->stop_requested) {
                iflytek_tts_audio_t audio_data = {
                    .audio_data = g_tts->audio_buffer,
                    .audio_len = actual_len,
                    .is_final = false,
                };
                g_tts->event_cb(IFLYTEK_TTS_EVENT_AUDIO_DATA, &audio_data, g_tts->user_data);
            } else if (ret != 0) {
                ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
            }
        } else {
            ESP_LOGE(TAG, "Failed to expand audio buffer");
        }
    }

    if (cJSON_IsNumber(status_item)) {
        status_value = status_item->valueint;
    }

    if (status_value == 2) {
        g_tts->state = IFLYTEK_TTS_STATE_CONNECTED;
        if (g_tts->event_cb && !g_tts->stop_requested) {
            iflytek_tts_audio_t audio_data = {
                .audio_data = NULL,
                .audio_len = 0,
                .is_final = true,
            };
            g_tts->event_cb(IFLYTEK_TTS_EVENT_AUDIO_DATA, &audio_data, g_tts->user_data);
            g_tts->event_cb(IFLYTEK_TTS_EVENT_COMPLETE, NULL, g_tts->user_data);
        }
    }

    cJSON_Delete(root);
}

static void process_json_stream_buffer(void)
{
    if (g_tts == NULL || g_tts->json_buffer == NULL || g_tts->json_len == 0) {
        return;
    }

    while (g_tts->json_len > 0) {
        size_t start = 0;
        while (start < g_tts->json_len && g_tts->json_buffer[start] != '{') {
            ++start;
        }

        if (start >= g_tts->json_len) {
            reset_json_stream_buffer();
            return;
        }

        if (start > 0) {
            memmove(g_tts->json_buffer, g_tts->json_buffer + start, g_tts->json_len - start);
            g_tts->json_len -= start;
            g_tts->json_buffer[g_tts->json_len] = '\0';
        }

        bool in_string = false;
        bool escaped = false;
        int depth = 0;
        bool found_object = false;
        size_t object_end = 0;

        for (size_t i = 0; i < g_tts->json_len; ++i) {
            char c = g_tts->json_buffer[i];

            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }

            if (c == '"') {
                in_string = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    found_object = true;
                    object_end = i + 1;
                    break;
                }
                if (depth < 0) {
                    reset_json_stream_buffer();
                    return;
                }
            }
        }

        if (!found_object) {
            if (g_tts->json_len > JSON_BUFFER_HARD_LIMIT) {
                ESP_LOGE(TAG, "JSON stream buffer overflow");
                emit_error_event();
                reset_json_stream_buffer();
            }
            return;
        }

        parse_tts_response(g_tts->json_buffer, object_end);

        size_t tail = g_tts->json_len - object_end;
        if (tail > 0) {
            memmove(g_tts->json_buffer, g_tts->json_buffer + object_end, tail);
        }
        g_tts->json_len = tail;
        g_tts->json_buffer[g_tts->json_len] = '\0';
    }
}

static void ws_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    (void)handler_args;
    (void)base;

    if (g_tts == NULL) {
        return;
    }

    switch ((esp_websocket_event_id_t)event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            g_tts->state = IFLYTEK_TTS_STATE_CONNECTED;
            reset_json_stream_buffer();
            g_tts->ws_continuation_is_text = false;
            xEventGroupSetBits(g_tts->ws_events, WS_EVT_CONNECTED);
            if (g_tts->event_cb) {
                g_tts->event_cb(IFLYTEK_TTS_EVENT_CONNECTED, NULL, g_tts->user_data);
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            xEventGroupSetBits(g_tts->ws_events, WS_EVT_DISCONNECTED);
            if (!g_tts->stop_requested && !g_tts->suppress_error_event &&
                g_tts->state == IFLYTEK_TTS_STATE_SYNTHESIZING) {
                emit_error_event();
            } else if (!g_tts->stop_requested && !g_tts->suppress_error_event) {
                g_tts->state = IFLYTEK_TTS_STATE_ERROR;
            }
            if (g_tts->event_cb && !g_tts->suppress_error_event) {
                g_tts->event_cb(IFLYTEK_TTS_EVENT_DISCONNECTED, NULL, g_tts->user_data);
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error: type=%d tls_err=%d hs_code=%d sock_errno=%d",
                     data ? data->error_handle.error_type : -1,
                     data ? data->error_handle.esp_tls_last_esp_err : 0,
                     data ? data->error_handle.esp_ws_handshake_status_code : 0,
                     data ? data->error_handle.esp_transport_sock_errno : 0);
            xEventGroupSetBits(g_tts->ws_events, WS_EVT_ERROR);
            if (!g_tts->suppress_error_event) {
                emit_error_event();
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data == NULL || data->data_ptr == NULL || data->data_len <= 0) {
                break;
            }

            bool treat_as_text = false;
            bool treat_as_binary = false;
            uint8_t opcode = ws_opcode_normalize(data->op_code);

            if (opcode == WS_TRANSPORT_OPCODES_TEXT) {
                g_tts->ws_continuation_is_text = true;
                treat_as_text = true;
            } else if (opcode == WS_TRANSPORT_OPCODES_BINARY) {
                g_tts->ws_continuation_is_text = false;
                treat_as_binary = true;
            } else if (opcode == WS_TRANSPORT_OPCODES_CONT) {
                if (g_tts->ws_continuation_is_text) {
                    treat_as_text = true;
                } else {
                    treat_as_binary = true;
                }
            } else if (opcode == WS_TRANSPORT_OPCODES_CLOSE) {
                log_close_frame(data);
                xEventGroupSetBits(g_tts->ws_events, WS_EVT_ERROR | WS_EVT_DISCONNECTED);
                if (!g_tts->suppress_error_event) {
                    emit_error_event();
                }
                break;
            } else if (opcode == WS_TRANSPORT_OPCODES_PING || opcode == WS_TRANSPORT_OPCODES_PONG) {
                break;
            }

            if (treat_as_text) {
                size_t required = g_tts->json_len + (size_t)data->data_len + 1U;

                if (ensure_json_buffer(required) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to expand JSON buffer");
                    emit_error_event();
                    break;
                }

                memcpy(g_tts->json_buffer + g_tts->json_len, data->data_ptr, (size_t)data->data_len);
                g_tts->json_len += (size_t)data->data_len;
                g_tts->json_buffer[g_tts->json_len] = '\0';
                process_json_stream_buffer();
            } else if (treat_as_binary) {
                if (g_tts->event_cb && !g_tts->stop_requested) {
                    iflytek_tts_audio_t audio_data = {
                        .audio_data = (const uint8_t *)data->data_ptr,
                        .audio_len = (size_t)data->data_len,
                        .is_final = false,
                    };
                    g_tts->event_cb(IFLYTEK_TTS_EVENT_AUDIO_DATA, &audio_data, g_tts->user_data);
                }
            }
            break;

        default:
            break;
    }
}

static void disconnect_from_server(void)
{
    if (g_tts == NULL || g_tts->ws_client == NULL) {
        return;
    }

    (void)esp_websocket_unregister_events(g_tts->ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler);
    (void)esp_websocket_client_stop(g_tts->ws_client);
    (void)esp_websocket_client_destroy(g_tts->ws_client);
    g_tts->ws_client = NULL;
    g_tts->state = IFLYTEK_TTS_STATE_IDLE;
}

static esp_err_t connect_to_host(const char *host)
{
    char date[64];
    char signature_origin[256];
    uint8_t hmac_result[32];
    char signature_base64[64];
    char authorization_origin[256];
    char authorization[256];
    size_t auth_output_len = sizeof(authorization);
    char *auth_encoded = NULL;
    char *date_encoded = NULL;
    char *host_encoded = NULL;
    char *uri = NULL;
    esp_websocket_client_config_t ws_cfg = {0};
    EventBits_t bits;
    esp_err_t ret = ESP_FAIL;

    if (g_tts == NULL || host == NULL || host[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    get_rfc1123_date(date, sizeof(date));
    snprintf(signature_origin, sizeof(signature_origin),
             "host: %s\ndate: %s\nGET /v2/tts HTTP/1.1",
             host, date);

    if (hmac_sha256((const uint8_t *)g_tts->config.api_secret,
                    strlen(g_tts->config.api_secret),
                    (const uint8_t *)signature_origin,
                    strlen(signature_origin),
                    hmac_result) != 0) {
        ESP_LOGE(TAG, "HMAC-SHA256 failed");
        return ESP_FAIL;
    }

    base64_encode_standard(hmac_result, sizeof(hmac_result), signature_base64, sizeof(signature_base64));

    snprintf(authorization_origin, sizeof(authorization_origin),
             "api_key=\"%s\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%s\"",
             g_tts->config.api_key, signature_base64);

    (void)mbedtls_base64_encode((unsigned char *)authorization, sizeof(authorization), &auth_output_len,
                                (const unsigned char *)authorization_origin, strlen(authorization_origin));
    if (auth_output_len >= sizeof(authorization)) {
        return ESP_FAIL;
    }
    authorization[auth_output_len] = '\0';

    auth_encoded = url_encode(authorization);
    date_encoded = url_encode(date);
    host_encoded = url_encode(host);
    if (!auth_encoded || !date_encoded || !host_encoded) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    uri = (char *)malloc(1024);
    if (!uri) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    snprintf(uri, 1024,
             "wss://%s%s?authorization=%s&date=%s&host=%s",
             host,
             IFLYTEK_TTS_PATH,
             auth_encoded,
             date_encoded,
             host_encoded);

    ESP_LOGI(TAG, "Connecting websocket: %s", host);
    log_tls_heap("before_ws_connect");

    ws_cfg.uri = uri;
    ws_cfg.host = host;
    ws_cfg.port = IFLYTEK_TTS_PORT;
    ws_cfg.disable_auto_reconnect = true;
    ws_cfg.task_prio = 5;
    ws_cfg.task_stack = WS_TASK_STACK;
    ws_cfg.buffer_size = WS_BUFFER_SIZE;
    ws_cfg.network_timeout_ms = WS_CONNECT_TIMEOUT_MS;
    ws_cfg.reconnect_timeout_ms = 0;
    ws_cfg.ping_interval_sec = 10;
    ws_cfg.user_agent = "ESP32-iFlytek-TTS/1.0";
    ws_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    ws_cfg.cert_common_name = host;
    ws_cfg.skip_cert_common_name_check = false;

    g_tts->ws_client = esp_websocket_client_init(&ws_cfg);
    if (g_tts->ws_client == NULL) {
        ESP_LOGE(TAG, "esp_websocket_client_init failed");
        ret = ESP_FAIL;
        goto cleanup;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_register_events(
        g_tts->ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL));

    xEventGroupClearBits(g_tts->ws_events, WS_EVT_CONNECTED | WS_EVT_ERROR | WS_EVT_DISCONNECTED);
    if (esp_websocket_client_start(g_tts->ws_client) != ESP_OK) {
        ESP_LOGE(TAG, "esp_websocket_client_start failed");
        ret = ESP_FAIL;
        goto cleanup;
    }

    bits = xEventGroupWaitBits(g_tts->ws_events,
                               WS_EVT_CONNECTED | WS_EVT_ERROR | WS_EVT_DISCONNECTED,
                               pdFALSE,
                               pdFALSE,
                               pdMS_TO_TICKS(WS_CONNECT_TIMEOUT_MS));

    if (bits & WS_EVT_CONNECTED) {
        g_tts->state = IFLYTEK_TTS_STATE_CONNECTED;
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "WebSocket connect timeout/failure on %s (bits=0x%02X)", host, (unsigned)bits);
        ret = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    if (ret != ESP_OK) {
        log_tls_heap("after_ws_connect_fail");
        disconnect_from_server();
    }

    if (uri) {
        free(uri);
    }
    if (auth_encoded) {
        free(auth_encoded);
    }
    if (date_encoded) {
        free(date_encoded);
    }
    if (host_encoded) {
        free(host_encoded);
    }

    return ret;
}

static esp_err_t connect_to_server(void)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_tts->ws_client && esp_websocket_client_is_connected(g_tts->ws_client)) {
        g_tts->state = IFLYTEK_TTS_STATE_CONNECTED;
        return ESP_OK;
    }

    g_tts->suppress_error_event = true;
    for (int attempt = 1; attempt <= WS_CONNECT_RETRY_PER_HOST; ++attempt) {
        disconnect_from_server();
        ESP_LOGI(TAG, "Host connect attempt %d/%d: %s",
                 attempt, WS_CONNECT_RETRY_PER_HOST, IFLYTEK_TTS_HOST);

        esp_err_t ret = connect_to_host(IFLYTEK_TTS_HOST);
        if (ret == ESP_OK) {
            g_tts->suppress_error_event = false;
            return ESP_OK;
        }

        if (attempt < WS_CONNECT_RETRY_PER_HOST) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    g_tts->suppress_error_event = false;
    return ESP_FAIL;
}

esp_err_t iflytek_tts_init(const iflytek_tts_config_t *config,
                           iflytek_tts_event_cb_t event_cb,
                           void *user_data)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_tts != NULL) {
        return ESP_OK;
    }

    g_tts = (iflytek_tts_ctx_t *)calloc(1, sizeof(iflytek_tts_ctx_t));
    if (!g_tts) {
        return ESP_ERR_NO_MEM;
    }

    g_tts->mutex = xSemaphoreCreateMutex();
    g_tts->ws_events = xEventGroupCreate();
    g_tts->audio_buffer = (uint8_t *)heap_caps_malloc(AUDIO_BUFFER_INIT_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (g_tts->audio_buffer == NULL) {
        g_tts->audio_buffer = (uint8_t *)malloc(AUDIO_BUFFER_INIT_SIZE);
    }
    g_tts->json_buffer = (char *)heap_caps_malloc(JSON_BUFFER_HARD_LIMIT, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (g_tts->json_buffer == NULL) {
        g_tts->json_buffer = (char *)malloc(JSON_BUFFER_HARD_LIMIT);
    }

    if (g_tts->mutex == NULL || g_tts->ws_events == NULL ||
        g_tts->audio_buffer == NULL || g_tts->json_buffer == NULL) {
        iflytek_tts_deinit();
        return ESP_ERR_NO_MEM;
    }

    g_tts->audio_buffer_size = AUDIO_BUFFER_INIT_SIZE;
    g_tts->json_buffer_size = JSON_BUFFER_HARD_LIMIT;
    g_tts->json_len = 0;

    strlcpy(g_tts->config.appid, config->appid, sizeof(g_tts->config.appid));
    strlcpy(g_tts->config.api_key, config->api_key, sizeof(g_tts->config.api_key));
    strlcpy(g_tts->config.api_secret, config->api_secret, sizeof(g_tts->config.api_secret));
    strlcpy(g_tts->config.voice_name, config->voice_name, sizeof(g_tts->config.voice_name));
    g_tts->config.sample_rate = config->sample_rate;
    g_tts->config.speed = config->speed;
    g_tts->config.volume = config->volume;
    g_tts->config.pitch = config->pitch;

    g_tts->event_cb = event_cb;
    g_tts->user_data = user_data;
    g_tts->state = IFLYTEK_TTS_STATE_IDLE;
    g_tts->stop_requested = false;
    g_tts->suppress_error_event = false;
    g_tts->ws_continuation_is_text = false;

    ESP_LOGI(TAG, "iFlytek TTS initialized");
    return ESP_OK;
}

esp_err_t iflytek_tts_deinit(void)
{
    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    iflytek_tts_stop();
    disconnect_from_server();

    if (g_tts->json_buffer) {
        free(g_tts->json_buffer);
    }
    if (g_tts->audio_buffer) {
        free(g_tts->audio_buffer);
    }
    if (g_tts->ws_events) {
        vEventGroupDelete(g_tts->ws_events);
    }
    if (g_tts->mutex) {
        vSemaphoreDelete(g_tts->mutex);
    }

    free(g_tts);
    g_tts = NULL;
    return ESP_OK;
}

esp_err_t iflytek_tts_synthesize(const char *text)
{
    cJSON *root = NULL;
    cJSON *common = NULL;
    cJSON *business = NULL;
    cJSON *data = NULL;
    char *text_base64 = NULL;
    char *json_str = NULL;
    size_t text_len;
    size_t text_base64_size;
    size_t text_base64_len = 0;
    int send_ret;

    if (g_tts == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (text == NULL || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_tts->mutex, portMAX_DELAY);
    if (g_tts->state == IFLYTEK_TTS_STATE_SYNTHESIZING) {
        xSemaphoreGive(g_tts->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    g_tts->stop_requested = false;
    xSemaphoreGive(g_tts->mutex);

    if (connect_to_server() != ESP_OK) {
        emit_error_event();
        return ESP_FAIL;
    }

    g_tts->state = IFLYTEK_TTS_STATE_SYNTHESIZING;
    if (g_tts->event_cb) {
        g_tts->event_cb(IFLYTEK_TTS_EVENT_SYNTHESIZING, NULL, g_tts->user_data);
    }

    text_len = strlen(text);
    text_base64_size = ((text_len + 2U) / 3U) * 4U + 8U;
    text_base64 = (char *)malloc(text_base64_size);
    if (text_base64 == NULL) {
        emit_error_event();
        return ESP_ERR_NO_MEM;
    }

    (void)mbedtls_base64_encode((unsigned char *)text_base64, text_base64_size, &text_base64_len,
                                (const unsigned char *)text, text_len);
    text_base64[text_base64_len] = '\0';

    root = cJSON_CreateObject();
    common = cJSON_CreateObject();
    business = cJSON_CreateObject();
    data = cJSON_CreateObject();

    if (!root || !common || !business || !data) {
        free(text_base64);
        if (root) {
            cJSON_Delete(root);
        }
        emit_error_event();
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(common, "app_id", g_tts->config.appid);
    cJSON_AddItemToObject(root, "common", common);

    char auf_value[32];
    uint32_t sample_rate = (g_tts->config.sample_rate == 0U) ? 16000U : g_tts->config.sample_rate;
    (void)snprintf(auf_value, sizeof(auf_value), "audio/L16;rate=%lu", (unsigned long)sample_rate);

    cJSON_AddStringToObject(business, "aue", "raw");
    cJSON_AddStringToObject(business, "auf", auf_value);
    cJSON_AddStringToObject(business, "vcn", g_tts->config.voice_name);
    cJSON_AddStringToObject(business, "tte", "UTF8");
    cJSON_AddNumberToObject(business, "speed", g_tts->config.speed);
    cJSON_AddNumberToObject(business, "volume", g_tts->config.volume);
    cJSON_AddNumberToObject(business, "pitch", g_tts->config.pitch);
    cJSON_AddItemToObject(root, "business", business);

    cJSON_AddNumberToObject(data, "status", 2);
    cJSON_AddStringToObject(data, "text", text_base64);
    cJSON_AddItemToObject(root, "data", data);

    json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(text_base64);

    if (json_str == NULL) {
        emit_error_event();
        return ESP_ERR_NO_MEM;
    }

    send_ret = esp_websocket_client_send_text(g_tts->ws_client, json_str, (int)strlen(json_str), WS_SEND_TIMEOUT_TICKS);
    free(json_str);

    if (send_ret <= 0) {
        ESP_LOGE(TAG, "Failed to send TTS request");
        emit_error_event();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TTS request sent");
    return ESP_OK;
}

esp_err_t iflytek_tts_stop(void)
{
    if (g_tts == NULL) {
        return ESP_OK;
    }

    g_tts->stop_requested = true;
    reset_json_stream_buffer();
    disconnect_from_server();
    g_tts->state = IFLYTEK_TTS_STATE_IDLE;
    return ESP_OK;
}

iflytek_tts_state_t iflytek_tts_get_state(void)
{
    return g_tts ? g_tts->state : IFLYTEK_TTS_STATE_IDLE;
}

bool iflytek_tts_is_synthesizing(void)
{
    return g_tts && g_tts->state == IFLYTEK_TTS_STATE_SYNTHESIZING;
}

bool iflytek_tts_is_connected(void)
{
    return g_tts && g_tts->ws_client && esp_websocket_client_is_connected(g_tts->ws_client);
}
