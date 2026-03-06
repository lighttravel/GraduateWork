/**
 * @file nvs_storage.c
 * @brief NVS存储管理实现
 */

#include "nvs_storage.h"
#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "NVS_STG";

// ==================== NVS初始化 ====================

esp_err_t nvs_storage_init(void)
{
    ESP_LOGI(TAG, "初始化NVS存储管理");

    // NVS由main.c中的nvs_flash_init()初始化
    // 这里只做检查
    return ESP_OK;
}

// ==================== 字符串存储 ====================

esp_err_t nvs_storage_set_string(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存字符串失败 [%s]: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "提交NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "保存字符串: %s = %s", key, value);
    return ESP_OK;
}

esp_err_t nvs_storage_get_string(const char *key, char *value, size_t max_len)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    size_t len = max_len;
    err = nvs_get_str(handle, key, value, &len);
    nvs_close(handle);

    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "键不存在: %s", key);
        } else {
            ESP_LOGE(TAG, "读取字符串失败 [%s]: %s", key, esp_err_to_name(err));
        }
        return err;
    }

    ESP_LOGD(TAG, "读取字符串: %s = %s", key, value);
    return ESP_OK;
}

// ==================== 整数存储 ====================

esp_err_t nvs_storage_set_int(const char *key, int value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存整数失败 [%s]: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "提交NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "保存整数: %s = %d", key, value);
    return ESP_OK;
}

esp_err_t nvs_storage_get_int(const char *key, int *value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    int32_t i32_value;
    err = nvs_get_i32(handle, key, &i32_value);
    nvs_close(handle);

    if (err == ESP_OK) {
        *value = (int)i32_value;
    }

    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "键不存在: %s", key);
        } else {
            ESP_LOGE(TAG, "读取整数失败 [%s]: %s", key, esp_err_to_name(err));
        }
        return err;
    }

    ESP_LOGD(TAG, "读取整数: %s = %d", key, *value);
    return ESP_OK;
}

// ==================== U8整数存储 ====================

esp_err_t nvs_storage_set_u8(const char *key, uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存u8失败 [%s]: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "提交NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "保存u8: %s = %u", key, value);
    return ESP_OK;
}

esp_err_t nvs_storage_get_u8(const char *key, uint8_t *value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u8(handle, key, value);
    nvs_close(handle);

    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "键不存在: %s", key);
        } else {
            ESP_LOGE(TAG, "读取u8失败 [%s]: %s", key, esp_err_to_name(err));
        }
        return err;
    }

    ESP_LOGD(TAG, "读取u8: %s = %u", key, *value);
    return ESP_OK;
}

// ==================== 布尔值存储 ====================

esp_err_t nvs_storage_set_bool(const char *key, bool value)
{
    return nvs_storage_set_int(key, value ? 1 : 0);
}

esp_err_t nvs_storage_get_bool(const char *key, bool *value)
{
    int int_value;
    esp_err_t err = nvs_storage_get_int(key, &int_value);
    if (err == ESP_OK) {
        *value = (int_value != 0);
    }
    return err;
}

// ==================== 系统配置 ====================

esp_err_t nvs_storage_save_config(const system_config_t *config)
{
    ESP_LOGI(TAG, "保存系统配置");

    esp_err_t err;

    err = nvs_storage_set_string(NVS_KEY_WIFI_SSID, config->wifi_ssid);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_string(NVS_KEY_WIFI_PASS, config->wifi_password);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_string(NVS_KEY_DEEPGRAM_KEY, config->deepgram_key);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_string(NVS_KEY_DEEPSEEK_KEY, config->deepseek_key);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_string(NVS_KEY_INDEX_TTS_URL, config->index_tts_url);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_int(NVS_KEY_TTS_PROVIDER, config->tts_provider);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_bool(NVS_KEY_CONFIG_DONE, config->config_done);
    if (err != ESP_OK) return err;

    err = nvs_storage_set_string(NVS_KEY_DEVICE_NAME, config->device_name);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "系统配置保存完成");
    return ESP_OK;
}

esp_err_t nvs_storage_load_config(system_config_t *config)
{
    ESP_LOGI(TAG, "加载系统配置");

    memset(config, 0, sizeof(system_config_t));

    // 设置默认值
    strncpy(config->device_name, "小智", sizeof(config->device_name) - 1);
    config->tts_provider = 0;  // 0=Deepgram (TTS_PROVIDER_DEEPGRAM)

    // WiFi配置
    nvs_storage_get_string(NVS_KEY_WIFI_SSID, config->wifi_ssid, sizeof(config->wifi_ssid));
    nvs_storage_get_string(NVS_KEY_WIFI_PASS, config->wifi_password, sizeof(config->wifi_password));

    // API配置
    nvs_storage_get_string(NVS_KEY_DEEPGRAM_KEY, config->deepgram_key, sizeof(config->deepgram_key));
    nvs_storage_get_string(NVS_KEY_DEEPSEEK_KEY, config->deepseek_key, sizeof(config->deepseek_key));
    nvs_storage_get_string(NVS_KEY_INDEX_TTS_URL, config->index_tts_url, sizeof(config->index_tts_url));

    // TTS配置
    int provider;
    if (nvs_storage_get_int(NVS_KEY_TTS_PROVIDER, &provider) == ESP_OK) {
        config->tts_provider = provider;
    }

    // 配置完成标志
    nvs_storage_get_bool(NVS_KEY_CONFIG_DONE, &config->config_done);

    // 设备名称
    nvs_storage_get_string(NVS_KEY_DEVICE_NAME, config->device_name, sizeof(config->device_name));

    ESP_LOGI(TAG, "系统配置加载完成, 配置状态: %s", config->config_done ? "已完成" : "未完成");
    return ESP_OK;
}

// ==================== 配置状态检查 ====================

bool nvs_storage_is_configured(void)
{
    bool configured = false;
    nvs_storage_get_bool(NVS_KEY_CONFIG_DONE, &configured);
    return configured;
}

// ==================== 清除配置 ====================

esp_err_t nvs_storage_clear_all(void)
{
    ESP_LOGI(TAG, "清除所有配置");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "清除NVS失败: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "配置已清除");
    return err;
}

esp_err_t nvs_storage_erase(void)
{
    ESP_LOGI(TAG, "擦除NVS存储区");
    return nvs_flash_erase();
}
