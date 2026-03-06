/**
 * @file iflytek_asr.c
 * @brief 科大讯飞语音听写ASR模块实现
 *
 * 功能:
 * - WebSocket连接和握手
 * - 音频数据发送
 * - 识别结果接收和解析
 * - 事件回调通知
 * - FreeRTOS接收任务
 *
 * 参考文档: https://www.xfyun.cn/doc/asr/voicedictation/API.html
 */

#include "iflytek_asr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// mbedTLS用于加密
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"

static const char *TAG = "IFLYTEK";
static const char *TLS_LOG_TAG = "esp-tls-mbedtls";
static const char *MBEDTLS_DYNAMIC_LOG_TAG = "Dynamic Impl";

// ==================== 内部配置 ====================

#define RECV_TASK_STACK_SIZE    4096
#define RECV_TASK_PRIORITY      5
#define RECV_BUF_SIZE           2048
#define HANDSHAKE_BUF_SIZE      2048
#define MAX_RESULT_TEXT_LEN     512
#define MAX_RESULT_SEGMENTS     32
#define MAX_SEGMENT_TEXT_LEN    64

// ==================== 全局上下文 ====================

typedef struct {
    iflytek_asr_config_t config;
    iflytek_asr_event_cb_t event_cb;
    void *user_data;

    // TLS连接 (用于WebSocket)
    esp_tls_t *tls_conn;

    // 状态
    iflytek_asr_state_t state;
    bool is_first_frame;
    bool handshake_complete;

    // 接收任务
    TaskHandle_t recv_task;
    volatile bool recv_task_running;
    volatile bool disconnect_requested;
    SemaphoreHandle_t conn_mutex;

    // 统计
    uint32_t frames_sent;
    uint32_t total_bytes_sent;
    char segment_text[MAX_RESULT_SEGMENTS][MAX_SEGMENT_TEXT_LEN];
    int max_segment_index;

} iflytek_asr_ctx_t;

static iflytek_asr_ctx_t *g_ctx = NULL;

static void destroy_tls_connection(void)
{
    if (g_ctx == NULL || g_ctx->tls_conn == NULL) {
        return;
    }

    esp_tls_conn_destroy(g_ctx->tls_conn);
    g_ctx->tls_conn = NULL;
    g_ctx->handshake_complete = false;
}

static void shutdown_tls_socket(void)
{
    int sockfd = -1;

    if (g_ctx == NULL || g_ctx->tls_conn == NULL) {
        return;
    }

    if (esp_tls_get_conn_sockfd(g_ctx->tls_conn, &sockfd) == ESP_OK && sockfd >= 0) {
        shutdown(sockfd, SHUT_RDWR);
    }
}

static void suppress_disconnect_logs(esp_log_level_t *tls_level, esp_log_level_t *dynamic_level)
{
    if (tls_level != NULL) {
        *tls_level = esp_log_level_get(TLS_LOG_TAG);
        esp_log_level_set(TLS_LOG_TAG, ESP_LOG_NONE);
    }

    if (dynamic_level != NULL) {
        *dynamic_level = esp_log_level_get(MBEDTLS_DYNAMIC_LOG_TAG);
        esp_log_level_set(MBEDTLS_DYNAMIC_LOG_TAG, ESP_LOG_NONE);
    }
}

static void restore_disconnect_logs(esp_log_level_t tls_level, esp_log_level_t dynamic_level)
{
    esp_log_level_set(TLS_LOG_TAG, tls_level);
    esp_log_level_set(MBEDTLS_DYNAMIC_LOG_TAG, dynamic_level);
}

static void reset_result_segments(void)
{
    if (g_ctx == NULL) {
        return;
    }

    memset(g_ctx->segment_text, 0, sizeof(g_ctx->segment_text));
    g_ctx->max_segment_index = -1;
}

static void rebuild_result_text(char *out_text, size_t out_len)
{
    size_t offset = 0;

    if (out_text == NULL || out_len == 0) {
        return;
    }

    out_text[0] = '\0';
    if (g_ctx == NULL) {
        return;
    }

    for (int i = 0; i <= g_ctx->max_segment_index && i < MAX_RESULT_SEGMENTS; ++i) {
        size_t segment_len = strnlen(g_ctx->segment_text[i], MAX_SEGMENT_TEXT_LEN);
        if (segment_len == 0) {
            continue;
        }

        if (offset + segment_len >= out_len) {
            segment_len = out_len - offset - 1U;
        }
        if (segment_len == 0) {
            break;
        }

        memcpy(out_text + offset, g_ctx->segment_text[i], segment_len);
        offset += segment_len;
        out_text[offset] = '\0';
    }
}

// ==================== 辅助函数 ====================

static void get_rfc1123_date(char *date_str, size_t max_len)
{
    time_t now;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    now = tv.tv_sec;

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    strftime(date_str, max_len, "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
}

static int hmac_sha256(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *output)
{
    const mbedtls_md_info_t *md_info;
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        ESP_LOGE(TAG, "Failed to get SHA256 info");
        return -1;
    }

    int ret = mbedtls_md_hmac(md_info, key, key_len, data, data_len, output);
    if (ret != 0) {
        ESP_LOGE(TAG, "HMAC-SHA256 calculation failed: %d", ret);
        return -1;
    }

    return 0;
}

// 标准base64编码（保留+和/和=填充）
static void base64_encode_standard(const uint8_t *src, size_t src_len, char *dst, size_t dst_len)
{
    size_t output_len = dst_len;
    mbedtls_base64_encode((unsigned char *)dst, dst_len, &output_len, src, src_len);
    dst[output_len] = '\0';
}

static char* url_encode(const char *src)
{
    if (!src) return NULL;

    size_t len = strlen(src);
    char *encoded = (char *)malloc(len * 3 + 1);
    if (!encoded) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[j++] = c;
        } else {
            snprintf(&encoded[j], 4, "%%%02X", (unsigned char)c);
            j += 3;
        }
    }
    encoded[j] = '\0';

    return encoded;
}

static int read_http_headers(esp_tls_t *tls, char *buffer, size_t buffer_len)
{
    size_t total = 0;

    if (tls == NULL || buffer == NULL || buffer_len < 4) {
        return -1;
    }

    while (total + 1 < buffer_len) {
        int ret = esp_tls_conn_read(tls, (unsigned char *)buffer + total, buffer_len - total - 1);
        if (ret <= 0) {
            return ret;
        }

        total += (size_t)ret;
        buffer[total] = '\0';

        if (strstr(buffer, "\r\n\r\n") != NULL) {
            return (int)total;
        }
    }

    return -1;
}

// ==================== WebSocket帧处理 ====================

// WebSocket帧编码（客户端发送需要mask）
static ssize_t encode_ws_frame(const uint8_t *data, size_t data_len,
                                uint8_t opcode, uint8_t *out_buf, size_t out_buf_len)
{
    if (!data || !out_buf) return -1;

    size_t header_len = 2;
    if (data_len < 126) {
        header_len = 2;
    } else if (data_len < 65536) {
        header_len = 4;
    } else {
        header_len = 10;
    }

    if (header_len + 4 + data_len > out_buf_len) {
        return -1;
    }

    uint8_t *buf = out_buf;

    // 第一个字节：FIN + opcode
    buf[0] = 0x80 | opcode;

    // 第二个字节：MASK + payload length
    if (data_len < 126) {
        buf[1] = (uint8_t)data_len | 0x80;
    } else if (data_len < 65536) {
        buf[1] = 126 | 0x80;
        buf[2] = (data_len >> 8) & 0xFF;
        buf[3] = data_len & 0xFF;
    } else {
        buf[1] = 127 | 0x80;
        buf[2] = 0; buf[3] = 0; buf[4] = 0; buf[5] = 0;
        buf[6] = (data_len >> 24) & 0xFF;
        buf[7] = (data_len >> 16) & 0xFF;
        buf[8] = (data_len >> 8) & 0xFF;
        buf[9] = data_len & 0xFF;
    }

    // Masking key
    uint8_t masking_key[4];
    esp_fill_random(masking_key, 4);
    memcpy(buf + header_len, masking_key, 4);

    // 应用mask并复制payload
    for (size_t i = 0; i < data_len; i++) {
        buf[header_len + 4 + i] = data[i] ^ masking_key[i % 4];
    }

    return header_len + 4 + data_len;
}

// WebSocket帧解码（服务器发送无mask）
static ssize_t decode_ws_frame(const uint8_t *frame_data, size_t frame_len,
                                uint8_t *opcode, uint8_t *payload, size_t payload_buf_len)
{
    if (!frame_data || frame_len < 2) return -1;

    // 解析帧头
    uint8_t op = frame_data[0] & 0x0F;
    bool masked = (frame_data[1] & 0x80) != 0;
    uint64_t payload_len = frame_data[1] & 0x7F;

    size_t header_len = 2;
    uint8_t *masking_key = NULL;

    if (payload_len == 126) {
        if (frame_len < 4) return -1;
        payload_len = ((uint16_t)frame_data[2] << 8) | frame_data[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (frame_len < 10) return -1;
        payload_len = ((uint64_t)frame_data[6] << 24) | ((uint64_t)frame_data[7] << 16) |
                      ((uint64_t)frame_data[8] << 8) | frame_data[9];
        header_len = 10;
    }

    if (masked) {
        masking_key = (uint8_t *)(frame_data + header_len);
        header_len += 4;
    }

    if (header_len + payload_len > frame_len) {
        return -1;  // 数据不完整
    }

    if (payload_len > payload_buf_len) {
        ESP_LOGW(TAG, "Payload too large: %llu > %zu", payload_len, payload_buf_len);
        payload_len = payload_buf_len;  // 截断
    }

    if (opcode) *opcode = op;

    // 复制并解密payload
    const uint8_t *src_payload = frame_data + header_len;
    if (masked && masking_key) {
        for (size_t i = 0; i < payload_len; i++) {
            payload[i] = src_payload[i] ^ masking_key[i % 4];
        }
    } else {
        memcpy(payload, src_payload, payload_len);
    }

    return (ssize_t)payload_len;
}

// ==================== ASR结果解析 ====================

static void parse_and_notify_result(const char *json_str)
{
    if (!g_ctx || !g_ctx->event_cb || !json_str) return;

    ESP_LOGI(TAG, "Parsing result: %s", json_str);

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    // 检查code字段
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code && cJSON_IsNumber(code)) {
        int code_val = code->valueint;
        if (code_val != 0) {
            cJSON *message = cJSON_GetObjectItem(root, "message");
            ESP_LOGE(TAG, "Server error: code=%d, message=%s",
                     code_val, message ? message->valuestring : "unknown");

            // 触发错误事件
            if (g_ctx->event_cb) {
                g_ctx->event_cb(IFLYTEK_ASR_EVENT_ERROR, NULL, g_ctx->user_data);
            }

            cJSON_Delete(root);
            return;
        }
    }

    // 获取sid
    cJSON *sid = cJSON_GetObjectItem(root, "sid");
    if (sid) {
        ESP_LOGI(TAG, "Session ID: %s", sid->valuestring);
    }

    // 解析data字段
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        cJSON_Delete(root);
        return;
    }

    cJSON *result = cJSON_GetObjectItem(data, "result");
    if (!result) {
        cJSON_Delete(root);
        return;
    }

    // 创建结果结构
    iflytek_asr_result_t asr_result;
    memset(&asr_result, 0, sizeof(asr_result));
    asr_result.is_final = false;

    // 检查是否是最后结果
    cJSON *status = cJSON_GetObjectItem(data, "status");
    if (status && cJSON_IsNumber(status)) {
        asr_result.is_final = (status->valueint == 2);
    }

    cJSON *pgs = cJSON_GetObjectItem(result, "pgs");
    cJSON *rg = cJSON_GetObjectItem(result, "rg");
    int segment_index = -1;
    cJSON *sn = cJSON_GetObjectItem(result, "sn");
    if (sn && cJSON_IsNumber(sn)) {
        segment_index = sn->valueint;
    }

    // 解析ws (word segments)
    cJSON *ws = cJSON_GetObjectItem(result, "ws");
    if (ws && cJSON_IsArray(ws)) {
        int ws_count = cJSON_GetArraySize(ws);
        char segment_text[MAX_SEGMENT_TEXT_LEN] = {0};
        int text_offset = 0;

        for (int i = 0; i < ws_count && text_offset < MAX_RESULT_TEXT_LEN - 10; i++) {
            cJSON *ws_item = cJSON_GetArrayItem(ws, i);
            if (!ws_item) continue;

            cJSON *cw = cJSON_GetObjectItem(ws_item, "cw");
            if (cw && cJSON_IsArray(cw)) {
                int cw_count = cJSON_GetArraySize(cw);
                for (int j = 0; j < cw_count && text_offset < MAX_RESULT_TEXT_LEN - 10; j++) {
                    cJSON *cw_item = cJSON_GetArrayItem(cw, j);
                    if (!cw_item) continue;

                    cJSON *w = cJSON_GetObjectItem(cw_item, "w");
                    if (w && cJSON_IsString(w)) {
                        int w_len = strlen(w->valuestring);
                        if (text_offset + w_len < (int)sizeof(segment_text) - 1) {
                            strcpy(&segment_text[text_offset], w->valuestring);
                            text_offset += w_len;
                        }
                    }
                }
            }
        }
        segment_text[text_offset] = '\0';

        if (segment_index >= 0 && segment_index < MAX_RESULT_SEGMENTS) {
            if (cJSON_IsString(pgs) && strcmp(pgs->valuestring, "rpl") == 0 &&
                cJSON_IsArray(rg) && cJSON_GetArraySize(rg) >= 2) {
                cJSON *rg_start = cJSON_GetArrayItem(rg, 0);
                cJSON *rg_end = cJSON_GetArrayItem(rg, 1);
                if (cJSON_IsNumber(rg_start) && cJSON_IsNumber(rg_end)) {
                    int start = rg_start->valueint;
                    int end = rg_end->valueint;
                    if (start < 0) {
                        start = 0;
                    }
                    if (end >= MAX_RESULT_SEGMENTS) {
                        end = MAX_RESULT_SEGMENTS - 1;
                    }
                    for (int i = start; i <= end; ++i) {
                        g_ctx->segment_text[i][0] = '\0';
                    }
                }
            }

            strlcpy(g_ctx->segment_text[segment_index],
                    segment_text,
                    sizeof(g_ctx->segment_text[segment_index]));
            if (segment_index > g_ctx->max_segment_index) {
                g_ctx->max_segment_index = segment_index;
            }
            rebuild_result_text(asr_result.text, sizeof(asr_result.text));
        } else {
            strlcpy(asr_result.text, segment_text, sizeof(asr_result.text));
        }
    }

    // 检查动态修正标记
    cJSON *ls = cJSON_GetObjectItem(result, "ls");
    if (ls && cJSON_IsBool(ls) && cJSON_IsTrue(ls)) {
        asr_result.is_final = true;
    }

    ESP_LOGI(TAG, "ASR Result: \"%s\" (final=%d)", asr_result.text, asr_result.is_final);

    // Always propagate a final result event, even if the recognized text is empty.
    if (g_ctx->event_cb && (asr_result.is_final || strlen(asr_result.text) > 0)) {
        iflytek_asr_event_t event = asr_result.is_final ?
            IFLYTEK_ASR_EVENT_RESULT_FINAL : IFLYTEK_ASR_EVENT_RESULT_PARTIAL;
        g_ctx->event_cb(event, &asr_result, g_ctx->user_data);
    }

    cJSON_Delete(root);
}

// ==================== 接收任务 ====================

static void recv_task_func(void *arg)
{
    uint8_t *recv_buf = (uint8_t *)malloc(RECV_BUF_SIZE);
    uint8_t *payload_buf = (uint8_t *)malloc(RECV_BUF_SIZE);

    if (!recv_buf || !payload_buf) {
        ESP_LOGE(TAG, "Failed to allocate receive buffers");
        if (recv_buf) free(recv_buf);
        if (payload_buf) free(payload_buf);
        if (g_ctx) g_ctx->recv_task_running = false;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Receive task started");

    while (g_ctx && g_ctx->recv_task_running && g_ctx->tls_conn) {
        esp_tls_t *tls_conn = g_ctx->tls_conn;
        if (tls_conn == NULL) {
            break;
        }

        int ret = esp_tls_conn_read(tls_conn, recv_buf, RECV_BUF_SIZE - 1);

        if (ret > 0) {
            // 成功读取数据
            uint8_t opcode = 0;
            ssize_t payload_len = decode_ws_frame(recv_buf, ret, &opcode,
                                                   payload_buf, RECV_BUF_SIZE - 1);

            if (payload_len > 0) {
                payload_buf[payload_len] = '\0';

                switch (opcode) {
                    case 0x01:  // Text frame
                        ESP_LOGI(TAG, "Received text frame, len=%zd", payload_len);
                        parse_and_notify_result((const char *)payload_buf);
                        break;

                    case 0x08:  // Close frame
                        ESP_LOGI(TAG, "Received close frame");
                        g_ctx->recv_task_running = false;
                        break;

                    case 0x09:  // Ping frame
                        ESP_LOGD(TAG, "Received ping frame");
                        // 可以发送pong响应（暂时忽略）
                        break;

                    default:
                        ESP_LOGD(TAG, "Received frame, opcode=0x%02X, len=%zd", opcode, payload_len);
                        break;
                }
            }
        } else if (ret == 0) {
            g_ctx->recv_task_running = false;
            if (g_ctx->disconnect_requested) {
                break;
            }

            ESP_LOGI(TAG, "Connection closed by server");
            if (g_ctx->event_cb) {
                g_ctx->event_cb(IFLYTEK_ASR_EVENT_DISCONNECTED, NULL, g_ctx->user_data);
            }
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            // 需要更多数据，等待
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            if (!g_ctx->recv_task_running || g_ctx->disconnect_requested || g_ctx->tls_conn == NULL) {
                break;
            }
            ESP_LOGE(TAG, "Receive error: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    free(recv_buf);
    free(payload_buf);

    ESP_LOGI(TAG, "Receive task stopped");
    // 安全地清理任务句柄（g_ctx可能已被deinit释放）
    if (g_ctx != NULL) {
        g_ctx->recv_task_running = false;
        g_ctx->recv_task = NULL;
    }
    vTaskDelete(NULL);
}

// 启动接收任务
static esp_err_t start_recv_task(void)
{
    if (!g_ctx) return ESP_ERR_INVALID_STATE;

    if (g_ctx->recv_task_running) {
        ESP_LOGW(TAG, "Receive task already running");
        return ESP_OK;
    }

    g_ctx->recv_task_running = true;
    g_ctx->disconnect_requested = false;

    BaseType_t ret = xTaskCreate(
        recv_task_func,
        "iflytek_recv",
        RECV_TASK_STACK_SIZE,
        NULL,
        RECV_TASK_PRIORITY,
        &g_ctx->recv_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receive task");
        g_ctx->recv_task_running = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

// 停止接收任务
static void stop_recv_task(void)
{
    if (!g_ctx) return;

    if (!g_ctx->recv_task_running) {
        return;  // 任务未运行
    }

    g_ctx->recv_task_running = false;

    if (g_ctx->recv_task) {
        // 等待任务真正结束（最多等待500ms）
        int wait_count = 0;
        while (g_ctx->recv_task != NULL && wait_count < 50) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
    }
}

// ==================== 公共API实现 ====================

esp_err_t iflytek_asr_init(const iflytek_asr_config_t *config,
                           iflytek_asr_event_cb_t event_cb,
                           void *user_data)
{
    ESP_LOGI(TAG, "Initializing iFlytek ASR module");

    if (g_ctx != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    g_ctx = (iflytek_asr_ctx_t *)calloc(1, sizeof(iflytek_asr_ctx_t));
    if (!g_ctx) {
        ESP_LOGE(TAG, "Failed to allocate context");
        return ESP_ERR_NO_MEM;
    }

    // 创建互斥锁
    g_ctx->conn_mutex = xSemaphoreCreateMutex();
    if (!g_ctx->conn_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(g_ctx);
        g_ctx = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 复制配置
    strlcpy(g_ctx->config.appid, config->appid, sizeof(g_ctx->config.appid));
    strlcpy(g_ctx->config.api_key, config->api_key, sizeof(g_ctx->config.api_key));
    strlcpy(g_ctx->config.api_secret, config->api_secret, sizeof(g_ctx->config.api_secret));
    strlcpy(g_ctx->config.language, config->language, sizeof(g_ctx->config.language));
    strlcpy(g_ctx->config.domain, config->domain, sizeof(g_ctx->config.domain));
    g_ctx->config.enable_punctuation = config->enable_punctuation;
    g_ctx->config.enable_nlp = config->enable_nlp;
    g_ctx->config.sample_rate = config->sample_rate;

    g_ctx->event_cb = event_cb;
    g_ctx->user_data = user_data;
    g_ctx->state = IFLYTEK_ASR_STATE_IDLE;
    g_ctx->disconnect_requested = false;
    reset_result_segments();

    ESP_LOGI(TAG, "ASR module initialized, APPID: %s", config->appid);
    return ESP_OK;
}

esp_err_t iflytek_asr_deinit(void)
{
    esp_log_level_t tls_log_level = ESP_LOG_ERROR;
    esp_log_level_t dynamic_log_level = ESP_LOG_ERROR;

    if (!g_ctx) {
        return ESP_ERR_INVALID_STATE;
    }

    g_ctx->disconnect_requested = true;
    suppress_disconnect_logs(&tls_log_level, &dynamic_log_level);
    shutdown_tls_socket();
    stop_recv_task();
    destroy_tls_connection();
    restore_disconnect_logs(tls_log_level, dynamic_log_level);

    // 删除互斥锁
    if (g_ctx->conn_mutex) {
        vSemaphoreDelete(g_ctx->conn_mutex);
    }

    free(g_ctx);
    g_ctx = NULL;

    ESP_LOGI(TAG, "ASR module deinitialized");
    return ESP_OK;
}

esp_err_t iflytek_asr_connect(void)
{
    ESP_LOGI(TAG, "Connecting to iFlytek server: %s", IFLYTEK_HOST);

    if (!g_ctx) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_ctx->state != IFLYTEK_ASR_STATE_IDLE) {
        ESP_LOGW(TAG, "Already connected or connecting");
        return ESP_OK;
    }

    g_ctx->state = IFLYTEK_ASR_STATE_CONNECTING;

    // 生成鉴权参数
    char date[64];
    get_rfc1123_date(date, sizeof(date));

    // 构建signature origin
    char signature_origin[256];
    snprintf(signature_origin, sizeof(signature_origin),
             "host: %s\ndate: %s\nGET /v2/iat HTTP/1.1",
             IFLYTEK_HOST, date);

    // 计算HMAC-SHA256
    uint8_t hmac_result[32];
    hmac_sha256((const uint8_t *)g_ctx->config.api_secret,
               strlen(g_ctx->config.api_secret),
               (const uint8_t *)signature_origin,
               strlen(signature_origin), hmac_result);

    // Base64编码签名
    char signature_base64[64];
    base64_encode_standard(hmac_result, sizeof(hmac_result),
                          signature_base64, sizeof(signature_base64));

    // 构建authorization
    char authorization_origin[256];
    snprintf(authorization_origin, sizeof(authorization_origin),
             "api_key=\"%s\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%s\"",
             g_ctx->config.api_key, signature_base64);

    char authorization[256];
    size_t auth_output_len = sizeof(authorization);
    mbedtls_base64_encode((unsigned char *)authorization, sizeof(authorization),
                         &auth_output_len, (const unsigned char *)authorization_origin,
                         strlen(authorization_origin));
    authorization[auth_output_len] = '\0';

    // URL编码
    char *auth_encoded = url_encode(authorization);
    char *date_encoded = url_encode(date);
    char *host_encoded = url_encode(IFLYTEK_HOST);

    // DNS解析
    struct addrinfo hints;
    struct addrinfo *dns_result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int dns_ret = getaddrinfo(IFLYTEK_HOST, "443", &hints, &dns_result);
    if (dns_ret != 0) {
        ESP_LOGE(TAG, "DNS resolution failed: %d", dns_ret);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_FAIL;
    }

    if (dns_result) {
        struct sockaddr_in *addr = (struct sockaddr_in *)dns_result->ai_addr;
        ESP_LOGI(TAG, "DNS resolved to: %s", inet_ntoa(addr->sin_addr));
        freeaddrinfo(dns_result);
    }

    // TLS配置
    esp_tls_cfg_t tls_cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .common_name = IFLYTEK_HOST,
        .skip_common_name = false,
        .timeout_ms = 30000,
        .non_block = false,
    };

    // 创建TLS连接
    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to create TLS context");
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_FAIL;
    }

    int ret = esp_tls_conn_new_sync(IFLYTEK_HOST, strlen(IFLYTEK_HOST), 443,
                                     &tls_cfg, tls);

    if (ret <= 0) {
        ESP_LOGE(TAG, "TLS connection failed: %d", ret);
        esp_tls_conn_destroy(tls);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TLS connection established");

    // 构建WebSocket握手请求
    char *request = (char *)malloc(1024);
    if (!request) {
        esp_tls_conn_destroy(tls);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_ERR_NO_MEM;
    }

    int request_len = snprintf(request, 1024,
        "GET /v2/iat?authorization=%s&date=%s&host=%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "User-Agent: ESP32-iFlytek-ASR/1.0\r\n"
        "\r\n",
        auth_encoded, date_encoded, host_encoded,
        IFLYTEK_HOST
    );

    // 发送握手请求
    ret = esp_tls_conn_write(tls, (const unsigned char *)request, request_len);
    free(request);

    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to send handshake");
        esp_tls_conn_destroy(tls);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_FAIL;
    }

    // 读取响应
    char *response = (char *)malloc(HANDSHAKE_BUF_SIZE);
    if (!response) {
        esp_tls_conn_destroy(tls);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_ERR_NO_MEM;
    }

    ret = read_http_headers(tls, response, HANDSHAKE_BUF_SIZE);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to read handshake response: %d", ret);
        free(response);
        esp_tls_conn_destroy(tls);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_FAIL;
    }
    response[ret] = '\0';
    ESP_LOGI(TAG, "Handshake response: %.*s", ret < 200 ? ret : 200, response);

    // 检查响应状态码
    bool handshake_ok = (strstr(response, "HTTP/1.1 101") != NULL ||
                         strstr(response, "HTTP/1.0 101") != NULL);
    free(response);

    if (!handshake_ok) {
        ESP_LOGE(TAG, "WebSocket handshake failed");
        esp_tls_conn_destroy(tls);
        free(auth_encoded);
        free(date_encoded);
        free(host_encoded);
        g_ctx->state = IFLYTEK_ASR_STATE_ERROR;
        return ESP_FAIL;
    }

    // 保存连接
    g_ctx->tls_conn = tls;
    g_ctx->handshake_complete = true;
    g_ctx->state = IFLYTEK_ASR_STATE_CONNECTED;
    g_ctx->disconnect_requested = false;

    free(auth_encoded);
    free(date_encoded);
    free(host_encoded);

    ESP_LOGI(TAG, "WebSocket handshake successful");

    // 启动接收任务
    esp_err_t err = start_recv_task();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start receive task");
    }

    // 触发连接事件
    if (g_ctx->event_cb) {
        g_ctx->event_cb(IFLYTEK_ASR_EVENT_CONNECTED, NULL, g_ctx->user_data);
    }

    return ESP_OK;
}

esp_err_t iflytek_asr_disconnect(void)
{
    esp_log_level_t tls_log_level = ESP_LOG_ERROR;
    esp_log_level_t dynamic_log_level = ESP_LOG_ERROR;

    if (!g_ctx) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Disconnecting...");

    g_ctx->disconnect_requested = true;
    suppress_disconnect_logs(&tls_log_level, &dynamic_log_level);
    shutdown_tls_socket();
    stop_recv_task();
    destroy_tls_connection();
    restore_disconnect_logs(tls_log_level, dynamic_log_level);

    g_ctx->state = IFLYTEK_ASR_STATE_IDLE;
    g_ctx->handshake_complete = false;

    // 触发断开事件
    if (g_ctx->event_cb) {
        g_ctx->event_cb(IFLYTEK_ASR_EVENT_DISCONNECTED, NULL, g_ctx->user_data);
    }

    ESP_LOGI(TAG, "Disconnected");
    return ESP_OK;
}

esp_err_t iflytek_asr_start_listening(void)
{
    if (!g_ctx || !g_ctx->tls_conn) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting listening...");

    // 构建第一帧JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *common = cJSON_CreateObject();
    cJSON *business = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    cJSON_AddStringToObject(common, "app_id", g_ctx->config.appid);
    cJSON_AddStringToObject(business, "language", g_ctx->config.language);
    cJSON_AddStringToObject(business, "domain", g_ctx->config.domain);
    cJSON_AddStringToObject(business, "accent", "mandarin");
    cJSON_AddStringToObject(business, "dwa", "wpgs");
    cJSON_AddNumberToObject(data, "status", 0);
    cJSON_AddStringToObject(data, "format", "audio/L16;rate=16000");
    cJSON_AddStringToObject(data, "encoding", "raw");
    cJSON_AddStringToObject(data, "audio", "");

    cJSON_AddItemToObject(root, "common", common);
    cJSON_AddItemToObject(root, "business", business);
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) {
        ESP_LOGE(TAG, "Failed to create JSON");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Sending first frame: %s", json_str);

    // 编码WebSocket帧
    uint8_t frame_buf[1024];
    ssize_t frame_len = encode_ws_frame((uint8_t *)json_str, strlen(json_str),
                                       0x01, frame_buf, sizeof(frame_buf));
    free(json_str);

    if (frame_len < 0) {
        ESP_LOGE(TAG, "Failed to encode frame");
        return ESP_FAIL;
    }

    // 发送
    int ret = esp_tls_conn_write(g_ctx->tls_conn, frame_buf, frame_len);

    if (ret > 0) {
        g_ctx->state = IFLYTEK_ASR_STATE_LISTENING;
        g_ctx->is_first_frame = false;
        g_ctx->frames_sent = 0;
        g_ctx->total_bytes_sent = 0;
        reset_result_segments();

        ESP_LOGI(TAG, "Listening started");

        if (g_ctx->event_cb) {
            g_ctx->event_cb(IFLYTEK_ASR_EVENT_LISTENING_START, NULL, g_ctx->user_data);
        }

        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to send first frame: %d", ret);
    return ESP_FAIL;
}

esp_err_t iflytek_asr_stop_listening(void)
{
    if (!g_ctx || !g_ctx->tls_conn) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping listening...");

    // 构建结束帧JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "status", 2);
    cJSON_AddStringToObject(data_obj, "format", "audio/L16;rate=16000");
    cJSON_AddStringToObject(data_obj, "encoding", "raw");
    cJSON_AddStringToObject(data_obj, "audio", "");
    cJSON_AddItemToObject(root, "data", data_obj);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str) {
        uint8_t frame_buf[256];
        ssize_t frame_len = encode_ws_frame((uint8_t *)json_str, strlen(json_str),
                                           0x01, frame_buf, sizeof(frame_buf));
        free(json_str);

        if (frame_len > 0) {
            esp_tls_conn_write(g_ctx->tls_conn, frame_buf, frame_len);
        }
    }

    g_ctx->state = IFLYTEK_ASR_STATE_CONNECTED;

    if (g_ctx->event_cb) {
        g_ctx->event_cb(IFLYTEK_ASR_EVENT_LISTENING_STOP, NULL, g_ctx->user_data);
    }

    ESP_LOGI(TAG, "Listening stopped, total frames: %u, bytes: %u",
             g_ctx->frames_sent, g_ctx->total_bytes_sent);
    return ESP_OK;
}

esp_err_t iflytek_asr_send_audio(const uint8_t *data, size_t len)
{
    if (!g_ctx || !g_ctx->tls_conn) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Base64编码
    size_t base64_buf_len = ((len + 2) / 3) * 4 + 10;
    char *base64_data = (char *)malloc(base64_buf_len);
    if (!base64_data) {
        return ESP_ERR_NO_MEM;
    }

    size_t base64_len = 0;
    int ret = mbedtls_base64_encode((unsigned char *)base64_data, base64_buf_len,
                                    &base64_len, data, len);
    if (ret != 0) {
        free(base64_data);
        return ESP_FAIL;
    }
    base64_data[base64_len] = '\0';

    // 构建JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "status", 1);
    cJSON_AddStringToObject(data_obj, "format", "audio/L16;rate=16000");
    cJSON_AddStringToObject(data_obj, "encoding", "raw");
    cJSON_AddStringToObject(data_obj, "audio", base64_data);
    cJSON_AddItemToObject(root, "data", data_obj);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(base64_data);

    if (!json_str) {
        return ESP_ERR_NO_MEM;
    }

    // 编码WebSocket帧
    uint8_t frame_buf[4096];
    ssize_t frame_len = encode_ws_frame((uint8_t *)json_str, strlen(json_str),
                                       0x01, frame_buf, sizeof(frame_buf));
    free(json_str);

    if (frame_len < 0) {
        return ESP_FAIL;
    }

    // 发送
    int write_ret = esp_tls_conn_write(g_ctx->tls_conn, frame_buf, frame_len);

    if (write_ret > 0) {
        g_ctx->frames_sent++;
        g_ctx->total_bytes_sent += len;
        return ESP_OK;
    }

    return ESP_FAIL;
}

iflytek_asr_state_t iflytek_asr_get_state(void)
{
    return g_ctx ? g_ctx->state : IFLYTEK_ASR_STATE_IDLE;
}

bool iflytek_asr_is_connected(void)
{
    return g_ctx && g_ctx->tls_conn != NULL &&
           (g_ctx->state == IFLYTEK_ASR_STATE_CONNECTED ||
            g_ctx->state == IFLYTEK_ASR_STATE_LISTENING);
}

bool iflytek_asr_is_listening(void)
{
    return g_ctx && g_ctx->state == IFLYTEK_ASR_STATE_LISTENING;
}

esp_err_t iflytek_asr_generate_auth_url(char *url, size_t url_len)
{
    if (!g_ctx || !url) return ESP_ERR_INVALID_ARG;

    char date[64];
    get_rfc1123_date(date, sizeof(date));

    char signature_origin[256];
    snprintf(signature_origin, sizeof(signature_origin),
             "host: %s\ndate: %s\nGET /v2/iat HTTP/1.1",
             IFLYTEK_HOST, date);

    uint8_t hmac_result[32];
    hmac_sha256((const uint8_t *)g_ctx->config.api_secret,
               strlen(g_ctx->config.api_secret),
               (const uint8_t *)signature_origin,
               strlen(signature_origin), hmac_result);

    char signature_base64[64];
    base64_encode_standard(hmac_result, sizeof(hmac_result),
                          signature_base64, sizeof(signature_base64));

    char authorization_origin[256];
    snprintf(authorization_origin, sizeof(authorization_origin),
             "api_key=\"%s\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%s\"",
             g_ctx->config.api_key, signature_base64);

    char authorization[256];
    size_t auth_output_len = sizeof(authorization);
    mbedtls_base64_encode((unsigned char *)authorization, sizeof(authorization),
                         &auth_output_len, (const unsigned char *)authorization_origin,
                         strlen(authorization_origin));
    authorization[auth_output_len] = '\0';

    char *auth_encoded = url_encode(authorization);
    char *date_encoded = url_encode(date);
    char *host_encoded = url_encode(IFLYTEK_HOST);

    snprintf(url, url_len,
             "wss://%s/v2/iat?authorization=%s&date=%s&host=%s",
             IFLYTEK_HOST, auth_encoded, date_encoded, host_encoded);

    free(auth_encoded);
    free(date_encoded);
    free(host_encoded);

    return ESP_OK;
}

// 获取统计信息
uint32_t iflytek_asr_get_frames_sent(void)
{
    return g_ctx ? g_ctx->frames_sent : 0;
}

uint32_t iflytek_asr_get_bytes_sent(void)
{
    return g_ctx ? g_ctx->total_bytes_sent : 0;
}
