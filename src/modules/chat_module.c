/**
 * @file chat_module.c
 * @brief Chat模块实现 - DeepSeek API
 *
 * 基于DeepSeek API文档实现:
 * - Base URL: https://api.deepseek.com
 * - Model: deepseek-chat (DeepSeek-V3)
 * - OpenAI兼容格式
 * - SSE流式响应
 */

#include "chat_module.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "cJSON.h"

static const char *TAG = "CHAT_MODULE";

// DeepSeek API配置
#define DEEPSEEK_API_BASE_URL   "https://api.deepseek.com"
#define DEEPSEEK_CHAT_ENDPOINT  "/v1/chat/completions"
#define DEEPSEEK_MODEL          "deepseek-chat"  // DeepSeek V3 模型

// 最大历史轮数
#define MAX_HISTORY (MAX_CHAT_HISTORY * 2)  // 每轮包含用户消息和助手回复

// 响应缓冲区大小
#define RESPONSE_BUFFER_SIZE    8192

// Chat模块上下文
typedef struct {
    char api_key[MAX_API_KEY_LEN];
    char system_prompt[512];

    // 对话历史
    chat_message_t history[MAX_HISTORY];
    int history_count;

    // 当前请求相关
    chat_event_callback_t event_cb;
    void *user_data;

    // 响应累积缓冲区
    char response_buffer[RESPONSE_BUFFER_SIZE];
    int response_len;

    // SSE解析状态
    char sse_line_buffer[1024];
    int sse_line_len;

    // 互斥锁
    SemaphoreHandle_t mutex;

    // 运行状态
    volatile bool request_active;
} chat_module_t;

static chat_module_t *g_chat = NULL;

// ==================== 前向声明 ====================
static void parse_sse_line(const char *line);
static void add_assistant_message(const char *content);
static char *escape_json_string(const char *str);

// ==================== SSE解析 ====================

/**
 * @brief 解析SSE数据行
 *
 * SSE格式:
 * data: {"id":"...","choices":[{"delta":{"content":"你好"}}]}
 * data: [DONE]
 */
static void parse_sse_line(const char *line)
{
    if (line == NULL || strlen(line) == 0) {
        return;
    }

    // 检查是否是data行
    if (strncmp(line, "data: ", 6) != 0) {
        return;
    }

    const char *data = line + 6;

    // 检查是否是结束标记
    if (strcmp(data, "[DONE]") == 0) {
        ESP_LOGD(TAG, "SSE stream completed");
        return;
    }

    // 解析JSON
    cJSON *root = cJSON_Parse(data);
    if (root == NULL) {
        ESP_LOGW(TAG, "Failed to parse SSE JSON: %s", data);
        return;
    }

    // 获取choices数组
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (choices == NULL || !cJSON_IsArray(choices)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    if (choice == NULL) {
        cJSON_Delete(root);
        return;
    }

    // 获取delta对象
    cJSON *delta = cJSON_GetObjectItem(choice, "delta");
    if (delta == NULL) {
        cJSON_Delete(root);
        return;
    }

    // 获取content
    cJSON *content = cJSON_GetObjectItem(delta, "content");
    if (content != NULL && cJSON_IsString(content)) {
        const char *text = cJSON_GetStringValue(content);
        if (text != NULL && strlen(text) > 0) {
            // 累积到响应缓冲区
            if (g_chat->response_len + strlen(text) < RESPONSE_BUFFER_SIZE - 1) {
                strcat(g_chat->response_buffer, text);
                g_chat->response_len += strlen(text);
            }

            // 触发数据事件
            if (g_chat && g_chat->event_cb) {
                g_chat->event_cb(CHAT_EVENT_DATA, text, false, g_chat->user_data);
            }
        }
    }

    // 检查finish_reason
    cJSON *finish_reason = cJSON_GetObjectItem(choice, "finish_reason");
    if (finish_reason != NULL && cJSON_IsString(finish_reason)) {
        const char *reason = cJSON_GetStringValue(finish_reason);
        if (reason != NULL && strcmp(reason, "stop") == 0) {
            ESP_LOGD(TAG, "Generation finished with reason: stop");
        }
    }

    cJSON_Delete(root);
}

/**
 * @brief 处理HTTP数据流，解析SSE
 */
static void process_http_data(const char *data, int len)
{
    if (g_chat == NULL || data == NULL || len <= 0) {
        return;
    }

    // 将数据追加到行缓冲区
    for (int i = 0; i < len; i++) {
        char c = data[i];

        if (c == '\n') {
            // 行结束，解析这一行
            g_chat->sse_line_buffer[g_chat->sse_line_len] = '\0';

            // 跳过空行
            if (g_chat->sse_line_len > 0) {
                parse_sse_line(g_chat->sse_line_buffer);
            }

            g_chat->sse_line_len = 0;
        } else if (c != '\r') {
            // 添加到行缓冲区（跳过回车符）
            if (g_chat->sse_line_len < (int)sizeof(g_chat->sse_line_buffer) - 1) {
                g_chat->sse_line_buffer[g_chat->sse_line_len++] = c;
            }
        }
    }
}

// ==================== JSON辅助函数 ====================

/**
 * @brief 转义JSON字符串
 * @note 返回的字符串需要调用者释放
 */
static char *escape_json_string(const char *str)
{
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    char *escaped = (char *)malloc(len * 2 + 1);  // 最坏情况：每个字符都需要转义
    if (escaped == NULL) {
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < len && j < len * 2; i++) {
        char c = str[i];
        switch (c) {
            case '"':
                escaped[j++] = '\\';
                escaped[j++] = '"';
                break;
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '\b':
                escaped[j++] = '\\';
                escaped[j++] = 'b';
                break;
            case '\f':
                escaped[j++] = '\\';
                escaped[j++] = 'f';
                break;
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            default:
                // 其他控制字符转为\uXXXX格式（简化处理）
                if ((unsigned char)c < 0x20) {
                    j += snprintf(escaped + j, len * 2 + 1 - j, "\\u%04x", (unsigned char)c);
                } else {
                    escaped[j++] = c;
                }
                break;
        }
    }
    escaped[j] = '\0';

    return escaped;
}

// ==================== 历史记录管理 ====================

/**
 * @brief 添加助手消息到历史
 */
static void add_assistant_message(const char *content)
{
    if (g_chat == NULL || content == NULL || strlen(content) == 0) {
        return;
    }

    if (g_chat->history_count < MAX_HISTORY) {
        g_chat->history[g_chat->history_count].role = CHAT_ROLE_ASSISTANT;
        strncpy(g_chat->history[g_chat->history_count].content, content,
                sizeof(g_chat->history[g_chat->history_count].content) - 1);
        g_chat->history[g_chat->history_count].content[sizeof(g_chat->history[g_chat->history_count].content) - 1] = '\0';
        g_chat->history_count++;
        ESP_LOGD(TAG, "Added assistant message to history, count=%d", g_chat->history_count);
    } else {
        // 历史已满，移除最旧的消息对
        ESP_LOGW(TAG, "History full, removing oldest messages");
        if (g_chat->history_count >= 2) {
            // 移除用户消息和助手回复
            memmove(&g_chat->history[0], &g_chat->history[2],
                    (g_chat->history_count - 2) * sizeof(chat_message_t));
            g_chat->history_count -= 2;

            // 添加新消息
            g_chat->history[g_chat->history_count].role = CHAT_ROLE_ASSISTANT;
            strncpy(g_chat->history[g_chat->history_count].content, content,
                    sizeof(g_chat->history[g_chat->history_count].content) - 1);
            g_chat->history_count++;
        }
    }
}

// ==================== HTTP事件处理 ====================

/**
 * @brief HTTP事件处理器
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // 处理流式响应数据
            if (evt->data_len > 0 && g_chat && g_chat->request_active) {
                process_http_data((const char *)evt->data, evt->data_len);
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP request finished");
            if (g_chat && g_chat->event_cb) {
                // 保存助手回复到历史
                if (g_chat->response_len > 0) {
                    add_assistant_message(g_chat->response_buffer);
                }
                g_chat->event_cb(CHAT_EVENT_DONE, NULL, true, g_chat->user_data);
            }
            g_chat->request_active = false;
            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP error occurred");
            if (g_chat && g_chat->event_cb) {
                g_chat->event_cb(CHAT_EVENT_ERROR, NULL, false, g_chat->user_data);
            }
            g_chat->request_active = false;
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP disconnected");
            g_chat->request_active = false;
            break;

        default:
            break;
    }

    return ESP_OK;
}

// ==================== 初始化和配置 ====================

esp_err_t chat_module_init(const char *api_key)
{
    if (g_chat != NULL) {
        ESP_LOGW(TAG, "Chat模块已初始化");
        return ESP_OK;
    }

    if (api_key == NULL || strlen(api_key) == 0) {
        ESP_LOGE(TAG, "API密钥为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "初始化Chat模块...");
    ESP_LOGI(TAG, "API Key: %.8s...", api_key);  // 只打印前8个字符

    // 分配内存
    g_chat = (chat_module_t *)calloc(1, sizeof(chat_module_t));
    if (g_chat == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存API密钥
    strncpy(g_chat->api_key, api_key, sizeof(g_chat->api_key) - 1);
    g_chat->api_key[sizeof(g_chat->api_key) - 1] = '\0';

    // 设置默认系统提示词
    strncpy(g_chat->system_prompt,
            "你是小智，一个友好、乐于助人的AI语音助手。"
            "请用简洁、自然的语言回答用户的问题。"
            "回答不要太长，适合语音播放。",
            sizeof(g_chat->system_prompt) - 1);

    g_chat->history_count = 0;
    g_chat->request_active = false;

    // 创建互斥锁
    g_chat->mutex = xSemaphoreCreateMutex();
    if (g_chat->mutex == NULL) {
        free(g_chat);
        g_chat = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Chat模块初始化完成 (Model: %s)", DEEPSEEK_MODEL);
    return ESP_OK;
}

esp_err_t chat_module_deinit(void)
{
    if (g_chat == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化Chat模块");

    // 等待请求完成
    int wait_count = 0;
    while (g_chat->request_active && wait_count < 100) {
        vTaskDelay(pdMS_TO_TICKS(50));
        wait_count++;
    }

    if (g_chat->mutex) {
        vSemaphoreDelete(g_chat->mutex);
    }

    free(g_chat);
    g_chat = NULL;

    return ESP_OK;
}

// ==================== 对话管理 ====================

esp_err_t chat_module_send_message(const char *message, chat_event_callback_t event_cb, void *user_data)
{
    if (g_chat == NULL) {
        ESP_LOGE(TAG, "Chat模块未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (message == NULL || strlen(message) == 0) {
        ESP_LOGE(TAG, "消息为空");
        return ESP_ERR_INVALID_ARG;
    }

    // 检查是否有正在进行的请求
    if (g_chat->request_active) {
        ESP_LOGW(TAG, "上一个请求正在进行中");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "发送消息: %s", message);

    xSemaphoreTake(g_chat->mutex, portMAX_DELAY);

    // 重置响应缓冲区
    memset(g_chat->response_buffer, 0, RESPONSE_BUFFER_SIZE);
    g_chat->response_len = 0;
    g_chat->sse_line_len = 0;

    // 保存回调
    g_chat->event_cb = event_cb;
    g_chat->user_data = user_data;
    g_chat->request_active = true;

    // 触发开始事件
    if (event_cb) {
        event_cb(CHAT_EVENT_START, NULL, false, user_data);
    }

    // 添加用户消息到历史
    if (g_chat->history_count < MAX_HISTORY) {
        g_chat->history[g_chat->history_count].role = CHAT_ROLE_USER;
        strncpy(g_chat->history[g_chat->history_count].content, message,
                sizeof(g_chat->history[g_chat->history_count].content) - 1);
        g_chat->history[g_chat->history_count].content[sizeof(g_chat->history[g_chat->history_count].content) - 1] = '\0';
        g_chat->history_count++;
    }

    // 使用cJSON构建请求体
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", DEEPSEEK_MODEL);

    // 创建messages数组
    cJSON *messages = cJSON_CreateArray();

    // 添加系统消息
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    char *escaped_prompt = escape_json_string(g_chat->system_prompt);
    cJSON_AddStringToObject(sys_msg, "content", escaped_prompt ? escaped_prompt : g_chat->system_prompt);
    cJSON_AddItemToArray(messages, sys_msg);
    if (escaped_prompt) {
        free(escaped_prompt);
    }

    // 添加历史消息
    for (int i = 0; i < g_chat->history_count; i++) {
        cJSON *msg = cJSON_CreateObject();
        const char *role_str = (g_chat->history[i].role == CHAT_ROLE_USER) ? "user" : "assistant";
        cJSON_AddStringToObject(msg, "role", role_str);
        char *escaped = escape_json_string(g_chat->history[i].content);
        cJSON_AddStringToObject(msg, "content", escaped ? escaped : g_chat->history[i].content);
        cJSON_AddItemToArray(messages, msg);
        if (escaped) {
            free(escaped);
        }
    }

    cJSON_AddItemToObject(root, "messages", messages);

    // 启用流式响应
    cJSON_AddBoolToObject(root, "stream", true);

    // 添加可选参数
    // cJSON_AddNumberToObject(root, "temperature", 0.7);
    // cJSON_AddNumberToObject(root, "max_tokens", 1024);

    // 序列化JSON
    char *request_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (request_body == NULL) {
        ESP_LOGE(TAG, "JSON序列化失败");
        g_chat->request_active = false;
        xSemaphoreGive(g_chat->mutex);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Request body: %s", request_body);

    // 配置HTTP客户端
    esp_http_client_config_t http_cfg = {
        .url = DEEPSEEK_API_URL,  // 使用config.h中的定义
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = 60000,  // 60秒超时
        .buffer_size = 2048,
        .buffer_size_tx = 4096,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP客户端创建失败");
        free(request_body);
        g_chat->request_active = false;
        xSemaphoreGive(g_chat->mutex);
        return ESP_FAIL;
    }

    // 设置headers
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_chat->api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "text/event-stream");

    // 设置请求体
    esp_http_client_set_post_field(client, request_body, strlen(request_body));

    // 发送请求
    esp_err_t ret = esp_http_client_perform(client);

    int http_status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);

    ESP_LOGI(TAG, "HTTP status: %d, content_length: %d", http_status, content_length);

    // 清理
    free(request_body);
    esp_http_client_cleanup(client);

    xSemaphoreGive(g_chat->mutex);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP请求失败: %s", esp_err_to_name(ret));
        g_chat->request_active = false;
        return ret;
    }

    if (http_status != 200) {
        ESP_LOGE(TAG, "HTTP错误状态码: %d", http_status);
        g_chat->request_active = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t chat_module_set_system_prompt(const char *prompt)
{
    if (g_chat == NULL || prompt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_chat->mutex, portMAX_DELAY);
    strncpy(g_chat->system_prompt, prompt, sizeof(g_chat->system_prompt) - 1);
    g_chat->system_prompt[sizeof(g_chat->system_prompt) - 1] = '\0';
    xSemaphoreGive(g_chat->mutex);

    ESP_LOGI(TAG, "系统提示词已更新");
    return ESP_OK;
}

// ==================== 历史记录 ====================

esp_err_t chat_module_clear_history(void)
{
    if (g_chat == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_chat->mutex, portMAX_DELAY);
    g_chat->history_count = 0;
    memset(g_chat->history, 0, sizeof(g_chat->history));
    xSemaphoreGive(g_chat->mutex);

    ESP_LOGI(TAG, "对话历史已清空");
    return ESP_OK;
}

int chat_module_get_history_count(void)
{
    if (g_chat == NULL) {
        return 0;
    }

    xSemaphoreTake(g_chat->mutex, portMAX_DELAY);
    int count = g_chat->history_count;
    xSemaphoreGive(g_chat->mutex);

    return count;
}

// ==================== 高级功能 ====================

/**
 * @brief 获取最后一次AI回复
 * @param buffer 输出缓冲区
 * @param buf_len 缓冲区长度
 * @return ESP_OK成功
 */
esp_err_t chat_module_get_last_response(char *buffer, size_t buf_len)
{
    if (g_chat == NULL || buffer == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_chat->mutex, portMAX_DELAY);

    // 查找最后一条助手消息
    const char *last_response = NULL;
    for (int i = g_chat->history_count - 1; i >= 0; i--) {
        if (g_chat->history[i].role == CHAT_ROLE_ASSISTANT) {
            last_response = g_chat->history[i].content;
            break;
        }
    }

    if (last_response) {
        strncpy(buffer, last_response, buf_len - 1);
        buffer[buf_len - 1] = '\0';
    } else {
        buffer[0] = '\0';
    }

    xSemaphoreGive(g_chat->mutex);

    return ESP_OK;
}

/**
 * @brief 设置temperature参数
 * @param temperature 温度值(0.0-2.0)
 * @return ESP_OK成功
 */
esp_err_t chat_module_set_temperature(float temperature)
{
    // 暂未实现，保留接口
    (void)temperature;
    return ESP_ERR_NOT_SUPPORTED;
}
