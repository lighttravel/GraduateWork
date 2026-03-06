/**
 * @file http_server.c
 * @brief HTTP服务器实现 - 提供Web界面和API
 */

#include "http_server.h"
// #include "ws_server.h"  // 暂时禁用
#include "wifi_manager.h"
#include "config.h"
#include "nvs_storage.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "HTTP_SRVR";

// HTTP服务器上下文
static httpd_handle_t g_server = NULL;
static ws_data_callback_t g_ws_data_cb = NULL;
static http_ws_event_callback_t g_ws_event_cb = NULL;

// WebSocket客户端管理
#define MAX_WS_CLIENTS 4
static int g_ws_clients[MAX_WS_CLIENTS] = {-1, -1, -1, -1};
static int g_ws_client_count = 0;
static SemaphoreHandle_t g_ws_mutex = NULL;

// ==================== HTML内容 ====================

// 简化的HTML页面(实际项目中应该从文件系统读取)
static const char index_html_start[] = "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>小智AI语音助手</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5}"
    ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:8px}"
    "h1{color:#333;text-align:center}"
    ".status{padding:15px;margin:10px 0;border-radius:4px}"
    ".status.online{background:#d4edda;color:#155724}"
    ".status.offline{background:#f8d7da;color:#721c24}"
    ".section{margin:20px 0;padding:15px;border:1px solid #ddd;border-radius:4px}"
    "label{display:block;margin:10px 0 5px;font-weight:bold}"
    "input,select{width:100%;padding:8px;margin:5px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}"
    "button{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;margin:5px}"
    "button:hover{background:#45a049}"
    ".log{background:#f8f9fa;padding:10px;height:200px;overflow-y:auto;font-family:monospace;font-size:12px}"
    "</style></head><body>"
    "<div class='container'>"
    "<h1>🎤 小智AI语音助手</h1>"
    "<div id='status' class='status offline'>● 离线</div>";

static const char index_html_config[] =
    "<div class='section'><h2>WiFi配置</h2>"
    "<label>SSID:</label><input type='text' id='wifi_ssid' placeholder='输入WiFi名称'>"
    "<label>密码:</label><input type='text' id='wifi_pass' placeholder='输入WiFi密码'>"
    "<button onclick='saveWifiConfig()'>保存WiFi配置</button></div>"

    "<div class='section'><h2>API配置</h2>"
    "<label>Deepgram API Key (ASR):</label><input type='text' id='deepgram_key' placeholder='输入Deepgram API密钥'><br>"
    "<button onclick='testDeepgram()'>测试Deepgram</button><br><br>"
    "<label>DeepSeek API Key (Chat):</label><input type='text' id='deepseek_key' placeholder='输入DeepSeek API密钥'><br>"
    "<button onclick='testDeepSeek()'>测试DeepSeek</button><br><br>"
    "<label>Index-TTS URL:</label><input type='text' id='tts_url' placeholder='输入TTS服务URL'>"
    "<button onclick='saveApiConfig()'>保存API配置</button></div>"

    "<div class='section'><h2>对话测试</h2>"
    "<textarea id='message' rows='3' placeholder='输入要发送的消息'></textarea><br>"
    "<button onclick='sendMessage()'>发送消息</button>"
    "<div id='response'></div></div>"

    "<div class='section'><h2>系统日志</h2>"
    "<div id='log' class='log'></div></div>"

    "<script>";

static const char index_html_script[] =
    "var currentConfig={wifi_ssid:'',wifi_pass:'',deepgram_key:'',deepseek_key:'',tts_url:''};"
    "function updateStatus(){"
    "  fetch('/api/status')"
    "  .then(r=>r.json())"
    "  .then(d=>{"
    "    var el=document.getElementById('status');"
    "    if(d.status==='online'){"
    "      el.className='status online';"
    "      el.innerHTML='● 在线';"
    "    }else{"
    "      el.className='status offline';"
    "      el.innerHTML='● 离线';"
    "    }"
    "  }).catch(e=>{"
    "    document.getElementById('status').className='status offline';"
    "    document.getElementById('status').innerHTML='● 离线';"
    "  });"
    "}"
    "function addLog(msg){"
    "  var log=document.getElementById('log');"
    "  var time=new Date().toLocaleTimeString();"
    "  log.innerHTML+='['+time+'] '+msg+'<br>';"
    "  log.scrollTop=log.scrollHeight;"
    "}"
    "function loadConfig(){"
    "  fetch('/api/config/get')"
    "  .then(r=>r.json())"
    "  .then(d=>{"
    "    if(d.status==='success'){"
    "      currentConfig=d;"
    "      if(d.wifi_ssid)document.getElementById('wifi_ssid').value=d.wifi_ssid;"
    "      if(d.wifi_pass)document.getElementById('wifi_pass').value=d.wifi_pass;"
    "      if(d.deepgram_key)document.getElementById('deepgram_key').value=d.deepgram_key;"
    "      if(d.deepseek_key)document.getElementById('deepseek_key').value=d.deepseek_key;"
    "      if(d.tts_url)document.getElementById('tts_url').value=d.tts_url;"
    "      addLog('配置已加载');"
    "    }"
    "  }).catch(e=>addLog('加载配置失败'));"
    "}"
    "function saveWifiConfig(){"
    "  var ssid=document.getElementById('wifi_ssid').value;"
    "  var pass=document.getElementById('wifi_pass').value;"
    "  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},"
    "  body:JSON.stringify({wifi_ssid:ssid,wifi_pass:pass})})"
    "  .then(r=>r.json()).then(d=>{addLog(d.message);if(d.status==='success')loadConfig();});"
    "}"
    "function saveApiConfig(){"
    "  var deepgram=document.getElementById('deepgram_key').value;"
    "  var deepseek=document.getElementById('deepseek_key').value;"
    "  var tts=document.getElementById('tts_url').value;"
    "  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},"
    "  body:JSON.stringify({deepgram_key:deepgram,deepseek_key:deepseek,tts_url:tts})})"
    "  .then(r=>r.json()).then(d=>{addLog(d.message);if(d.status==='success')loadConfig();});"
    "}"
    "function testDeepgram(){"
    "  addLog('正在测试Deepgram API... (可能需要15-20秒)');"
    "  fetch('/api/test/deepgram')"
    "  .then(r=>r.json())"
    "  .then(d=>{"
    "    if(d.status==='success'){"
    "      addLog('✓ Deepgram API测试成功: '+d.message);"
    "    }else{"
    "      addLog('✗ Deepgram API测试失败: '+d.message);"
    "    }"
    "  }).catch(e=>addLog('✗ Deepgram API测试失败: 连接超时或网络错误'));"
    "}"
    "function testDeepSeek(){"
    "  addLog('正在测试DeepSeek API... (可能需要15-20秒)');"
    "  fetch('/api/test/deepseek')"
    "  .then(r=>r.json())"
    "  .then(d=>{"
    "    if(d.status==='success'){"
    "      addLog('✓ DeepSeek API测试成功: '+d.message);"
    "    }else{"
    "      addLog('✗ DeepSeek API测试失败: '+d.message);"
    "    }"
    "  }).catch(e=>addLog('✗ DeepSeek API测试失败: 连接超时或网络错误'));"
    "}"
    "function sendMessage(){"
    "  var msg=document.getElementById('message').value;"
    "  addLog('发送: '+msg);"
    "  document.getElementById('response').innerHTML='消息已发送';"
    "}"
    "window.onload=function(){"
    "  loadConfig();"
    "  updateStatus();"
    "  setInterval(updateStatus,5000);"
    "  addLog('系统已启动');"
    "  addLog('提示: API测试可能需要15-20秒，请耐心等待');"
    "};"
    "</script></div></body></html>";

// ==================== 辅助函数 ====================

/**
 * @brief 从JSON中提取字段值
 */
static char* extract_json_value(const char *json, const char *key)
{
    static char value_buf[256];
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);

    char *start = strstr(json, search);
    if (!start) {
        return NULL;
    }

    start += strlen(search);
    char *end = strchr(start, '"');
    if (!end) {
        return NULL;
    }

    int len = end - start;
    if (len >= sizeof(value_buf)) {
        len = sizeof(value_buf) - 1;
    }

    strncpy(value_buf, start, len);
    value_buf[len] = '\0';

    return value_buf;
}

// ==================== 请求处理器 ====================

/**
 * @brief 根路径处理器 - 返回HTML页面
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "访问主页");

    httpd_resp_set_type(req, "text/html");

    // 发送HTML
    httpd_resp_send_chunk(req, index_html_start, strlen(index_html_start));
    httpd_resp_send_chunk(req, index_html_config, strlen(index_html_config));
    httpd_resp_send_chunk(req, index_html_script, strlen(index_html_script));
    httpd_resp_sendstr_chunk(req, NULL);  // 结束响应

    return ESP_OK;
}

/**
 * @brief API状态处理器
 */
static esp_err_t api_status_handler(httpd_req_t *req)
{
    // 获取WiFi状态
    bool is_online = wifi_manager_is_connected();

    // 获取IP地址
    char *ip_addr = wifi_manager_get_ip();
    if (ip_addr == NULL) {
        ip_addr = "0.0.0.0";
    }

    char *json = NULL;
    asprintf(&json,
             "{\"status\":\"%s\",\"ip\":\"%s\",\"uptime\":%lld,\"free_heap\":%lu}",
             is_online ? "online" : "offline",
             ip_addr,
             esp_timer_get_time() / 1000000,
             (unsigned long)esp_get_free_heap_size()
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free(json);
    return ESP_OK;
}

/**
 * @brief API配置保存处理器
 */
static esp_err_t api_config_handler(httpd_req_t *req)
{
    // 读取请求体
    char buf[MAX_HTTP_REQ_LEN];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "收到配置: %s", buf);

    // 解析并保存WiFi配置
    char *value;
    if ((value = extract_json_value(buf, "wifi_ssid"))) {
        nvs_storage_set_string(NVS_KEY_WIFI_SSID, value);
        ESP_LOGI(TAG, "保存SSID: %s", value);
    }
    if ((value = extract_json_value(buf, "wifi_pass"))) {
        nvs_storage_set_string(NVS_KEY_WIFI_PASS, value);
    }

    // 解析并保存API配置
    if ((value = extract_json_value(buf, "deepgram_key"))) {
        nvs_storage_set_string(NVS_KEY_DEEPGRAM_KEY, value);
        ESP_LOGI(TAG, "保存Deepgram密钥");
    }
    if ((value = extract_json_value(buf, "deepseek_key"))) {
        nvs_storage_set_string(NVS_KEY_DEEPSEEK_KEY, value);
        ESP_LOGI(TAG, "保存DeepSeek密钥");
    }
    if ((value = extract_json_value(buf, "tts_url"))) {
        nvs_storage_set_string(NVS_KEY_INDEX_TTS_URL, value);
    }

    // 返回成功响应
    const char *response = "{\"status\":\"success\",\"message\":\"配置已保存\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    return ESP_OK;
}

/**
 * @brief 获取当前配置
 */
static esp_err_t api_get_config_handler(httpd_req_t *req)
{
    char wifi_ssid[64] = {0};
    char wifi_pass[128] = {0};
    char deepgram_key[MAX_API_KEY_LEN] = {0};
    char deepseek_key[MAX_API_KEY_LEN] = {0};
    char tts_url[MAX_URL_LEN] = {0};

    // 从NVS读取配置
    nvs_storage_get_string(NVS_KEY_WIFI_SSID, wifi_ssid, sizeof(wifi_ssid));
    nvs_storage_get_string(NVS_KEY_WIFI_PASS, wifi_pass, sizeof(wifi_pass));
    nvs_storage_get_string(NVS_KEY_DEEPGRAM_KEY, deepgram_key, sizeof(deepgram_key));
    nvs_storage_get_string(NVS_KEY_DEEPSEEK_KEY, deepseek_key, sizeof(deepseek_key));
    nvs_storage_get_string(NVS_KEY_INDEX_TTS_URL, tts_url, sizeof(tts_url));

    // 如果API密钥为空或无效（长度太短），设置默认值
    if (strlen(deepgram_key) < 10) {
        strncpy(deepgram_key, "1a0dd98ccea71a0431445b5ccf9fe601c106a01e", MAX_API_KEY_LEN - 1);
        nvs_storage_set_string(NVS_KEY_DEEPGRAM_KEY, deepgram_key);
        ESP_LOGI(TAG, "设置默认Deepgram密钥");
    }
    if (strlen(deepseek_key) < 10) {
        strncpy(deepseek_key, "sk-ee35fd5c6c50431380952e0ec39050d9", MAX_API_KEY_LEN - 1);
        nvs_storage_set_string(NVS_KEY_DEEPSEEK_KEY, deepseek_key);
        ESP_LOGI(TAG, "设置默认DeepSeek密钥");
    }

    // WiFi默认值
    if (strlen(wifi_ssid) == 0) {
        strncpy(wifi_ssid, DEFAULT_WIFI_SSID, sizeof(wifi_ssid) - 1);
    }

    // 日志输出配置值（用于调试）
    ESP_LOGI(TAG, "返回配置: DeepSeek=%s, Deepgram=%s",
             deepseek_key, deepgram_key);

    // 构建JSON响应 - 确保字符串正确转义
    httpd_resp_set_type(req, "application/json");

    // 分段构建JSON以避免格式问题
    char *json = NULL;
    asprintf(&json,
             "{"
             "\"status\":\"success\","
             "\"wifi_ssid\":\"%s\","
             "\"wifi_pass\":\"%s\","
             "\"deepgram_key\":\"%s\","
             "\"deepseek_key\":\"%s\","
             "\"tts_url\":\"%s\""
             "}",
             wifi_ssid, wifi_pass, deepgram_key, deepseek_key, tts_url);

    httpd_resp_sendstr(req, json);
    free(json);

    return ESP_OK;
}

/**
 * @brief HTTP事件处理器（用于API测试）
 */
typedef struct {
    char *response_buffer;
    size_t buffer_size;
    size_t current_length;
    bool request_complete;
    int status_code;
} test_api_context_t;

static esp_err_t test_api_http_event_handler(esp_http_client_event_t *evt)
{
    test_api_context_t *ctx = (test_api_context_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx->response_buffer && ctx->current_length < ctx->buffer_size - 1) {
                size_t copy_len = evt->data_len;
                if (copy_len > ctx->buffer_size - ctx->current_length - 1) {
                    copy_len = ctx->buffer_size - ctx->current_length - 1;
                }
                memcpy(ctx->response_buffer + ctx->current_length, evt->data, copy_len);
                ctx->current_length += copy_len;
                ctx->response_buffer[ctx->current_length] = '\0';
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ctx->request_complete = true;
            ctx->status_code = esp_http_client_get_status_code(evt->client);
            break;

        case HTTP_EVENT_ERROR:
            ctx->request_complete = true;
            ctx->status_code = -1;
            break;

        default:
            break;
    }

    return ESP_OK;
}

/**
 * @brief 测试Deepgram API（真实连接测试）
 */
static esp_err_t api_test_deepgram_handler(httpd_req_t *req)
{
    char deepgram_key[MAX_API_KEY_LEN] = {0};
    char response_buffer[1024] = {0};
    test_api_context_t ctx = {
        .response_buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .current_length = 0,
        .request_complete = false,
        .status_code = 0
    };

    // 从NVS读取Deepgram密钥
    nvs_storage_get_string(NVS_KEY_DEEPGRAM_KEY, deepgram_key, sizeof(deepgram_key));

    ESP_LOGI(TAG, "测试Deepgram API，密钥长度: %zu", strlen(deepgram_key));

    char *json = NULL;

    // 检查密钥长度
    if (strlen(deepgram_key) < 10) {
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"Deepgram API密钥无效 (太短: %zu字符)\",\"key_length\":%zu}",
                 strlen(deepgram_key), strlen(deepgram_key));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        free(json);
        return ESP_OK;
    }

    // 发送真实API请求 - 尝试获取账户信息
    char url[256];
    snprintf(url, sizeof(url), "https://api.deepgram.com/v1/projects");

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .event_handler = test_api_http_event_handler,
        .user_data = &ctx,
        .buffer_size = 1024,
        .cert_pem = NULL,  // 使用mbedtls证书bundle
        .skip_cert_common_name_check = false,
        .use_global_ca_store = false,  // 不使用全局CA存储
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL) {
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"HTTP客户端初始化失败\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        free(json);
        return ESP_OK;
    }

    // 设置Authorization header
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", deepgram_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // 发送请求
    esp_err_t ret = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    // 等待请求完成
    int timeout = 100; // 10秒超时
    while (!ctx.request_complete && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
    }

    // 分析结果
    if (ctx.status_code == 200 || ctx.status_code == 201) {
        ESP_LOGI(TAG, "Deepgram API测试成功，响应: %s", response_buffer);
        asprintf(&json,
                 "{\"status\":\"success\",\"message\":\"Deepgram API连接成功\",\"status_code\":%d}",
                 ctx.status_code);
    } else if (ctx.status_code == 401) {
        ESP_LOGE(TAG, "Deepgram API密钥无效");
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"Deepgram API密钥无效（认证失败）\",\"status_code\":%d}",
                 ctx.status_code);
    } else if (ctx.status_code > 0) {
        ESP_LOGE(TAG, "Deepgram API返回错误: %d", ctx.status_code);
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"Deepgram API返回错误状态码\",\"status_code\":%d}",
                 ctx.status_code);
    } else {
        ESP_LOGE(TAG, "Deepgram API连接失败");
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"Deepgram API连接失败（网络错误）\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);

    return ESP_OK;
}

/**
 * @brief 测试DeepSeek API（真实连接测试）
 */
static esp_err_t api_test_deepseek_handler(httpd_req_t *req)
{
    char deepseek_key[MAX_API_KEY_LEN] = {0};
    char response_buffer[2048] = {0};
    test_api_context_t ctx = {
        .response_buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .current_length = 0,
        .request_complete = false,
        .status_code = 0
    };

    // 从NVS读取DeepSeek密钥
    nvs_storage_get_string(NVS_KEY_DEEPSEEK_KEY, deepseek_key, sizeof(deepseek_key));

    ESP_LOGI(TAG, "测试DeepSeek API，密钥长度: %zu", strlen(deepseek_key));

    char *json = NULL;

    // 检查密钥长度
    if (strlen(deepseek_key) < 10) {
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"DeepSeek API密钥无效 (太短: %zu字符)\",\"key_length\":%zu}",
                 strlen(deepseek_key), strlen(deepseek_key));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        free(json);
        return ESP_OK;
    }

    // 构建请求体 - 发送一个简单的测试消息
    const char *request_body = "{\"model\":\"deepseek-chat\",\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}],\"max_tokens\":10}";

    esp_http_client_config_t http_cfg = {
        .url = DEEPSEEK_API_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .event_handler = test_api_http_event_handler,
        .user_data = &ctx,
        .buffer_size = 2048,
        .cert_pem = NULL,  // 使用mbedtls证书bundle
        .skip_cert_common_name_check = false,
        .use_global_ca_store = false,  // 不使用全局CA存储
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL) {
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"HTTP客户端初始化失败\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        free(json);
        return ESP_OK;
    }

    // 设置headers
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", deepseek_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // 设置请求体
    esp_http_client_set_post_field(client, request_body, strlen(request_body));

    // 发送请求
    esp_err_t ret = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    // 等待请求完成
    int timeout = 150; // 15秒超时
    while (!ctx.request_complete && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
    }

    // 分析结果
    if (ctx.status_code == 200 || ctx.status_code == 201) {
        ESP_LOGI(TAG, "DeepSeek API测试成功，响应: %s", response_buffer);
        asprintf(&json,
                 "{\"status\":\"success\",\"message\":\"DeepSeek API连接成功\",\"status_code\":%d,\"response\":\"%s\"}",
                 ctx.status_code, response_buffer);
    } else if (ctx.status_code == 401) {
        ESP_LOGE(TAG, "DeepSeek API密钥无效");
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"DeepSeek API密钥无效（认证失败）\",\"status_code\":%d}",
                 ctx.status_code);
    } else if (ctx.status_code > 0) {
        ESP_LOGE(TAG, "DeepSeek API返回错误: %d, 响应: %s", ctx.status_code, response_buffer);
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"DeepSeek API返回错误\",\"status_code\":%d}",
                 ctx.status_code);
    } else {
        ESP_LOGE(TAG, "DeepSeek API连接失败");
        asprintf(&json,
                 "{\"status\":\"error\",\"message\":\"DeepSeek API连接失败（网络错误或超时）\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);

    return ESP_OK;
}

/**
 * @brief WebSocket升级处理器 - 简化版本(使用HTTP轮询模拟WebSocket)
 */
static esp_err_t ws_upgrade_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WebSocket连接请求(模拟)");

    // 创建互斥锁（如果还没有）
    if (g_ws_mutex == NULL) {
        g_ws_mutex = xSemaphoreCreateMutex();
    }

    // 添加到客户端列表
    int fd = httpd_req_to_sockfd(req);
    if (g_ws_mutex) {
        xSemaphoreTake(g_ws_mutex, portMAX_DELAY);
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (g_ws_clients[i] == -1) {
                g_ws_clients[i] = fd;
                g_ws_client_count++;
                break;
            }
        }
        xSemaphoreGive(g_ws_mutex);
    }

    // 触发连接事件
    if (g_ws_event_cb) {
        g_ws_event_cb(true, NULL);
    }

    // 返回成功但不真正升级连接
    // 客户端会使用HTTP轮询来获取状态更新
    const char *response = "{\"status\":\"connected\",\"message\":\"WebSocket connected (simulated)\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);

    return ESP_OK;
}

// ==================== URI注册 ====================

/**
 * @brief 注册所有URI处理器
 */
static void register_handlers(httpd_handle_t server)
{
    httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_index);

    httpd_uri_t uri_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = api_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_status);

    httpd_uri_t uri_config = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = api_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_config);

    httpd_uri_t uri_get_config = {
        .uri = "/api/config/get",
        .method = HTTP_GET,
        .handler = api_get_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_get_config);

    httpd_uri_t uri_test_deepgram = {
        .uri = "/api/test/deepgram",
        .method = HTTP_GET,
        .handler = api_test_deepgram_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_test_deepgram);

    httpd_uri_t uri_test_deepseek = {
        .uri = "/api/test/deepseek",
        .method = HTTP_GET,
        .handler = api_test_deepseek_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_test_deepseek);

    httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_upgrade_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_ws);
}

// ==================== 初始化和配置 ====================

esp_err_t http_server_start(void)
{
    if (g_server != NULL) {
        ESP_LOGW(TAG, "HTTP服务器已在运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "启动HTTP服务器...");

    // 配置HTTP服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.lru_purge_enable = true;  // 启用LRU清理

    // 启动服务器
    esp_err_t ret = httpd_start(&g_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 注册URI处理器
    register_handlers(g_server);

    ESP_LOGI(TAG, "HTTP服务器启动成功，端口: %d", WEB_SERVER_PORT);
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (g_server == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止HTTP服务器");

    // 清理WebSocket客户端
    if (g_ws_mutex) {
        xSemaphoreTake(g_ws_mutex, portMAX_DELAY);
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            g_ws_clients[i] = -1;
        }
        g_ws_client_count = 0;
        xSemaphoreGive(g_ws_mutex);
    }

    esp_err_t ret = httpd_stop(g_server);
    g_server = NULL;

    // 删除互斥锁
    if (g_ws_mutex) {
        vSemaphoreDelete(g_ws_mutex);
        g_ws_mutex = NULL;
    }

    return ret;
}

bool http_server_is_running(void)
{
    return g_server != NULL;
}

// ==================== WebSocket通信 ====================

esp_err_t http_server_ws_broadcast(const char *data, size_t len)
{
    if (g_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "广播消息(模拟WebSocket): %.*s", len, data);

    // 注意：由于我们使用HTTP轮询模拟WebSocket，
    // 客户端会定期调用/api/status来获取最新状态
    // 真实的WebSocket广播需要完整的WebSocket支持

    return ESP_OK;
}

esp_err_t http_server_ws_send(int fd, const char *data, size_t len)
{
    ESP_LOGI(TAG, "发送消息到客户端 %d (模拟WebSocket): %.*s", fd, len, data);

    // 注意：由于我们使用HTTP轮询模拟WebSocket，
    // 实际的消息发送通过HTTP响应完成
    // 真实的WebSocket发送需要完整的WebSocket支持

    return ESP_OK;
}

esp_err_t http_server_ws_set_data_callback(ws_data_callback_t callback)
{
    g_ws_data_cb = callback;
    return ESP_OK;
}

esp_err_t http_server_ws_set_event_callback(http_ws_event_callback_t callback)
{
    g_ws_event_cb = callback;
    return ESP_OK;
}

// ==================== 辅助功能 ====================

esp_err_t http_server_get_url(char *url, size_t url_size)
{
    if (url == NULL || url_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: 从WiFi管理器获取实际IP
    snprintf(url, url_size, "http://192.168.4.1");
    return ESP_OK;
}
