/**
 * @file main_simple.c
 * @brief 小智AI语音助手主程序 - 简化版（用于初次编译）
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

// 本项目模块
#include "config.h"
#include "drivers/gpio_driver.h"
#include "middleware/audio_manager.h"
#include "middleware/wifi_manager.h"
#include "middleware/nvs_storage.h"
#include "modules/wakenet_module.h"
#include "modules/asr_module_new.h"
#include "modules/tts_module_new.h"
#include "modules/config_manager.h"

static const char *TAG = "MAIN";

// ==================== 简化的系统状态 ====================

static bool g_system_ready = false;

// ==================== 函数前置声明 ====================

static void wifi_connected_callback(void);
static void wakenet_event_callback(wakenet_event_t event, const wakenet_result_t *result, void *user_data);
static void asr_event_callback(asr_event_t event, const asr_result_t *result, void *user_data);
static void tts_event_callback(tts_event_t event, const uint8_t *data, size_t len, void *user_data);
static void audio_data_callback(uint8_t *data, size_t len, void *user_data);
static void audio_event_callback(audio_event_t event, void *user_data);
static void wifi_mgr_event_callback(wifi_mgr_event_t event, void *user_data);

// ==================== WiFi事件处理 ====================

/**
 * @brief WiFi管理器事件回调
 */
static void wifi_mgr_event_callback(wifi_mgr_event_t event, void *user_data)
{
    switch (event) {
        case WIFI_MGR_EVENT_STA_CONNECTED:
            wifi_connected_callback();
            break;
        case WIFI_MGR_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi连接断开");
            break;
        default:
            break;
    }
}

/**
 * @brief WiFi连接成功回调
 */
static void wifi_connected_callback(void)
{
    ESP_LOGI(TAG, "WiFi已连接，系统就绪");
    g_system_ready = true;

    // 启动唤醒词检测
    wakenet_config_t wakenet_config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .vad_enable = true,
        .vad_threshold = 0.5f,
    };
    strncpy(wakenet_config.wake_word, "hinet_xiaozhi", sizeof(wakenet_config.wake_word) - 1);

    if (wakenet_init(&wakenet_config, wakenet_event_callback, NULL) == ESP_OK) {
        wakenet_start();
        ESP_LOGI(TAG, "进入空闲模式，等待唤醒词...");
        gpio_driver_set_level(LED_STATUS_PIN, 1);  // LED亮表示就绪
    }
}

// ==================== WakeNet事件处理 ====================

/**
 * @brief WakeNet事件回调
 */
static void wakenet_event_callback(wakenet_event_t event, const wakenet_result_t *result, void *user_data)
{
    switch (event) {
        case WAKENET_EVENT_DETECTED:
            ESP_LOGI(TAG, "检测到唤醒词！置信度: %.2f", result->score);
            wakenet_stop();

            // TODO: 连接到小智服务器并开始ASR
            ESP_LOGI(TAG, "服务器连接功能待实现...");
            break;

        default:
            break;
    }
}

// ==================== ASR事件处理 ====================

/**
 * @brief ASR模块事件回调
 */
static void asr_event_callback(asr_event_t event, const asr_result_t *result, void *user_data)
{
    switch (event) {
        case ASR_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ASR已连接");
            break;

        case ASR_EVENT_TRANSCRIPT:
            if (result && result->is_final) {
                ESP_LOGI(TAG, "识别到文本: %s", result->text);
            }
            break;

        default:
            break;
    }
}

// ==================== TTS事件处理 ====================

/**
 * @brief TTS模块事件回调
 */
static void tts_event_callback(tts_event_t event, const uint8_t *data, size_t len, void *user_data)
{
    switch (event) {
        case TTS_EVENT_START:
            ESP_LOGI(TAG, "开始TTS播放");
            break;

        case TTS_EVENT_END:
            ESP_LOGI(TAG, "TTS播放完成");
            break;

        default:
            break;
    }
}

// ==================== 音频回调 ====================

/**
 * @brief 音频数据回调（用于录音）
 */
static void audio_data_callback(uint8_t *data, size_t len, void *user_data)
{
    // 空闲状态，传递给WakeNet检测唤醒词
    if (g_system_ready) {
        wakenet_process_audio((const int16_t *)data, len / 2);
    }
}

/**
 * @brief 音频事件回调
 */
static void audio_event_callback(audio_event_t event, void *user_data)
{
    // 暂不处理
}

// ==================== 主函数 ====================

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "小智AI语音助手启动");
    ESP_LOGI(TAG, "版本: v2.0 - 小智AI方案（简化版）");
    ESP_LOGI(TAG, "=========================================");

    // 1. 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化配置管理器
    config_manager_init();
    ESP_LOGI(TAG, "配置管理器初始化完成");

    // 3. 初始化WiFi管理器
    wifi_manager_init(wifi_mgr_event_callback, NULL);
    ESP_LOGI(TAG, "WiFi管理器初始化完成");

    // 4. 初始化音频管理器
    audio_config_t audio_config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .volume = AUDIO_VOLUME,
        .vad_enabled = true,
        .vad_threshold = VAD_THRESHOLD,
        .data_cb = audio_data_callback,
        .event_cb = audio_event_callback,
    };
    audio_manager_init(&audio_config);
    ESP_LOGI(TAG, "音频管理器初始化完成");

    // 5. 初始化GPIO
    gpio_driver_init();
    ESP_LOGI(TAG, "GPIO初始化完成");

    // 6. 初始化ASR模块
    char server_url[MAX_URL_LEN];
    if (config_manager_get_server_url(server_url, sizeof(server_url)) != ESP_OK) {
        // 使用默认URL
        strncpy(server_url, XIAOZHI_SERVER_URL, sizeof(server_url) - 1);
    }

    asr_module_init(server_url, asr_event_callback, NULL);
    ESP_LOGI(TAG, "ASR模块初始化完成");

    // 7. 初始化TTS模块
    tts_module_init(tts_event_callback, NULL);
    ESP_LOGI(TAG, "TTS模块初始化完成");

    // 8. 初始化WakeNet模块（在WiFi连接后启动）
    wakenet_config_t wakenet_config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .vad_enable = true,
        .vad_threshold = 0.5f,
    };
    strncpy(wakenet_config.wake_word, "hinet_xiaozhi", sizeof(wakenet_config.wake_word) - 1);
    wakenet_init(&wakenet_config, wakenet_event_callback, NULL);
    ESP_LOGI(TAG, "WakeNet模块初始化完成");

    // 9. 启动WiFi连接（使用配置文件中的SSID和密码）
    ESP_LOGI(TAG, "开始WiFi连接...");
    // TODO: 需要定义wifi_sta_config_t结构体后调用wifi_manager_start_sta()
    ESP_LOGI(TAG, "WiFi连接功能待配置实现...");

    ESP_LOGI(TAG, "系统初始化完成，进入主循环");

    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
