/**
 * @file config_manager.c
 * @brief 配置管理器实现
 */

#include "config_manager.h"
#include "nvs_storage.h"
#include "config.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "CONFIG_MGR";

// 当前配置
static config_t g_config = {
    .wifi = {.ssid = "", .password = ""},
    .api = {.deepgram_key = "", .deepseek_key = "", .tts_url = "", .tts_provider = 0},
    .system = {.device_name = "小智", .volume = 80, .vad_enabled = false, .vad_threshold = 500},
    .config_done = false
};

// ==================== 初始化和配置 ====================

esp_err_t config_manager_init(void)
{
    ESP_LOGI(TAG, "初始化配置管理器");

    // 默认值已设置
    return ESP_OK;
}

esp_err_t config_manager_deinit(void)
{
    ESP_LOGI(TAG, "反初始化配置管理器");
    return ESP_OK;
}

esp_err_t config_manager_load(void)
{
    ESP_LOGI(TAG, "从NVS加载配置");

    esp_err_t ret;

    // 加载WiFi配置
    ret = nvs_storage_get_string(NVS_KEY_WIFI_SSID, g_config.wifi.ssid, sizeof(g_config.wifi.ssid));
    if (ret == ESP_OK) {
        nvs_storage_get_string(NVS_KEY_WIFI_PASS, g_config.wifi.password, sizeof(g_config.wifi.password));
        ESP_LOGI(TAG, "WiFi配置: SSID=%s", g_config.wifi.ssid);
    }

    // 加载API配置
    nvs_storage_get_string(NVS_KEY_DEEPGRAM_KEY, g_config.api.deepgram_key, sizeof(g_config.api.deepgram_key));
    nvs_storage_get_string(NVS_KEY_DEEPSEEK_KEY, g_config.api.deepseek_key, sizeof(g_config.api.deepseek_key));
    nvs_storage_get_string(NVS_KEY_INDEX_TTS_URL, g_config.api.tts_url, sizeof(g_config.api.tts_url));

    // 加载小智服务器URL
    nvs_storage_get_string(NVS_KEY_SERVER_URL, g_config.api.tts_url, sizeof(g_config.api.tts_url));

    // 加载TTS提供商
    uint8_t tts_provider = 0;
    if (nvs_storage_get_u8(NVS_KEY_TTS_PROVIDER, &tts_provider) == ESP_OK) {
        g_config.api.tts_provider = tts_provider;
    }

    // 加载系统配置
    nvs_storage_get_string(NVS_KEY_DEVICE_NAME, g_config.system.device_name, sizeof(g_config.system.device_name));

    uint8_t volume = 0;
    if (nvs_storage_get_u8("volume", &volume) == ESP_OK) {
        g_config.system.volume = volume;
    }

    // 检查配置完成标志
    uint8_t config_done = 0;
    if (nvs_storage_get_u8(NVS_KEY_CONFIG_DONE, &config_done) == ESP_OK) {
        g_config.config_done = config_done;
    }

    ESP_LOGI(TAG, "配置加载完成，配置完成: %s", g_config.config_done ? "是" : "否");
    return ESP_OK;
}

esp_err_t config_manager_save(void)
{
    ESP_LOGI(TAG, "保存配置到NVS");

    // 保存WiFi配置
    if (strlen(g_config.wifi.ssid) > 0) {
        nvs_storage_set_string(NVS_KEY_WIFI_SSID, g_config.wifi.ssid);
        nvs_storage_set_string(NVS_KEY_WIFI_PASS, g_config.wifi.password);
    }

    // 保存API配置
    if (strlen(g_config.api.deepgram_key) > 0) {
        nvs_storage_set_string(NVS_KEY_DEEPGRAM_KEY, g_config.api.deepgram_key);
    }
    if (strlen(g_config.api.deepseek_key) > 0) {
        nvs_storage_set_string(NVS_KEY_DEEPSEEK_KEY, g_config.api.deepseek_key);
    }
    if (strlen(g_config.api.tts_url) > 0) {
        nvs_storage_set_string(NVS_KEY_INDEX_TTS_URL, g_config.api.tts_url);
    }

    // 保存小智服务器URL
    if (strlen(g_config.api.tts_url) > 0) {
        nvs_storage_set_string(NVS_KEY_SERVER_URL, g_config.api.tts_url);
    }
    nvs_storage_set_u8(NVS_KEY_TTS_PROVIDER, g_config.api.tts_provider);

    // 保存系统配置
    if (strlen(g_config.system.device_name) > 0) {
        nvs_storage_set_string(NVS_KEY_DEVICE_NAME, g_config.system.device_name);
    }
    nvs_storage_set_u8("volume", g_config.system.volume);
    nvs_storage_set_u8(NVS_KEY_CONFIG_DONE, g_config.config_done);

    ESP_LOGI(TAG, "配置保存完成");
    return ESP_OK;
}

esp_err_t config_manager_reset(void)
{
    ESP_LOGI(TAG, "重置配置为默认值");

    memset(&g_config, 0, sizeof(config_t));

    // 恢复默认值
    strncpy(g_config.system.device_name, "小智", sizeof(g_config.system.device_name));
    g_config.system.volume = 80;
    g_config.system.vad_enabled = false;
    g_config.system.vad_threshold = 500;
    g_config.config_done = false;

    return ESP_OK;
}

// ==================== WiFi配置 ====================

esp_err_t config_manager_set_wifi(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.wifi.ssid, ssid, sizeof(g_config.wifi.ssid) - 1);
    strncpy(g_config.wifi.password, password, sizeof(g_config.wifi.password) - 1);

    ESP_LOGI(TAG, "WiFi配置已更新: SSID=%s", g_config.wifi.ssid);
    return ESP_OK;
}

esp_err_t config_manager_get_wifi(config_wifi_t *wifi)
{
    if (wifi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(wifi, &g_config.wifi, sizeof(config_wifi_t));
    return ESP_OK;
}

// ==================== API配置 ====================

esp_err_t config_manager_set_deepgram_key(const char *key)
{
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.api.deepgram_key, key, sizeof(g_config.api.deepgram_key) - 1);
    ESP_LOGI(TAG, "Deepgram API密钥已设置");
    return ESP_OK;
}

esp_err_t config_manager_get_deepgram_key(char *key, size_t key_len)
{
    if (key == NULL || key_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(key, g_config.api.deepgram_key, key_len - 1);
    key[key_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t config_manager_set_deepseek_key(const char *key)
{
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.api.deepseek_key, key, sizeof(g_config.api.deepseek_key) - 1);
    ESP_LOGI(TAG, "DeepSeek API密钥已设置");
    return ESP_OK;
}

esp_err_t config_manager_get_deepseek_key(char *key, size_t key_len)
{
    if (key == NULL || key_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(key, g_config.api.deepseek_key, key_len - 1);
    key[key_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t config_manager_set_tts(const char *url, uint8_t provider)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.api.tts_url, url, sizeof(g_config.api.tts_url) - 1);
    g_config.api.tts_provider = provider;

    ESP_LOGI(TAG, "TTS配置已更新: provider=%d", provider);
    return ESP_OK;
}

esp_err_t config_manager_get_tts(char *url, size_t url_len, uint8_t *provider)
{
    if (url == NULL || url_len == 0 || provider == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(url, g_config.api.tts_url, url_len - 1);
    url[url_len - 1] = '\0';
    *provider = g_config.api.tts_provider;

    return ESP_OK;
}

// ==================== 小智服务器配置 ====================

esp_err_t config_manager_set_server_url(const char *url)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.api.tts_url, url, sizeof(g_config.api.tts_url) - 1);
    ESP_LOGI(TAG, "小智服务器URL已设置: %s", url);

    return ESP_OK;
}

esp_err_t config_manager_get_server_url(char *url, size_t url_len)
{
    if (url == NULL || url_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 检查是否有配置的URL
    if (strlen(g_config.api.tts_url) == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    strncpy(url, g_config.api.tts_url, url_len - 1);
    url[url_len - 1] = '\0';

    return ESP_OK;
}

// ==================== 系统配置 ====================

esp_err_t config_manager_set_device_name(const char *name)
{
    if (name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.system.device_name, name, sizeof(g_config.system.device_name) - 1);
    ESP_LOGI(TAG, "设备名称设置为: %s", g_config.system.device_name);
    return ESP_OK;
}

esp_err_t config_manager_get_device_name(char *name, size_t name_len)
{
    if (name == NULL || name_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(name, g_config.system.device_name, name_len - 1);
    name[name_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t config_manager_set_volume(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }

    g_config.system.volume = volume;
    ESP_LOGI(TAG, "音量设置为: %d", volume);
    return ESP_OK;
}

uint8_t config_manager_get_volume(void)
{
    return g_config.system.volume;
}

// ==================== 配置验证 ====================

bool config_manager_is_valid(void)
{
    // 检查必需的配置项
    if (strlen(g_config.wifi.ssid) == 0) {
        ESP_LOGW(TAG, "WiFi未配置");
        return false;
    }

    if (strlen(g_config.api.deepgram_key) == 0) {
        ESP_LOGW(TAG, "Deepgram API密钥未配置");
        return false;
    }

    if (strlen(g_config.api.deepseek_key) == 0) {
        ESP_LOGW(TAG, "DeepSeek API密钥未配置");
        return false;
    }

    return true;
}

bool config_manager_is_configured(void)
{
    return g_config.config_done;
}

esp_err_t config_manager_set_config_done(bool done)
{
    g_config.config_done = done;
    return nvs_storage_set_u8(NVS_KEY_CONFIG_DONE, done ? 1 : 0);
}

// ==================== 调试 ====================

void config_manager_print(void)
{
    ESP_LOGI(TAG, "========== 配置信息 ==========");
    ESP_LOGI(TAG, "WiFi SSID: %s", g_config.wifi.ssid);
    ESP_LOGI(TAG, "设备名称: %s", g_config.system.device_name);
    ESP_LOGI(TAG, "音量: %d", g_config.system.volume);
    ESP_LOGI(TAG, "VAD: %s (阈值: %d)",
             g_config.system.vad_enabled ? "启用" : "禁用",
             g_config.system.vad_threshold);
    ESP_LOGI(TAG, "TTS提供商: %d", g_config.api.tts_provider);
    ESP_LOGI(TAG, "配置完成: %s", g_config.config_done ? "是" : "否");
    ESP_LOGI(TAG, "================================");
}
