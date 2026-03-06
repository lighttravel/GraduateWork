/**
 * @file i2c_driver.c
 * @brief I2C驱动实现(用于配置ES8311)
 */

#include "i2c_driver.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "I2C_DRV";
static bool g_i2c_initialized = false;

// ==================== I2C初始化 ====================

esp_err_t i2c_driver_init(void)
{
    if (g_i2c_initialized) {
        ESP_LOGW(TAG, "I2C已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化I2C主机, SCL=%d, SDA=%d", I2C_SCL_PIN, I2C_SDA_PIN);

    // I2C配置
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    // 安装I2C驱动
    esp_err_t ret = i2c_param_config(I2C_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_i2c_initialized = true;
    ESP_LOGI(TAG, "I2C初始化完成");
    return ESP_OK;
}

// ==================== I2C反初始化 ====================

esp_err_t i2c_driver_deinit(void)
{
    if (!g_i2c_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化I2C");

    esp_err_t ret = i2c_driver_delete(I2C_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动删除失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_i2c_initialized = false;
    ESP_LOGI(TAG, "I2C已卸载");
    return ESP_OK;
}

// ==================== I2C写入数据 ====================

esp_err_t i2c_driver_write_bytes(uint8_t device_addr, uint8_t reg_addr,
                                  const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);

    // 增加超时时间到2000ms，ES8311在初始化时可能需要较长时间
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, pdMS_TO_TICKS(2000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C写入失败 @0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t i2c_driver_write_byte(uint8_t device_addr, uint8_t reg_addr, uint8_t data)
{
    return i2c_driver_write_bytes(device_addr, reg_addr, &data, 1);
}

// ==================== I2C读取数据 ====================

esp_err_t i2c_driver_read_bytes(uint8_t device_addr, uint8_t reg_addr,
                                 uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    // 增加超时时间到2000ms
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, pdMS_TO_TICKS(2000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C读取失败 @0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t i2c_driver_read_byte(uint8_t device_addr, uint8_t reg_addr, uint8_t *data)
{
    return i2c_driver_read_bytes(device_addr, reg_addr, data, 1);
}
