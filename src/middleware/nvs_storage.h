/**
 * @file nvs_storage.h
 * @brief NVS存储管理头文件
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统配置结构
 */
typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char deepgram_key[128];
    char deepseek_key[128];
    char index_tts_url[256];
    int tts_provider;          // 0=Deepgram, 1=IndexTTS
    bool config_done;
    char device_name[32];
} system_config_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化NVS存储
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_init(void);

/**
 * @brief 保存字符串到NVS
 * @param key 键名
 * @param value 字符串值
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_set_string(const char *key, const char *value);

/**
 * @brief 从NVS读取字符串
 * @param key 键名
 * @param value 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_get_string(const char *key, char *value, size_t max_len);

/**
 * @brief 保存整数到NVS
 * @param key 键名
 * @param value 整数值
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_set_int(const char *key, int value);

/**
 * @brief 从NVS读取整数
 * @param key 键名
 * @param value 输出指针
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_get_int(const char *key, int *value);

/**
 * @brief 保存8位无符号整数到NVS
 * @param key 键名
 * @param value 8位无符号整数值
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_set_u8(const char *key, uint8_t value);

/**
 * @brief 从NVS读取8位无符号整数
 * @param key 键名
 * @param value 输出指针
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_get_u8(const char *key, uint8_t *value);

/**
 * @brief 保存布尔值到NVS
 * @param key 键名
 * @param value 布尔值
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_set_bool(const char *key, bool value);

/**
 * @brief 从NVS读取布尔值
 * @param key 键名
 * @param value 输出指针
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_get_bool(const char *key, bool *value);

/**
 * @brief 保存系统配置
 * @param config 配置结构
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_save_config(const system_config_t *config);

/**
 * @brief 加载系统配置
 * @param config 配置结构
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_load_config(system_config_t *config);

/**
 * @brief 检查是否已完成配置
 * @return true已配置，false未配置
 */
bool nvs_storage_is_configured(void);

/**
 * @brief 清除所有配置
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_clear_all(void);

/**
 * @brief 擦除NVS存储区
 * @return ESP_OK成功
 */
esp_err_t nvs_storage_erase(void);

#ifdef __cplusplus
}
#endif

#endif // NVS_STORAGE_H
