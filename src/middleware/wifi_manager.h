/**
 * @file wifi_manager.h
 * @brief WiFi管理器 - 管理WiFi连接(AP/STA模式)
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief WiFi事件类型 (扩展的WiFi事件)
 */
typedef enum {
    WIFI_MGR_EVENT_SCAN_DONE = 0,      // 扫描完成
    WIFI_MGR_EVENT_STA_START,          // STA启动
    WIFI_MGR_EVENT_STA_STOP,           // STA停止
    WIFI_MGR_EVENT_STA_CONNECTED,      // STA已连接
    WIFI_MGR_EVENT_STA_DISCONNECTED,   // STA已断开
    WIFI_MGR_EVENT_AP_START,           // AP启动
    WIFI_MGR_EVENT_AP_STOP,            // AP停止
    WIFI_MGR_EVENT_AP_STA_CONNECTED,   // STA连接到AP
    WIFI_MGR_EVENT_AP_STA_DISCONNECTED // STA从AP断开
} wifi_mgr_event_t;

/**
 * @brief WiFi状态
 */
typedef enum {
    WIFI_STATE_IDLE = 0,       // 空闲
    WIFI_STATE_CONNECTING,     // 连接中
    WIFI_STATE_CONNECTED,      // 已连接
    WIFI_STATE_DISCONNECTED,   // 已断开
    WIFI_STATE_ERROR           // 错误
} wifi_state_t;

/**
 * @brief WiFi事件回调函数
 * @param event WiFi事件
 * @param user_data 用户数据
 */
typedef void (*wifi_event_callback_t)(wifi_mgr_event_t event, void *user_data);

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化WiFi管理器
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_init(wifi_event_callback_t event_cb, void *user_data);

/**
 * @brief 反初始化WiFi管理器
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_deinit(void);

// ==================== WiFi模式控制 ====================

/**
 * @brief 启动AP模式
 * @param config AP配置
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_start_ap(const wifi_ap_config_t *config);

/**
 * @brief 停止AP模式
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_stop_ap(void);

/**
 * @brief 启动STA模式
 * @param config STA配置
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_start_sta(const wifi_sta_config_t *config);

/**
 * @brief 停止STA模式
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_stop_sta(void);

/**
 * @brief 断开WiFi连接
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_disconnect(void);

// ==================== WiFi扫描 ====================

/**
 * @brief 开始扫描WiFi网络
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_start_scan(void);

/**
 * @brief 获取扫描结果
 * @param ap_list AP列表缓冲区
 * @param max_aps 最大AP数量
 * @return 实际找到的AP数量
 */
int wifi_manager_get_scan_results(wifi_ap_record_t *ap_list, int max_aps);

// ==================== 状态查询 ====================

/**
 * @brief 获取WiFi状态
 * @return WiFi状态
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief 获取WiFi模式
 * @return WiFi模式
 */
wifi_mode_t wifi_manager_get_mode(void);

/**
 * @brief 获取当前连接的AP信息
 * @param rssi RSSI(信号强度)
 * @param ip_addr IP地址缓冲区
 * @param ip_buf_size IP地址缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_get_ap_info(int8_t *rssi, char *ip_addr, size_t ip_buf_size);

/**
 * @brief 获取AP模式下连接的STA数量
 * @return STA数量
 */
uint8_t wifi_manager_get_sta_num(void);

/**
 * @brief 是否已连接
 * @return true已连接
 */
bool wifi_manager_is_connected(void);

// ==================== 辅助功能 ====================

/**
 * @brief 获取本地IP地址
 * @return IP地址字符串
 */
char* wifi_manager_get_ip(void);

/**
 * @brief 获取MAC地址
 * @param mac MAC地址缓冲区(至少6字节)
 * @return ESP_OK成功
 */
esp_err_t wifi_manager_get_mac(uint8_t *mac);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
