/**
 * @file main_minimal.c
 * @brief 极简版主程序 - 只测试基础硬件功能
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
#include "modules/wakenet_module.h"

static const char *TAG = "MAIN_MINIMAL";

// ==================== 全局变量 ====================

static bool g_wifi_connected = false;
static bool g_system_ready = false;

// ==================== WakeNet事件回调 ====================

static void wakenet_event_callback(wakenet_event_t event, const wakenet_result_t *result, void *user_data)
{
    switch (event) {
        case WAKENET_EVENT_DETECTED:
            ESP_LOGI(TAG, "检测到唤醒词！置信度: %.2f", result->score);
            wakenet_stop();

            // LED闪烁提示
            gpio_driver_set_level(LED_STATUS_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_driver_set_level(LED_STATUS_PIN, 1);

            ESP_LOGI(TAG, "唤醒词检测成功，系统就绪");
            g_system_ready = true;
            break;

        default:
            break;
    }
}

// ==================== 音频数据回调（用于录音） ====================

static void audio_data_callback(uint8_t *data, size_t len, void *user_data)
{
    // 简单打印音频能量，用于验证录音功能
    if (len >= 100) {
        int32_t energy = 0;
        for (int i = 0; i < len / 2; i++) {
            energy += ((int16_t *)data)[i] * ((int16_t *)data)[i];
        }
        energy = energy / (len / 2);

        ESP_LOGI(TAG, "音频能量: %d (样本数: %d)", energy, len / 2);
    }
}

// ==================== 音频事件回调 ====================

static void audio_event_callback(audio_event_t event, void *user_data)
{
    // 暂不处理
}

// ==================== WiFi连接回调 ====================

static void wifi_event_handler(wifi_mgr_event_t event, void *user_data)
{
    switch (event) {
        case WIFI_MGR_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi已连接");
            ESP_LOGI(TAG, "IP地址: %s", wifi_manager_get_ip());
            g_wifi_connected = true;

            // 初始化WakeNet
            wakenet_config_t wakenet_config = {
                .sample_rate = I2S_SAMPLE_RATE,
                .vad_enable = true,
                .vad_threshold = 0.5f,
            };
            strncpy(wakenet_config.wake_word, "hinet_xiaozhi", sizeof(wakenet_config.wake_word) - 1);

            if (wakenet_init(&wakenet_config, wakenet_event_callback, NULL) == ESP_OK) {
                wakenet_start();
                ESP_LOGI(TAG, "进入空闲模式，等待唤醒词...");
            }
            break;

        case WIFI_MGR_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi连接断开");
            g_wifi_connected = false;
            break;

        default:
            break;
    }
}

// ==================== 主函数 ====================

void app_main(void)
{
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "极简版ESP32 AI语音助手");
    ESP_LOGI(TAG, "版本: v1.0-minimal");
    ESP_LOGI(TAG, "只测试基础硬件功能");
    ESP_LOGI(TAG, "=========================================");

    // 1. 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化GPIO
    ret = gpio_driver_init();
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "GPIO初始化完成");

    // 3. 初始化音频管理器
    audio_config_t audio_config = {
        .sample_rate = I2S_SAMPLE_RATE,
        .volume = AUDIO_VOLUME,
        .vad_enabled = true,
        .vad_threshold = VAD_THRESHOLD,
        .data_cb = audio_data_callback,
        .event_cb = audio_event_callback,
    };
    ret = audio_manager_init(&audio_config);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "音频管理器初始化完成");

    // 4. 初始化WiFi管理器
    ret = wifi_manager_init(wifi_event_handler, NULL);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "WiFi管理器初始化完成");

    // 5. 启动WiFi连接
    ESP_LOGI(TAG, "开始WiFi连接...");
    ESP_LOGI(TAG, "SSID: %s", DEFAULT_WIFI_SSID);

    wifi_sta_config_t sta_config = {0};
    strncpy((char*)sta_config.ssid, DEFAULT_WIFI_SSID, sizeof(sta_config.ssid) - 1);
    strncpy((char*)sta_config.password, DEFAULT_WIFI_PASSWORD, sizeof(sta_config.password) - 1);

    esp_err_t wifi_ret = wifi_manager_start_sta(&sta_config);
    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi连接成功");
        ESP_LOGI(TAG, "IP地址: %s", wifi_manager_get_ip());
        g_wifi_connected = true;

        // 初始化WakeNet（在WiFi连接成功后）
        wakenet_config_t wakenet_config = {
            .sample_rate = I2S_SAMPLE_RATE,
            .vad_enable = true,
            .vad_threshold = 0.5f,
        };
        strncpy(wakenet_config.wake_word, "hinet_xiaozhi", sizeof(wakenet_config.wake_word) - 1);

        if (wakenet_init(&wakenet_config, wakenet_event_callback, NULL) == ESP_OK) {
            wakenet_start();
            ESP_LOGI(TAG, "进入空闲模式，等待唤醒词...");
        }
    } else {
        ESP_LOGE(TAG, "WiFi连接失败: %s", esp_err_to_name(wifi_ret));
        ESP_LOGE(TAG, "系统将继续运行，但AI功能将不可用");
        g_wifi_connected = false;
    }

    ESP_LOGI(TAG, "系统初始化完成");
    ESP_LOGI(TAG, "=========================================");

    // 主循环
    while (1) {
        // LED闪烁表示系统运行
        gpio_driver_set_level(LED_STATUS_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_driver_set_level(LED_STATUS_PIN, 1);

        // 检查按钮（可选功能）
        // TODO: 检查GPIO按键

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
