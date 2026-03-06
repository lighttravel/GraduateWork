/**
 * @file i2c_driver_v2.c
 * @brief I2C驱动实现 (新版 - 基于 i2c_master_bus)
 */

#include "i2c_driver_v2.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "I2C_V2";

// ==================== I2C 总线管理 ====================

esp_err_t i2c_driver_v2_init(const i2c_driver_v2_config_t *config,
                              i2c_master_bus_handle_t *p_bus_handle)
{
    if (config == NULL || p_bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "初始化 I2C 总线 (新版 API)");
    ESP_LOGI(TAG, "  端口: %d", config->port);
    ESP_LOGI(TAG, "  SDA: %d, SCL: %d", config->sda_pin, config->scl_pin);
    ESP_LOGI(TAG, "  频率: %lu Hz", config->clk_speed_hz);

    i2c_master_bus_config_t bus_config = {
        .i2c_port = config->port,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = config->enable_pullup ? 1 : 0,
        },
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, p_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2C 总线失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C 总线初始化成功");
    return ESP_OK;
}

esp_err_t i2c_driver_v2_deinit(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "反初始化 I2C 总线");

    esp_err_t ret = i2c_del_master_bus(bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "删除 I2C 总线失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ==================== 设备发现 ====================

esp_err_t i2c_driver_v2_scan(i2c_master_bus_handle_t bus_handle,
                              uint8_t *found_addr, int max_devices, int *num_found)
{
    if (bus_handle == NULL || found_addr == NULL || num_found == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "扫描 I2C 总线...");

    *num_found = 0;

    for (uint8_t addr = 0x08; addr < 0x78 && *num_found < max_devices; addr++) {
        if (i2c_driver_v2_probe(bus_handle, addr)) {
            ESP_LOGI(TAG, "  发现设备: 0x%02X", addr);
            found_addr[*num_found] = addr;
            (*num_found)++;
        }
    }

    ESP_LOGI(TAG, "扫描完成，发现 %d 个设备", *num_found);
    return ESP_OK;
}

bool i2c_driver_v2_probe(i2c_master_bus_handle_t bus_handle, uint8_t device_addr)
{
    if (bus_handle == NULL) {
        return false;
    }

    esp_err_t ret = i2c_master_probe(bus_handle, device_addr, pdMS_TO_TICKS(100));
    return (ret == ESP_OK);
}

// ==================== 设备句柄管理 ====================

esp_err_t i2c_driver_v2_device_add(i2c_master_bus_handle_t bus_handle,
                                    uint8_t device_addr,
                                    i2c_master_dev_handle_t *p_dev_handle)
{
    if (bus_handle == NULL || p_dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_addr,
        .scl_speed_hz = 100000,  // 使用默认速度
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, p_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加 I2C 设备 0x%02X 失败: %s", device_addr, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "添加 I2C 设备: 0x%02X", device_addr);
    return ESP_OK;
}

esp_err_t i2c_driver_v2_device_remove(i2c_master_dev_handle_t dev_handle)
{
    if (dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_rm_device(dev_handle);
    return ESP_OK;
}

// ==================== 数据传输 ====================

esp_err_t i2c_driver_v2_write_byte(i2c_master_dev_handle_t dev_handle,
                                    uint8_t reg_addr, uint8_t data)
{
    return i2c_driver_v2_write_bytes(dev_handle, reg_addr, &data, 1);
}

esp_err_t i2c_driver_v2_read_byte(i2c_master_dev_handle_t dev_handle,
                                   uint8_t reg_addr, uint8_t *data)
{
    return i2c_driver_v2_read_bytes(dev_handle, reg_addr, data, 1);
}

esp_err_t i2c_driver_v2_write_bytes(i2c_master_dev_handle_t dev_handle,
                                     uint8_t reg_addr, const uint8_t *data, size_t len)
{
    if (dev_handle == NULL || (len > 0 && data == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 创建发送缓冲区: [寄存器地址][数据...]
    uint8_t *write_buf = malloc(len + 1);
    if (write_buf == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    write_buf[0] = reg_addr;
    if (len > 0) {
        memcpy(&write_buf[1], data, len);
    }

    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, len + 1, pdMS_TO_TICKS(1000));

    free(write_buf);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C 写入失败 @0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t i2c_driver_v2_read_bytes(i2c_master_dev_handle_t dev_handle,
                                    uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (dev_handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 先写入寄存器地址，然后读取数据
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &reg_addr, 1,
                                                 data, len, pdMS_TO_TICKS(1000));

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C 读取失败 @0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }

    return ret;
}
