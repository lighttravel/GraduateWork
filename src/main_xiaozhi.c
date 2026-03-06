/**
 * @file main_xiaozhi.c
 * @brief 小智AI语音助手主程序 - 完整集成版
 *
 * 整合模块:
 * - WiFi管理 (wifi_manager)
 * - 音频管理 (audio_manager)
 * - 唤醒词检测 (wakenet_module)
 * - 语音识别 (iflytek_asr)
 * - AI对话 (chat_module)
 * - 语音合成 (tts_module)
 *
 * 工作流程:
 * 1. 连接WiFi
 * 2. 初始化音频设备
 * 3. 监听唤醒词
 * 4. 检测到唤醒词后开始录音
 * 5. 将音频发送到科大讯飞ASR获取文本
 * 6. 将文本发送到AI对话模块获取回复
 * 7. 将回复通过TTS播放
 * 8. 返回步骤3等待下一次唤醒
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#include "config.h"
#include "middleware/wifi_manager.h"
#include "middleware/audio_manager.h"
#include "modules/wakenet_module.h"
#include "modules/iflytek_asr.h"
#include "modules/chat_module.h"
#include "modules/tts_module.h"

static const char *TAG = "XIAOZHI";

// ==================== 系统状态机 ====================

typedef enum {
    SYS_STATE_IDLE = 0,          // 空闲，等待唤醒
    SYS_STATE_WAKE_WORD,         // 检测到唤醒词
    SYS_STATE_LISTENING,         // 正在听用户说话
    SYS_STATE_PROCESSING_ASR,    // 正在处理ASR
    SYS_STATE_THINKING,          // AI正在思考
    SYS_STATE_SPEAKING,          // 正在播放回复
    SYS_STATE_ERROR              // 错误状态
} system_state_t;

// ==================== 全局变量 ====================

static system_state_t g_system_state = SYS_STATE_IDLE;
static volatile bool g_wifi_connected = false;
static volatile bool g_asr_result_ready = false;
static char g_asr_text[512] = {0};
static char g_ai_response[1024] = {0};

// 事件组
static EventGroupHandle_t g_system_events;
#define EVENT_WIFI_CONNECTED    (1 << 0)
#define EVENT_WAKE_WORD         (1 << 1)
#define EVENT_ASR_DONE          (1 << 2)
#define EVENT_CHAT_DONE         (1 << 3)
#define EVENT_TTS_DONE          (1 << 4)

// ==================== 配置 ====================

// 科大讯飞ASR配置
#define IFLYTEK_APPID      "9ed12221"
#define IFLYTEK_API_KEY    "b1ffeca6c122160445ebd4a4d69003b4"
#define IFLYTEK_API_SECRET "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4"

// DeepSeek API配置 (用于AI对话)
#define DEEPSEEK_API_KEY   "sk-e858af6399024030a35798cac18a961a"

// ==================== 回调函数 ====================

/**
 * @brief WiFi事件回调
 */
static void wifi_event_callback(wifi_mgr_event_t event, void *user_data)
{
    switch (event) {
        case WIFI_MGR_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi已连接");
            g_wifi_connected = true;
            xEventGroupSetBits(g_system_events, EVENT_WIFI_CONNECTED);
            break;

        case WIFI_MGR_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi已断开");
            g_wifi_connected = false;
            xEventGroupClearBits(g_system_events, EVENT_WIFI_CONNECTED);
            break;

        default:
            break;
    }
}

/**
 * @brief 唤醒词事件回调
 */
static void wakenet_event_callback(wakenet_event_t event, const wakenet_result_t *result, void *user_data)
{
    switch (event) {
        case WAKENET_EVENT_DETECTED:
            ESP_LOGI(TAG, "========== 检测到唤醒词！==========");
            ESP_LOGI(TAG, "置信度: %.2f", result->score);
            xEventGroupSetBits(g_system_events, EVENT_WAKE_WORD);
            break;

        case WAKENET_EVENT_ERROR:
            ESP_LOGE(TAG, "唤醒词检测错误");
            break;

        default:
            break;
    }
}

/**
 * @brief iFlytek ASR事件回调
 */
static void iflytek_asr_event_callback(iflytek_asr_event_t event,
                                        const iflytek_asr_result_t *result,
                                        void *user_data)
{
    switch (event) {
        case IFLYTEK_ASR_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ASR已连接到服务器");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_START:
            ESP_LOGI(TAG, "ASR开始监听");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_STOP:
            ESP_LOGI(TAG, "ASR停止监听");
            break;

        case IFLYTEK_ASR_EVENT_RESULT_PARTIAL:
            if (result && strlen(result->text) > 0) {
                ESP_LOGI(TAG, "临时识别: %s", result->text);
            }
            break;

        case IFLYTEK_ASR_EVENT_RESULT_FINAL:
            if (result && strlen(result->text) > 0) {
                ESP_LOGI(TAG, "========== 最终识别结果: %s ==========", result->text);
                strncpy(g_asr_text, result->text, sizeof(g_asr_text) - 1);
                g_asr_text[sizeof(g_asr_text) - 1] = '\0';
                g_asr_result_ready = true;
                xEventGroupSetBits(g_system_events, EVENT_ASR_DONE);
            }
            break;

        case IFLYTEK_ASR_EVENT_ERROR:
            ESP_LOGE(TAG, "ASR发生错误");
            g_system_state = SYS_STATE_ERROR;
            break;

        case IFLYTEK_ASR_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "ASR已断开连接");
            break;

        default:
            break;
    }
}

/**
 * @brief Chat模块事件回调
 */
static void chat_event_callback(chat_event_t event, const char *data, bool is_done, void *user_data)
{
    switch (event) {
        case CHAT_EVENT_START:
            ESP_LOGI(TAG, "AI开始生成回复...");
            g_ai_response[0] = '\0';
            break;

        case CHAT_EVENT_DATA:
            if (data) {
                // 追加到响应缓冲区
                strncat(g_ai_response, data, sizeof(g_ai_response) - strlen(g_ai_response) - 1);
            }
            break;

        case CHAT_EVENT_DONE:
            ESP_LOGI(TAG, "========== AI回复: %s ==========", g_ai_response);
            xEventGroupSetBits(g_system_events, EVENT_CHAT_DONE);
            break;

        case CHAT_EVENT_ERROR:
            ESP_LOGE(TAG, "Chat模块错误");
            g_system_state = SYS_STATE_ERROR;
            break;

        default:
            break;
    }
}

/**
 * @brief TTS模块事件回调
 */
static void tts_event_callback(tts_event_t event, void *user_data)
{
    switch (event) {
        case TTS_EVENT_START:
            ESP_LOGI(TAG, "开始播放TTS音频...");
            break;

        case TTS_EVENT_DONE:
            ESP_LOGI(TAG, "TTS播放完成");
            xEventGroupSetBits(g_system_events, EVENT_TTS_DONE);
            break;

        case TTS_EVENT_ERROR:
            ESP_LOGE(TAG, "TTS模块错误");
            g_system_state = SYS_STATE_ERROR;
            break;

        default:
            break;
    }
}

/**
 * @brief TTS音频数据回调
 */
static void tts_data_callback(const uint8_t *data, size_t len, void *user_data)
{
    ESP_LOGI(TAG, "========== tts_data_callback 被调用! ==========");
    ESP_LOGI(TAG, "  data=%p, len=%d", (void*)data, (int)len);

    if (data && len > 0) {
        ESP_LOGI(TAG, "TTS音频数据: %d 字节，准备播放", (int)len);
        // 将TTS音频数据发送到音频管理器播放
        esp_err_t ret = audio_manager_play_tts_audio(data, len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "播放TTS音频失败: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "播放TTS音频成功: %d 字节", (int)len);
        }
    } else {
        ESP_LOGW(TAG, "TTS数据为空或长度为0");
    }
}

/**
 * @brief 录音数据回调 - 将音频发送到ASR或唤醒词检测
 * @param data 音频数据 (PCM 16-bit 16kHz)
 * @param len 数据长度
 * @param user_data 用户数据
 */
static void audio_record_callback(uint8_t *data, size_t len, void *user_data)
{
    // 根据系统状态决定音频数据的处理方式
    if (g_system_state == SYS_STATE_LISTENING && iflytek_asr_is_listening()) {
        // ASR模式：将录音数据发送到讯飞ASR
        esp_err_t ret = iflytek_asr_send_audio(data, len);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "发送音频到ASR失败: %s", esp_err_to_name(ret));
        }
    } else if (g_system_state == SYS_STATE_IDLE && wakenet_is_listening()) {
        // 唤醒词检测模式：将音频送入唤醒词检测
        wakenet_process_audio((const int16_t *)data, len / 2);
    }
}

// ==================== 初始化函数 ====================

/**
 * @brief 初始化NVS
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS需要擦除...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief 初始化SNTP时间同步
 */
static esp_err_t init_sntp(void)
{
    ESP_LOGI(TAG, "初始化SNTP时间同步...");

    // 设置SNTP操作模式
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // 设置SNTP服务器 (优先使用中国服务器)
    esp_sntp_setservername(0, "ntp.aliyun.com");      // 阿里云NTP (中国最快)
    esp_sntp_setservername(1, "cn.ntp.org.cn");       // 中国NTP服务器
    esp_sntp_setservername(2, "ntp.tencent.com");     // 腾讯云NTP

    // 启动SNTP
    esp_sntp_init();

    // 等待时间同步 (最多30秒，更长的等待时间)
    int retry = 0;
    const int retry_count = 60;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "等待时间同步... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        time_t now;
        char strftime_buf[64];
        time(&now);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "SNTP时间同步成功: %s", strftime_buf);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "SNTP时间同步超时，继续运行");
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * @brief 初始化WiFi
 */
static esp_err_t init_wifi(void)
{
    ESP_LOGI(TAG, "初始化WiFi...");

    esp_err_t ret = wifi_manager_init(wifi_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi管理器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置STA模式
    wifi_sta_config_t sta_config = {0};
    strncpy((char *)sta_config.ssid, DEFAULT_WIFI_SSID, sizeof(sta_config.ssid) - 1);
    strncpy((char *)sta_config.password, DEFAULT_WIFI_PASSWORD, sizeof(sta_config.password) - 1);

    ret = wifi_manager_start_sta(&sta_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动STA模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 等待WiFi连接
    ESP_LOGI(TAG, "等待WiFi连接...");
    EventBits_t bits = xEventGroupWaitBits(g_system_events, EVENT_WIFI_CONNECTED,
                                            pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));

    if (!(bits & EVENT_WIFI_CONNECTED)) {
        ESP_LOGE(TAG, "WiFi连接超时");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "WiFi连接成功，IP: %s", wifi_manager_get_ip());
    return ESP_OK;
}

/**
 * @brief 初始化音频管理器
 */
static esp_err_t init_audio(void)
{
    ESP_LOGI(TAG, "初始化音频管理器...");

    audio_config_t config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .volume = AUDIO_VOLUME,
        .vad_enabled = true,
        .vad_threshold = VAD_THRESHOLD,
        .data_cb = audio_record_callback,  // 设置录音数据回调
        .event_cb = NULL,
        .user_data = NULL,
    };

    esp_err_t ret = audio_manager_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "音频管理器初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "音频管理器初始化成功");
    return ESP_OK;
}

/**
 * @brief 初始化唤醒词模块
 */
static esp_err_t init_wakenet(void)
{
    ESP_LOGI(TAG, "初始化唤醒词模块...");

    wakenet_config_t config = {
        .wake_word = "hinet_xiaozhi",  // 小智唤醒词
        .sample_rate = 16000,
        .vad_enable = true,
        .vad_threshold = 0.5f,
        .aec_enable = false,  // 暂时禁用AEC
        .aec_filter = false,
    };

    esp_err_t ret = wakenet_init(&config, wakenet_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "唤醒词模块初始化失败: %s", esp_err_to_name(ret));
        // 唤醒词模块可能不支持，使用按键触发代替
        ESP_LOGW(TAG, "将使用手动触发代替唤醒词");
        return ESP_OK;  // 继续运行
    }

    ESP_LOGI(TAG, "唤醒词模块初始化成功");
    return ESP_OK;
}

/**
 * @brief 初始化科大讯飞ASR
 */
static esp_err_t init_asr(void)
{
    ESP_LOGI(TAG, "初始化科大讯飞ASR...");

    iflytek_asr_config_t config = {
        .appid = IFLYTEK_APPID,
        .api_key = IFLYTEK_API_KEY,
        .api_secret = IFLYTEK_API_SECRET,
        .language = "zh_cn",
        .domain = "iat",
        .enable_punctuation = true,
        .enable_nlp = false,
        .sample_rate = 16000,
    };

    esp_err_t ret = iflytek_asr_init(&config, iflytek_asr_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ASR初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "科大讯飞ASR初始化成功");
    return ESP_OK;
}

/**
 * @brief 初始化Chat模块
 */
static esp_err_t init_chat(void)
{
    ESP_LOGI(TAG, "初始化Chat模块...");

    // TODO: 从NVS读取API密钥
    const char *api_key = DEEPSEEK_API_KEY;
    if (strlen(api_key) == 0) {
        ESP_LOGW(TAG, "DeepSeek API密钥未配置，Chat功能将不可用");
        return ESP_OK;  // 继续运行
    }

    esp_err_t ret = chat_module_init(api_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Chat模块初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置系统提示词
    chat_module_set_system_prompt("你是小智，一个友好、乐于助人的AI语音助手。"
                                   "请用简洁、自然的语言回答用户的问题。"
                                   "回答不要太长，适合语音播放。");

    ESP_LOGI(TAG, "Chat模块初始化成功");
    return ESP_OK;
}

/**
 * @brief 初始化TTS模块
 */
static esp_err_t init_tts(void)
{
    ESP_LOGI(TAG, "初始化TTS模块...");

    // 使用讯飞TTS（支持中文）
    esp_err_t ret = tts_module_init(TTS_PROVIDER_IFLYTEK);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS模块初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "TTS模块初始化成功（讯飞TTS）");
    return ESP_OK;
}

// ==================== 状态处理函数 ====================

/**
 * @brief 等待唤醒词状态
 */
static void state_wait_wake_word(void)
{
    ESP_LOGI(TAG, "等待唤醒词...");
    g_system_state = SYS_STATE_IDLE;

    // 开始监听唤醒词
    wakenet_start();

    // 开始录音（音频数据通过audio_record_callback送入wakenet）
    esp_err_t ret = audio_manager_start_record();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "开始录音失败");
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 等待唤醒词事件（或手动触发）
    EventBits_t bits = xEventGroupWaitBits(g_system_events, EVENT_WAKE_WORD,
                                            pdTRUE, pdFALSE, portMAX_DELAY);

    // 检测到唤醒词后停止录音
    audio_manager_stop_record();
    wakenet_stop();

    if (bits & EVENT_WAKE_WORD) {
        ESP_LOGI(TAG, "进入监听状态");
        g_system_state = SYS_STATE_WAKE_WORD;
    }
}

/**
 * @brief 监听用户说话状态
 */
static void state_listening(void)
{
    ESP_LOGI(TAG, "开始录音...");
    g_system_state = SYS_STATE_LISTENING;
    g_asr_result_ready = false;
    g_asr_text[0] = '\0';

    // 连接ASR服务器
    esp_err_t ret = iflytek_asr_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ASR连接失败");
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 等待连接稳定
    vTaskDelay(pdMS_TO_TICKS(500));

    // 开始听写
    ret = iflytek_asr_start_listening();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "开始听写失败");
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 开始录音（音频数据通过audio_record_callback自动发送到ASR）
    ret = audio_manager_start_record();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "开始录音失败");
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 录音最多8秒，或检测到静音后停止
    int record_duration_ms = 8000;
    int elapsed_ms = 0;
    const int chunk_ms = 100;

    while (elapsed_ms < record_duration_ms && g_system_state == SYS_STATE_LISTENING) {
        vTaskDelay(pdMS_TO_TICKS(chunk_ms));
        elapsed_ms += chunk_ms;

        // 检查是否已经有识别结果
        if (g_asr_result_ready) {
            ESP_LOGI(TAG, "已获得识别结果，停止录音");
            break;
        }
    }

    // 停止录音
    audio_manager_stop_record();

    // 停止听写
    iflytek_asr_stop_listening();

    // 等待最终结果
    if (!g_asr_result_ready) {
        ESP_LOGI(TAG, "等待ASR最终结果...");
        xEventGroupWaitBits(g_system_events, EVENT_ASR_DONE,
                           pdTRUE, pdFALSE, pdMS_TO_TICKS(3000));
    }

    // 断开ASR连接
    iflytek_asr_disconnect();

    if (strlen(g_asr_text) > 0) {
        g_system_state = SYS_STATE_PROCESSING_ASR;
    } else {
        ESP_LOGW(TAG, "未识别到语音，返回等待状态");
        g_system_state = SYS_STATE_IDLE;
    }
}

/**
 * @brief 处理ASR结果，发送到Chat
 */
static void state_process_asr(void)
{
    ESP_LOGI(TAG, "用户说: %s", g_asr_text);
    g_system_state = SYS_STATE_THINKING;

    // 发送到Chat模块
    esp_err_t ret = chat_module_send_message(g_asr_text, chat_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送消息到Chat失败");
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 等待Chat完成
    EventBits_t bits = xEventGroupWaitBits(g_system_events, EVENT_CHAT_DONE,
                                            pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));

    if (bits & EVENT_CHAT_DONE) {
        g_system_state = SYS_STATE_SPEAKING;
    } else {
        ESP_LOGE(TAG, "Chat响应超时");
        g_system_state = SYS_STATE_ERROR;
    }
}

/**
 * @brief 播放TTS回复
 */
static void state_speaking(void)
{
    ESP_LOGI(TAG, "播放AI回复...");
    g_system_state = SYS_STATE_SPEAKING;

    if (strlen(g_ai_response) == 0) {
        ESP_LOGW(TAG, "AI回复为空");
        g_system_state = SYS_STATE_IDLE;
        return;
    }

    // 启动TTS播放
    esp_err_t ret = audio_manager_start_tts_playback();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动TTS播放失败");
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 调用TTS模块合成并播放
    ret = tts_module_speak(g_ai_response, tts_data_callback, tts_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS合成失败");
        audio_manager_stop_tts_playback();
        g_system_state = SYS_STATE_ERROR;
        return;
    }

    // 等待TTS完成
    xEventGroupWaitBits(g_system_events, EVENT_TTS_DONE,
                        pdTRUE, pdFALSE, portMAX_DELAY);

    // 停止TTS播放
    audio_manager_stop_tts_playback();

    ESP_LOGI(TAG, "回复播放完成");
    g_system_state = SYS_STATE_IDLE;
}

/**
 * @brief 错误处理状态
 */
static void state_error(void)
{
    ESP_LOGE(TAG, "系统进入错误状态，5秒后重置...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    g_system_state = SYS_STATE_IDLE;
}

/**
 * @brief 测试喇叭播放 - 使用简单PCM测试音
 */
static void test_speaker(void)
{
    ESP_LOGI(TAG, "========== 开始喇叭测试 ==========");

    // 设置较高音量
    audio_manager_set_volume(100);

    // ===== 步骤0: 转储 ES8311 寄存器诊断 =====
    ESP_LOGI(TAG, "===== 步骤0: ES8311 寄存器诊断 =====");
    audio_manager_dump_codec_registers();

    // ===== 第一步：简单 PCM 测试音 (16kHz, 16-bit, mono) =====
    ESP_LOGI(TAG, "===== 步骤1: PCM 测试音 (1kHz 正弦波 0.5秒) =====");

    // 创建 1kHz 正弦波测试音 (16kHz 采样率，0.5秒 = 8000 采样点)
    // 1kHz @ 16kHz = 每个周期 16 个采样点
    #define TEST_TONE_SAMPLES 8000
    static int16_t test_tone[TEST_TONE_SAMPLES];

    // 生成 1kHz 正弦波 (使用查表法避免浮点运算)
    // 一个周期的正弦波 (16个点)
    static const int16_t sine_table[16] = {
        0, 11585, 21213, 27246, 30000, 27246, 21213, 11585,
        0, -11585, -21213, -27246, -30000, -27246, -21213, -11585
    };

    for (int i = 0; i < TEST_TONE_SAMPLES; i++) {
        test_tone[i] = sine_table[i % 16];  // 循环查表
    }

    ESP_LOGI(TAG, "PCM测试音: %d 采样点, %d 字节, 持续 %.2f 秒",
             TEST_TONE_SAMPLES, TEST_TONE_SAMPLES * 2, (float)TEST_TONE_SAMPLES / 16000.0f);

    // 启动播放模式
    esp_err_t ret = audio_manager_start_tts_playback();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动播放失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "播放模式已启动");

    // 播放测试音 (3次，每次间隔0.7秒)
    ESP_LOGI(TAG, "开始播放 PCM 测试音 (3次)...");
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "  播放第 %d/3 次...", i + 1);
        audio_manager_play_tts_audio((const uint8_t *)test_tone, TEST_TONE_SAMPLES * 2);
        vTaskDelay(pdMS_TO_TICKS(700));  // 间隔 0.7 秒
    }
    ESP_LOGI(TAG, "PCM 测试音播放完成");

    // 等待一下
    vTaskDelay(pdMS_TO_TICKS(500));

    // 停止播放模式
    audio_manager_stop_tts_playback();

    // ===== 第二步：TTS 语音测试 =====
    ESP_LOGI(TAG, "===== 步骤2: TTS 语音测试 =====");

    const char *test_text = "你好小智";

    // 启动TTS播放模式
    ret = audio_manager_start_tts_playback();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动TTS播放失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "TTS播放模式已启动，I2S已配置");

    // 调用TTS合成并播放
    ESP_LOGI(TAG, "开始TTS合成: %s", test_text);
    ret = tts_module_speak(test_text, tts_data_callback, tts_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS合成失败: %s", esp_err_to_name(ret));
        audio_manager_stop_tts_playback();
        return;
    }
    ESP_LOGI(TAG, "TTS请求已发送，等待音频数据...");

    // 等待TTS完成（最多60秒，讯飞服务器响应可能较慢）
    EventBits_t bits = xEventGroupWaitBits(g_system_events, EVENT_TTS_DONE,
                                            pdTRUE, pdFALSE, pdMS_TO_TICKS(60000));

    // 停止TTS播放
    audio_manager_stop_tts_playback();

    if (bits & EVENT_TTS_DONE) {
        ESP_LOGI(TAG, "========== 喇叭测试完成 ==========");
    } else {
        ESP_LOGW(TAG, "========== 喇叭测试超时 ==========");
    }
}

// ==================== 主程序 ====================

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("    小智AI语音助手 v2.0\n");
    printf("    基于ESP32 + 科大讯飞ASR\n");
    printf("========================================\n");
    printf("\n");

    // 创建事件组
    g_system_events = xEventGroupCreate();
    if (!g_system_events) {
        ESP_LOGE(TAG, "创建事件组失败");
        return;
    }

    // 初始化NVS
    ESP_ERROR_CHECK(init_nvs());

    // 初始化WiFi
    ESP_ERROR_CHECK(init_wifi());

    // 初始化SNTP时间同步 (TLS连接需要正确的时间)
    init_sntp();

    // 初始化音频
    ESP_ERROR_CHECK(init_audio());

    // 初始化各模块
    init_wakenet();   // 唤醒词（可选）
    ESP_ERROR_CHECK(init_asr());    // ASR（必须）
    init_chat();      // Chat（可选）
    ESP_ERROR_CHECK(init_tts());    // TTS（必须，用于喇叭测试）

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "系统初始化完成，开始运行主循环");
    ESP_LOGI(TAG, "========================================");

    // ===== 喇叭测试 =====
    test_speaker();
    ESP_LOGI(TAG, "喇叭测试结束，3秒后进入正常工作模式...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    // ====================

    // 主循环 - 状态机
    while (1) {
        switch (g_system_state) {
            case SYS_STATE_IDLE:
                state_wait_wake_word();
                break;

            case SYS_STATE_WAKE_WORD:
                state_listening();
                break;

            case SYS_STATE_PROCESSING_ASR:
                state_process_asr();
                break;

            case SYS_STATE_SPEAKING:
                state_speaking();
                break;

            case SYS_STATE_ERROR:
                state_error();
                break;

            default:
                g_system_state = SYS_STATE_IDLE;
                break;
        }

        // 短暂延时防止忙循环
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
