/**
 * @file detailed_i2c_scan.c
 * @brief 详细的I2C总线扫描工具 - 慢速扫描所有地址
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_system.h"

static const char *TAG = "I2C_SCAN";

#define I2C_SCL_PIN           GPIO_NUM_4
#define I2C_SDA_PIN           GPIO_NUM_5
#define I2C_FREQ_HZ           100000
#define I2C_PORT              I2C_NUM_0

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "详细I2C总线扫描工具");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    ESP_LOGI(TAG, "I2C SCL: GPIO%d", I2C_SCL_PIN);
    ESP_LOGI(TAG, "I2C SDA: GPIO%d", I2C_SDA_PIN);
    ESP_LOGI(TAG, "I2C频率: %d Hz", I2C_FREQ_HZ);
    ESP_LOGI(TAG, "");

    // I2C配置
    ESP_LOGI(TAG, "正在配置I2C...");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return;
    }

    ret = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "I2C驱动初始化成功");
    ESP_LOGI(TAG, "");

    // 慢速扫描所有可能的I2C地址
    ESP_LOGI(TAG, "开始扫描I2C地址 0x03 - 0x77...");
    ESP_LOGI(TAG, "");

    int device_count = 0;

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        // 简单ping测试
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✓ 发现设备 @ 0x%02X", addr);
            device_count++;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 每个地址延迟10ms
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    if (device_count == 0) {
        ESP_LOGE(TAG, "未发现任何I2C设备");
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "可能原因:");
        ESP_LOGE(TAG, "1. 路小班模块未供电 - 请确认5V和GND连接");
        ESP_LOGE(TAG, "2. I2C接线错误 - SDA->GPIO5, SCL->GPIO4");
        ESP_LOGE(TAG, "3. 模块掉电 - 重新给模块上电");
        ESP_LOGE(TAG, "4. 上拉电阻问题 - 检查I2C上拉");
    } else {
        ESP_LOGI(TAG, "扫描完成: 发现 %d 个I2C设备", device_count);
    }
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    i2c_driver_delete(I2C_PORT);

    ESP_LOGI(TAG, "测试完成。30秒后重启...");
    vTaskDelay(pdMS_TO_TICKS(30000));
    esp_restart();
}
