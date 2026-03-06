/**
 * @file debug_i2c.c
 * @brief I2C调试程序
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "DEBUG_I2C";

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Longer delay to allow monitor to connect
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "I2C Debug Scanner");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "I2C Pins: SDA=GPIO5, SCL=GPIO4");

    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_5,
        .scl_io_num = GPIO_NUM_4,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Initializing I2C bus...");

    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "I2C bus initialized successfully!");
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Scanning common I2C addresses...");

    // 只扫描常见地址以节省时间
    uint8_t test_addrs[] = {0x18, 0x19, 0x1A, 0x1B, 0x26, 0x46, 0x4C, 0x50};
    int num_addrs = sizeof(test_addrs) / sizeof(test_addrs[0]);
    int found = 0;

    for (int i = 0; i < num_addrs; i++) {
        uint8_t addr = test_addrs[i];

        ESP_LOGD(TAG, "Testing address 0x%02X...", addr);

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t dev_handle;
        ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

        if (ret == ESP_OK) {
            // 尝试写入测试
            uint8_t reg = 0xFD;
            ret = i2c_master_transmit(dev_handle, &reg, 1, pdMS_TO_TICKS(100));

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, ">>> Found device at 0x%02X! <<<", addr);
                found++;
            } else {
                ESP_LOGD(TAG, "No ACK at 0x%02X", addr);
            }
            i2c_master_bus_rm_device(dev_handle);
        } else {
            ESP_LOGD(TAG, "Failed to add device at 0x%02X: %s", addr, esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Scan complete. Found %d device(s)", found);
    ESP_LOGI(TAG, "========================================");

    if (found == 0) {
        ESP_LOGW(TAG, "No I2C devices found!");
        ESP_LOGW(TAG, "Check wiring:");
        ESP_LOGW(TAG, "  - SDA=GPIO5, SCL=GPIO4");
        ESP_LOGW(TAG, "  - Module VDD=5V, GND connected");
        ESP_LOGW(TAG, "  - Module 3.3V regulator output");
    }

    ESP_LOGI(TAG, "Entering idle loop...");

    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "[%d] Still running...", count++);
    }
}
