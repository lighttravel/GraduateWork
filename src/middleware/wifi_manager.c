/**
 * @file wifi_manager.c
 * @brief WiFi管理器实现
 */

#include "wifi_manager.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_MGR";

// WiFi事件位
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

// ==================== WiFi管理器上下文 ====================

typedef struct {
    wifi_mode_t mode;
    wifi_state_t state;
    wifi_event_callback_t event_cb;
    void *user_data;

    // WiFi事件组
    EventGroupHandle_t wifi_event_group;

    // 扫描结果
    wifi_ap_record_t *ap_records;
    uint16_t ap_count;

    // 重试计数
    int retry_num;

    // IP地址
    char ip_str[16];

    // 互斥锁
    SemaphoreHandle_t mutex;
} wifi_manager_t;

static wifi_manager_t *g_wifi_mgr = NULL;
static esp_netif_t *g_sta_netif = NULL;
static esp_netif_t *g_ap_netif = NULL;

// ==================== 事件处理 ====================

/**
 * @brief WiFi事件处理器
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA启动");
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_STA_START, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        ESP_LOGI(TAG, "WiFi STA停止");
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_STA_STOP, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi STA已连接");
        g_wifi_mgr->state = WIFI_STATE_CONNECTED;
        g_wifi_mgr->retry_num = 0;
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_STA_CONNECTED, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi STA已断开");
        g_wifi_mgr->state = WIFI_STATE_DISCONNECTED;

        if (g_wifi_mgr->retry_num < WIFI_RETRY_COUNT) {
            esp_wifi_connect();
            g_wifi_mgr->retry_num++;
            ESP_LOGI(TAG, "重连中... (%d/%d)", g_wifi_mgr->retry_num, WIFI_RETRY_COUNT);
        } else {
            g_wifi_mgr->state = WIFI_STATE_ERROR;
            xEventGroupSetBits(g_wifi_mgr->wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "连接失败");
        }

        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_STA_DISCONNECTED, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP启动");
        g_wifi_mgr->state = WIFI_STATE_CONNECTED;
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_AP_START, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "WiFi AP停止");
        g_wifi_mgr->state = WIFI_STATE_IDLE;
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_AP_STOP, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        (void)event;  // Suppress unused warning
        ESP_LOGI(TAG, "STA connected: %02x:%02x:%02x:%02x:%02x:%02x, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_AP_STA_CONNECTED, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        (void)event;  // Suppress unused warning
        ESP_LOGI(TAG, "STA disconnected: %02x:%02x:%02x:%02x:%02x:%02x, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_AP_STA_DISCONNECTED, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "WiFi扫描完成");
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_SCAN_DONE, g_wifi_mgr->user_data);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获取IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // 保存IP地址
        snprintf(g_wifi_mgr->ip_str, sizeof(g_wifi_mgr->ip_str),
                 IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(g_wifi_mgr->wifi_event_group, WIFI_CONNECTED_BIT);

        // 触发回调通知main.c WiFi完全连接成功
        if (g_wifi_mgr->event_cb) {
            g_wifi_mgr->event_cb(WIFI_MGR_EVENT_STA_CONNECTED, g_wifi_mgr->user_data);
        }
    }
}

// ==================== 初始化和配置 ====================

esp_err_t wifi_manager_init(wifi_event_callback_t event_cb, void *user_data)
{
    if (g_wifi_mgr != NULL) {
        ESP_LOGW(TAG, "WiFi管理器已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化WiFi管理器...");

    // 分配内存
    g_wifi_mgr = (wifi_manager_t *)calloc(1, sizeof(wifi_manager_t));
    if (g_wifi_mgr == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存回调
    g_wifi_mgr->event_cb = event_cb;
    g_wifi_mgr->user_data = user_data;
    g_wifi_mgr->mode = WIFI_MODE_NULL;
    g_wifi_mgr->state = WIFI_STATE_IDLE;
    g_wifi_mgr->retry_num = 0;

    // 创建事件组
    g_wifi_mgr->wifi_event_group = xEventGroupCreate();
    if (g_wifi_mgr->wifi_event_group == NULL) {
        ESP_LOGE(TAG, "事件组创建失败");
        free(g_wifi_mgr);
        g_wifi_mgr = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 创建互斥锁
    g_wifi_mgr->mutex = xSemaphoreCreateMutex();
    if (g_wifi_mgr->mutex == NULL) {
        ESP_LOGE(TAG, "互斥锁创建失败");
        vEventGroupDelete(g_wifi_mgr->wifi_event_group);
        free(g_wifi_mgr);
        g_wifi_mgr = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 初始化TCP/IP栈
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    ESP_LOGI(TAG, "WiFi管理器初始化完成");
    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void)
{
    if (g_wifi_mgr == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化WiFi管理器...");

    // 停止WiFi
    esp_wifi_stop();

    // 注销事件处理
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);

    // 反初始化WiFi
    esp_wifi_deinit();

    // 删除事件组和互斥锁
    if (g_wifi_mgr->wifi_event_group) {
        vEventGroupDelete(g_wifi_mgr->wifi_event_group);
    }
    if (g_wifi_mgr->mutex) {
        vSemaphoreDelete(g_wifi_mgr->mutex);
    }

    // 释放扫描结果
    if (g_wifi_mgr->ap_records) {
        free(g_wifi_mgr->ap_records);
    }

    // 释放内存
    free(g_wifi_mgr);
    g_wifi_mgr = NULL;

    ESP_LOGI(TAG, "WiFi管理器反初始化完成");
    return ESP_OK;
}

// ==================== WiFi模式控制 ====================

esp_err_t wifi_manager_start_ap(const wifi_ap_config_t *config)
{
    if (g_wifi_mgr == NULL) {
        ESP_LOGE(TAG, "WiFi管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "启动AP模式...");

    xSemaphoreTake(g_wifi_mgr->mutex, portMAX_DELAY);

    // 创建AP网络接口
    if (g_ap_netif == NULL) {
        g_ap_netif = esp_netif_create_default_wifi_ap();
    }

    // 设置WiFi模式为AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    // 配置AP
    wifi_config_t wifi_config = {
        .ap = {
            .channel = config ? config->channel : WIFI_AP_CHANNEL,
            .max_connection = config ? config->max_connection : WIFI_AP_MAX_CONN,
            .authmode = config && config->authmode ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
        },
    };

    if (config) {
        strncpy((char*)wifi_config.ap.ssid, (const char*)config->ssid, sizeof(wifi_config.ap.ssid));
        if (config->authmode) {
            strncpy((char*)wifi_config.ap.password, (const char*)config->password, sizeof(wifi_config.ap.password));
        }
    } else {
        strncpy((char*)wifi_config.ap.ssid, WIFI_AP_SSID, sizeof(wifi_config.ap.ssid));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    g_wifi_mgr->mode = WIFI_MODE_AP;
    g_wifi_mgr->state = WIFI_STATE_CONNECTED;

    // 设置默认IP (192.168.4.1)
    snprintf(g_wifi_mgr->ip_str, sizeof(g_wifi_mgr->ip_str), "192.168.4.1");

    xSemaphoreGive(g_wifi_mgr->mutex);

    ESP_LOGI(TAG, "AP模式启动成功，SSID: %s", wifi_config.ap.ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_stop_ap(void)
{
    if (g_wifi_mgr == NULL || g_wifi_mgr->mode != WIFI_MODE_AP) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止AP模式");

    xSemaphoreTake(g_wifi_mgr->mutex, portMAX_DELAY);
    esp_wifi_stop();
    g_wifi_mgr->mode = WIFI_MODE_NULL;
    g_wifi_mgr->state = WIFI_STATE_IDLE;
    xSemaphoreGive(g_wifi_mgr->mutex);

    return ESP_OK;
}

esp_err_t wifi_manager_start_sta(const wifi_sta_config_t *config)
{
    if (g_wifi_mgr == NULL) {
        ESP_LOGE(TAG, "WiFi管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "STA配置为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "启动STA模式，SSID: %s", config->ssid);

    xSemaphoreTake(g_wifi_mgr->mutex, portMAX_DELAY);

    // 创建STA网络接口
    if (g_sta_netif == NULL) {
        g_sta_netif = esp_netif_create_default_wifi_sta();
    }

    // 设置WiFi模式为STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 配置STA
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strncpy((char*)wifi_config.sta.ssid, (const char*)config->ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, (const char*)config->password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    g_wifi_mgr->mode = WIFI_MODE_STA;
    g_wifi_mgr->state = WIFI_STATE_CONNECTING;
    g_wifi_mgr->retry_num = 0;

    xSemaphoreGive(g_wifi_mgr->mutex);

    // 连接WiFi
    esp_wifi_connect();

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(g_wifi_mgr->wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi连接成功");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi连接失败");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi连接超时");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_manager_stop_sta(void)
{
    if (g_wifi_mgr == NULL || g_wifi_mgr->mode != WIFI_MODE_STA) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止STA模式");

    xSemaphoreTake(g_wifi_mgr->mutex, portMAX_DELAY);
    esp_wifi_stop();
    g_wifi_mgr->mode = WIFI_MODE_NULL;
    g_wifi_mgr->state = WIFI_STATE_IDLE;
    xSemaphoreGive(g_wifi_mgr->mutex);

    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
    if (g_wifi_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "断开WiFi连接");
    return esp_wifi_disconnect();
}

// ==================== WiFi扫描 ====================

esp_err_t wifi_manager_start_scan(void)
{
    if (g_wifi_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "开始扫描WiFi网络...");

    // 释放旧结果
    if (g_wifi_mgr->ap_records) {
        free(g_wifi_mgr->ap_records);
        g_wifi_mgr->ap_records = NULL;
        g_wifi_mgr->ap_count = 0;
    }

    return esp_wifi_scan_start(NULL, true);
}

int wifi_manager_get_scan_results(wifi_ap_record_t *ap_list, int max_aps)
{
    if (g_wifi_mgr == NULL) {
        return 0;
    }

    // 获取扫描结果数量
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0) {
        return 0;
    }

    // 分配缓冲区
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return 0;
    }

    // 获取扫描结果
    esp_err_t ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取扫描结果失败: %s", esp_err_to_name(ret));
        free(ap_records);
        return 0;
    }

    // 保存结果
    g_wifi_mgr->ap_records = ap_records;
    g_wifi_mgr->ap_count = ap_count;

    // 复制到用户缓冲区
    int copy_count = (ap_count < max_aps) ? ap_count : max_aps;
    if (ap_list && copy_count > 0) {
        memcpy(ap_list, ap_records, sizeof(wifi_ap_record_t) * copy_count);
    }

    ESP_LOGI(TAG, "找到 %d 个WiFi网络", ap_count);
    return copy_count;
}

// ==================== 状态查询 ====================

wifi_state_t wifi_manager_get_state(void)
{
    return g_wifi_mgr ? g_wifi_mgr->state : WIFI_STATE_IDLE;
}

wifi_mode_t wifi_manager_get_mode(void)
{
    return g_wifi_mgr ? g_wifi_mgr->mode : WIFI_MODE_NULL;
}

esp_err_t wifi_manager_get_ap_info(int8_t *rssi, char *ip_addr, size_t ip_buf_size)
{
    if (g_wifi_mgr == NULL || g_wifi_mgr->state != WIFI_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    if (rssi) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            *rssi = ap_info.rssi;
        }
    }

    if (ip_addr && ip_buf_size > 0) {
        strncpy(ip_addr, g_wifi_mgr->ip_str, ip_buf_size);
    }

    return ESP_OK;
}

uint8_t wifi_manager_get_sta_num(void)
{
    if (g_wifi_mgr == NULL || g_wifi_mgr->mode != WIFI_MODE_AP) {
        return 0;
    }

    wifi_sta_list_t sta_list;
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }

    return 0;
}

bool wifi_manager_is_connected(void)
{
    return g_wifi_mgr && g_wifi_mgr->state == WIFI_STATE_CONNECTED;
}

// ==================== 辅助功能 ====================

char* wifi_manager_get_ip(void)
{
    return g_wifi_mgr ? g_wifi_mgr->ip_str : NULL;
}

esp_err_t wifi_manager_get_mac(uint8_t *mac)
{
    if (g_wifi_mgr == NULL || mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_wifi_get_mac(WIFI_IF_STA, mac);
}
