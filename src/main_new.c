/**
 * @file main_new.c
 * @brief 小智AI语音助手主程序 - 科大讯飞ASR测试版
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
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_netif_types.h"

#include "modules/iflytek_asr.h"

// WiFi配置
#define WIFI_SSID      "XUNTIAN_2.4G"
#define WIFI_PASSWORD  "xuntian13937020766"

static volatile bool wifi_connected = false;
static volatile bool time_synced = false;

static void time_sync_notification_callback(struct timeval *tv)
{
    printf("[SNTP] 时间同步成功！当前时间: %lld\n", (long long)tv->tv_sec);
    time_synced = true;
}

static void initialize_sntp(void)
{
    printf("[SNTP] 初始化SNTP时间同步...\n");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");  // 中国NTP服务器优先
    esp_sntp_setservername(1, "cn.ntp.org.cn");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_callback);
    esp_sntp_init();

    printf("[SNTP] 等待时间同步...\n");
    int retry = 0;
    while (!time_synced && retry < 60) {  // 增加到60秒
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
        if (retry % 10 == 0) {
            printf("[SNTP] 继续等待时间同步... (%d/60)\n", retry);
        }
    }

    if (time_synced) {
        time_t now;
        time(&now);
        printf("[SNTP] 当前时间: %s", ctime(&now));
    } else {
        printf("[SNTP] WARNING: 时间同步超时，将使用系统时间\n");
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        printf("[WiFi事件] WiFi启动，尝试连接...\n");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("[WiFi事件] WiFi断开，5秒后重连...\n");
        wifi_connected = false;
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        printf("[WiFi事件] ========== 获取到IP地址: " IPSTR " ==========\n", IP2STR(&event->ip_info.ip));
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
            printf("[ASR事件] ASR已连接到服务器\n");
            break;
        case IFLYTEK_ASR_EVENT_LISTENING_START:
            printf("[ASR事件] ASR开始监听\n");
            break;
        case IFLYTEK_ASR_EVENT_LISTENING_STOP:
            printf("[ASR事件] ASR停止监听\n");
            break;
        case IFLYTEK_ASR_EVENT_RESULT_PARTIAL:
            if (result) {
                printf("[ASR事件] 临时识别结果: %s\n", result->text);
            }
            break;
        case IFLYTEK_ASR_EVENT_RESULT_FINAL:
            if (result) {
                printf("[ASR事件] ========== 最终识别结果: %s ==========\n", result->text);
                printf("[ASR事件] 置信度: %d%%\n", result->confidence);
            }
            break;
        case IFLYTEK_ASR_EVENT_ERROR:
            printf("[ASR事件] ASR发生错误\n");
            break;
        case IFLYTEK_ASR_EVENT_DISCONNECTED:
            printf("[ASR事件] ASR已断开连接\n");
            break;
        default:
            break;
    }
}

void app_main(void)
{
    printf("========================================\n");
    printf("科大讯飞ASR语音识别测试\n");
    printf("========================================\n");
    printf("DEBUG: 程序启动\n");

    // 初始化NVS
    printf("DEBUG: 步骤1 - 初始化NVS...\n");
    esp_err_t ret = nvs_flash_init();
    printf("DEBUG: nvs_flash_init返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("DEBUG: NVS需要擦除重试\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
        printf("DEBUG: nvs_flash_init重试后: %d (%s)\n", ret, esp_err_to_name(ret));
    }
    ESP_ERROR_CHECK(ret);
    printf("DEBUG: NVS初始化成功\n");

    // 初始化网络接口（允许已初始化状态）
    printf("DEBUG: 步骤2 - 初始化网络接口...\n");
    ret = esp_netif_init();
    printf("DEBUG: esp_netif_init返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        printf("ERROR: esp_netif_init失败: %s\n", esp_err_to_name(ret));
        return;
    }

    printf("DEBUG: 步骤3 - 创建事件循环...\n");
    ret = esp_event_loop_create_default();
    printf("DEBUG: esp_event_loop_create_default返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        printf("ERROR: esp_event_loop_create_default失败: %s\n", esp_err_to_name(ret));
        return;
    }

    printf("DEBUG: 步骤4 - 创建WiFi STA接口...\n");
    esp_netif_create_default_wifi_sta();
    printf("DEBUG: WiFi STA接口创建完成\n");

    printf("DEBUG: 步骤5 - 初始化WiFi...\n");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    printf("DEBUG: esp_wifi_init返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        printf("ERROR: esp_wifi_init失败: %s\n", esp_err_to_name(ret));
        return;
    }

    printf("DEBUG: 步骤6 - 注册事件处理器...\n");
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    printf("DEBUG: 事件处理器注册完成\n");

    printf("DEBUG: 步骤7 - 配置WiFi参数...\n");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    printf("DEBUG: 步骤8 - 设置WiFi模式和配置...\n");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    printf("DEBUG: esp_wifi_set_mode返回: %d (%s)\n", ret, esp_err_to_name(ret));

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    printf("DEBUG: esp_wifi_set_config返回: %d (%s)\n", ret, esp_err_to_name(ret));

    printf("DEBUG: 步骤9 - 启动WiFi...\n");
    ret = esp_wifi_start();
    printf("DEBUG: esp_wifi_start返回: %d (%s)\n", ret, esp_err_to_name(ret));

    printf("DEBUG: 步骤10 - 主动连接WiFi...\n");
    ret = esp_wifi_connect();
    printf("DEBUG: esp_wifi_connect返回: %d (%s)\n", ret, esp_err_to_name(ret));

    printf("DEBUG: WiFi初始化完成，正在等待IP地址...\n");

    // 等待WiFi连接
    int retry = 0;
    while (!wifi_connected && retry < 60) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
        printf("DEBUG: 等待WiFi连接... (%d/60), wifi_connected=%d\n", retry, wifi_connected);
    }

    if (!wifi_connected) {
        printf("ERROR: WiFi连接超时\n");
        return;
    }

    printf("SUCCESS: WiFi已连接并获取IP\n");

    // 等待网络稳定
    printf("DEBUG: 等待网络稳定...\n");
    vTaskDelay(pdMS_TO_TICKS(3000));  // 额外等待3秒确保网络稳定

    // 初始化SNTP时间同步
    printf("DEBUG: 步骤10.5 - 初始化SNTP时间同步...\n");
    initialize_sntp();

    // 打印内存信息
    printf("DEBUG: 当前可用堆内存: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("DEBUG: 最大可分配块: %lu bytes\n", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // 配置科大讯飞ASR
    printf("DEBUG: 步骤11 - 配置科大讯飞ASR...\n");
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
    printf("DEBUG: 步骤12 - 初始化ASR模块...\n");
    ret = iflytek_asr_init(&config, asr_event_callback, NULL);
    printf("DEBUG: iflytek_asr_init返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK) {
        printf("ERROR: ASR初始化失败: %s\n", esp_err_to_name(ret));
        return;
    }
    printf("SUCCESS: ASR初始化成功\n");

    // 连接服务器
    printf("DEBUG: 步骤13 - 连接科大讯飞服务器...\n");
    ret = iflytek_asr_connect();
    printf("DEBUG: iflytek_asr_connect返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK) {
        printf("ERROR: ASR连接失败: %s\n", esp_err_to_name(ret));
        iflytek_asr_deinit();
        return;
    }
    printf("SUCCESS: ASR连接成功\n");

    // 等待一会确保连接稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 开始听写
    printf("============================================\n");
    printf("开始语音识别测试...\n");
    printf("测试说明：将发送1秒模拟音频到科大讯飞服务器\n");
    printf("============================================\n");

    ret = iflytek_asr_start_listening();
    printf("DEBUG: iflytek_asr_start_listening返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK) {
        printf("ERROR: 开始听写失败: %s\n", esp_err_to_name(ret));
        iflytek_asr_disconnect();
        iflytek_asr_deinit();
        return;
    }
    printf("SUCCESS: 开始听写成功\n");

    // 分配音频缓冲区 (1秒 16kHz 16bit单声道 - 适配ASR模块的32KB缓冲区)
    const int sample_rate = 16000;
    const int duration_sec = 1;
    const int sample_count = sample_rate * duration_sec;
    const int buffer_size = sample_count * sizeof(int16_t);
    // 每帧大小：40ms @ 16kHz 16bit = 1280 bytes（官方推荐）
    const int frame_size = 1280;

    printf("DEBUG: 分配音频缓冲区: %d bytes (%d秒 @ %dHz)\n", buffer_size, duration_sec, sample_rate);
    int16_t *audio_buffer = (int16_t *)malloc(buffer_size);
    if (audio_buffer == NULL) {
        printf("ERROR: 分配音频缓冲区失败\n");
        iflytek_asr_stop_listening();
        iflytek_asr_disconnect();
        iflytek_asr_deinit();
        return;
    }
    printf("SUCCESS: 音频缓冲区分配成功\n");

    // 生成测试音频
    printf("DEBUG: 生成测试音频数据...\n");
    generate_test_audio(audio_buffer, sample_count);
    printf("SUCCESS: 测试音频生成完成\n");

    // 分帧发送音频数据（每帧1280字节，间隔40ms）
    printf("DEBUG: 分帧发送音频数据: %d bytes, 每帧 %d bytes...\n", buffer_size, frame_size);
    int frames_sent = 0;
    int offset = 0;
    while (offset < buffer_size) {
        int chunk_size = (buffer_size - offset) > frame_size ? frame_size : (buffer_size - offset);
        ret = iflytek_asr_send_audio((uint8_t *)(audio_buffer) + offset, chunk_size);
        if (ret != ESP_OK) {
            printf("ERROR: 发送第%d帧失败: %s\n", frames_sent + 1, esp_err_to_name(ret));
            break;
        }
        frames_sent++;
        offset += chunk_size;
        // 每40ms发送一帧
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    printf("SUCCESS: 发送完成，共发送 %d 帧 (%d bytes)\n", frames_sent, offset);

    // 释放音频缓冲区
    printf("DEBUG: 释放音频缓冲区...\n");
    free(audio_buffer);
    printf("SUCCESS: 音频缓冲区已释放\n");

    // 等待服务器处理和接收结果（增加等待时间）
    printf("DEBUG: 等待服务器响应和识别结果...\n");
    vTaskDelay(pdMS_TO_TICKS(3000));  // 等待3秒让接收任务处理结果

    // 停止听写（触发识别）
    printf("DEBUG: 停止录音，等待最终识别结果...\n");
    ret = iflytek_asr_stop_listening();
    printf("DEBUG: iflytek_asr_stop_listening返回: %d (%s)\n", ret, esp_err_to_name(ret));
    if (ret != ESP_OK) {
        printf("ERROR: 停止听写失败: %s\n", esp_err_to_name(ret));
    } else {
        printf("SUCCESS: 停止听写成功\n");

    // 等待最终识别结果
    printf("DEBUG: 等待最终识别结果返回...\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    }

    printf("============================================\n");
    printf("科大讯飞ASR测试完成\n");
    printf("============================================\n");

    // 清理
    printf("DEBUG: 清理资源...\n");
    iflytek_asr_disconnect();
    iflytek_asr_deinit();
    printf("SUCCESS: 资源清理完成\n");

    printf("测试程序结束，进入空闲循环\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
