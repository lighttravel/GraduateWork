/**
 * @file main_new.c
 * @brief 小智AI语音助手主程序
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "modules/iflytek_asr.h"

static const char *TAG = "MAIN";

// WiFi配置 - 使用项目配置
#define WIFI_SSID      "XUNTIAN_2.4G"
#define WIFI_PASSWORD  "xuntian13937020766"

static volatile bool wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi断开，5秒后重连...");
        wifi_connected = false;
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

static void generate_test_audio(int16_t *buffer, int sample_count)
{
    for (int i = 0; i < sample_count; i++) {
        float t = (float)i / 16000.0f;
        float sample = sin(2 * M_PI * 200 * t) * 0.3f +
                       sin(2 * M_PI * 500 * t) * 0.4f +
                       sin(2 * M_PI * 1000 * t) * 0.2f +
                       sin(2 * M_PI * 2000 * t) * 0.1f;
        buffer[i] = (int16_t)(sample * 8000 * (0.8f + 0.4f * sin(2 * M_PI * 3 * t)));
    }
}

static void asr_event_callback(iflytek_asr_event_t event,
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
            if (result) {
                ESP_LOGI(TAG, "临时识别结果: %s", result->text);
            }
            break;
        case IFLYTEK_ASR_EVENT_RESULT_FINAL:
            if (result) {
                ESP_LOGI(TAG, "最终识别结果: %s", result->text);
                ESP_LOGI(TAG, "置信度: %d%%", result->confidence);
            }
            break;
        case IFLYTEK_ASR_EVENT_ERROR:
            ESP_LOGE(TAG, "ASR发生错误");
            break;
        case IFLYTEK_ASR_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "ASR已断开连接");
            break;
        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "科大讯飞ASR语音识别测试");
    ESP_LOGI(TAG, "============================================");

    ESP_LOGI(TAG, "步骤1: 初始化NVS...");
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS需要擦除，正在擦除...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "NVS初始化成功");

    ESP_LOGI(TAG, "步骤2: 初始化网络接口...");
    // 初始化WiFi
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "网络接口初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "网络接口初始化成功");

    ESP_LOGI(TAG, "步骤3: 创建事件循环...");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "事件循环创建失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "事件循环创建成功");

    ESP_LOGI(TAG, "步骤4: 创建WiFi站接口...");
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL) {
        ESP_LOGE(TAG, "WiFi站接口创建失败");
        return;
    }
    ESP_LOGI(TAG, "WiFi站接口创建成功");

    ESP_LOGI(TAG, "步骤5: 初始化WiFi驱动...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi驱动初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "WiFi驱动初始化成功");

    ESP_LOGI(TAG, "步骤6: 注册事件处理器...");
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL,
                                               NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi事件处理器注册失败: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL,
                                               NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IP事件处理器注册失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "事件处理器注册成功");

    ESP_LOGI(TAG, "步骤7: 配置WiFi...");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi模式设置失败: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi配置设置失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "WiFi配置成功");

    ESP_LOGI(TAG, "步骤8: 启动WiFi...");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi启动失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "WiFi启动成功，正在连接...");

    // 等待WiFi连接
    int retry = 0;
    while (!wifi_connected && retry < 60) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
        ESP_LOGI(TAG, "等待WiFi连接... (%d/60)", retry);
    }

    if (!wifi_connected) {
        ESP_LOGE(TAG, "WiFi连接超时");
        return;
    }

    // 配置科大讯飞ASR
    iflytek_asr_config_t config = {
        .appid = "9ed12221",
        .api_key = "b1ffeca6c122160445ebd4a4d69003b4",
        .api_secret = "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4",
        .language = "zh_cn",
        .domain = "iat",
        .enable_punctuation = true,
        .sample_rate = 16000,
    };

    // 初始化ASR
    ESP_LOGI(TAG, "初始化科大讯飞ASR...");
    ret = iflytek_asr_init(&config, asr_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ASR初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "ASR初始化成功");

    // 连接服务器
    ESP_LOGI(TAG, "连接科大讯飞服务器...");
    ret = iflytek_asr_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ASR连接失败: %s", esp_err_to_name(ret));
        iflytek_asr_deinit();
        return;
    }
    ESP_LOGI(TAG, "ASR连接成功");

    // 等待一会确保连接稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 开始听写
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "开始语音识别测试...");
    ESP_LOGI(TAG, "测试说明：将发送3秒模拟音频到科大讯飞服务器");
    ESP_LOGI(TAG, "============================================");

    ret = iflytek_asr_start_listening();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "开始听写失败: %s", esp_err_to_name(ret));
        iflytek_asr_disconnect();
        iflytek_asr_deinit();
        return;
    }

    // 分配音频缓冲区 (3秒 16kHz 16bit单声道)
    const int sample_rate = 16000;
    const int duration_sec = 3;
    const int sample_count = sample_rate * duration_sec;
    const int buffer_size = sample_count * sizeof(int16_t);

    int16_t *audio_buffer = (int16_t *)malloc(buffer_size);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "分配音频缓冲区失败");
        iflytek_asr_stop_listening();
        iflytek_asr_disconnect();
        iflytek_asr_deinit();
        return;
    }

    // 生成测试音频
    ESP_LOGI(TAG, "生成测试音频数据...");
    generate_test_audio(audio_buffer, sample_count);

    // 发送音频数据
    ESP_LOGI(TAG, "发送音频数据: %d bytes (%d秒)", buffer_size, duration_sec);
    ret = iflytek_asr_send_audio((uint8_t *)audio_buffer, buffer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送音频数据失败: %s", esp_err_to_name(ret));
    }

    // 释放音频缓冲区
    free(audio_buffer);

    // 停止听写（触发识别）
    ESP_LOGI(TAG, "停止录音，等待识别结果...");
    ret = iflytek_asr_stop_listening();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "停止听写失败: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "科大讯飞ASR测试完成");
    ESP_LOGI(TAG, "============================================");

    // 清理
    iflytek_asr_disconnect();
    iflytek_asr_deinit();

    ESP_LOGI(TAG, "测试程序结束");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
