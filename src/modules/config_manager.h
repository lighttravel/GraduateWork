/**
 * @file config_manager.h
 * @brief 配置管理器 - 统一管理系统配置
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 配置结构体 ====================

/**
 * @brief WiFi配置
 */
typedef struct {
    char ssid[MAX_SSID_LEN];          // WiFi SSID
    char password[MAX_PASSWORD_LEN];  // WiFi密码
} config_wifi_t;

/**
 * @brief API配置
 */
typedef struct {
    char deepgram_key[MAX_API_KEY_LEN];   // Deepgram API密钥
    char deepseek_key[MAX_API_KEY_LEN];   // DeepSeek API密钥
    char tts_url[MAX_URL_LEN];            // TTS服务URL
    uint8_t tts_provider;                 // TTS提供商 (0=Deepgram, 1=Index-TTS)
} config_api_t;

/**
 * @brief 系统配置
 */
typedef struct {
    char device_name[MAX_DEVICE_NAME_LEN]; // 设备名称
    uint8_t volume;                        // 音量 (0-100)
    bool vad_enabled;                      // VAD启用
    uint16_t vad_threshold;                // VAD阈值
} config_system_t;

/**
 * @brief 完整配置
 */
typedef struct {
    config_wifi_t wifi;              // WiFi配置
    config_api_t api;                // API配置
    config_system_t system;          // 系统配置
    bool config_done;                // 配置完成标志
} config_t;

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化配置管理器
 * @return ESP_OK成功
 */
esp_err_t config_manager_init(void);

/**
 * @brief 反初始化配置管理器
 * @return ESP_OK成功
 */
esp_err_t config_manager_deinit(void);

/**
 * @brief 加载配置(从NVS)
 * @return ESP_OK成功
 */
esp_err_t config_manager_load(void);

/**
 * @brief 保存配置(到NVS)
 * @return ESP_OK成功
 */
esp_err_t config_manager_save(void);

/**
 * @brief 重置配置为默认值
 * @return ESP_OK成功
 */
esp_err_t config_manager_reset(void);

// ==================== WiFi配置 ====================

/**
 * @brief 设置WiFi配置
 * @param ssid SSID
 * @param password 密码
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_wifi(const char *ssid, const char *password);

/**
 * @brief 获取WiFi配置
 * @param wifi WiFi配置结构体
 * @return ESP_OK成功
 */
esp_err_t config_manager_get_wifi(config_wifi_t *wifi);

// ==================== API配置 ====================

/**
 * @brief 设置DeepGram API密钥
 * @param key API密钥
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_deepgram_key(const char *key);

/**
 * @brief 获取Deepgram API密钥
 * @param key 密钥缓冲区
 * @param key_len 缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t config_manager_get_deepgram_key(char *key, size_t key_len);

/**
 * @brief 设置DeepSeek API密钥
 * @param key API密钥
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_deepseek_key(const char *key);

/**
 * @brief 获取DeepSeek API密钥
 * @param key 密钥缓冲区
 * @param key_len 缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t config_manager_get_deepseek_key(char *key, size_t key_len);

/**
 * @brief 设置TTS配置
 * @param url TTS服务URL
 * @param provider TTS提供商
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_tts(const char *url, uint8_t provider);

/**
 * @brief 获取TTS配置
 * @param url URL缓冲区
 * @param url_len 缓冲区大小
 * @param provider TTS提供商
 * @return ESP_OK成功
 */
esp_err_t config_manager_get_tts(char *url, size_t url_len, uint8_t *provider);

// ==================== 小智服务器配置 ====================

/**
 * @brief 设置小智服务器URL
 * @param url 服务器URL
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_server_url(const char *url);

/**
 * @brief 获取小智服务器URL
 * @param url URL缓冲区
 * @param url_len 缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t config_manager_get_server_url(char *url, size_t url_len);

// ==================== 系统配置 ====================

/**
 * @brief 设置设备名称
 * @param name 设备名称
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_device_name(const char *name);

/**
 * @brief 获取设备名称
 * @param name 名称缓冲区
 * @param name_len 缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t config_manager_get_device_name(char *name, size_t name_len);

/**
 * @brief 设置音量
 * @param volume 音量(0-100)
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_volume(uint8_t volume);

/**
 * @brief 获取音量
 * @return 音量值(0-100)
 */
uint8_t config_manager_get_volume(void);

// ==================== 配置验证 ====================

/**
 * @brief 验证配置是否完整
 * @return true配置完整
 */
bool config_manager_is_valid(void);

/**
 * @brief 检查是否已配置
 * @return true已配置
 */
bool config_manager_is_configured(void);

/**
 * @brief 标记配置完成
 * @param done 是否完成
 * @return ESP_OK成功
 */
esp_err_t config_manager_set_config_done(bool done);

// ==================== 调试 ====================

/**
 * @brief 打印当前配置
 */
void config_manager_print(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
