/**
 * @file test_i2c_scan.c
 * @brief I2C bus scanner for ES8311 detection on ESP32-S3
 */

#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define TAG "I2C_SCAN"

// Selected pin set per current hardware wiring
#define I2C_NUM         I2C_NUM_0
#define I2C_SCL_PIN     GPIO_NUM_4
#define I2C_SDA_PIN     GPIO_NUM_5
#define I2C_FREQ_HZ     50000
#define I2C_TIMEOUT_MS  1000

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C init done: SCL=%d SDA=%d Freq=%dHz", I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_HZ);
    return ESP_OK;
}

static int i2c_scan_bus(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Scanning I2C bus: 0x03 - 0x77");
    ESP_LOGI(TAG, "========================================");

    int found_devices = 0;

    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
            found_devices++;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (found_devices == 0) {
        ESP_LOGE(TAG, "No I2C devices found");
        ESP_LOGE(TAG, "Check wiring (SCL/SDA), module power, pull-ups");
    } else {
        ESP_LOGI(TAG, "Total I2C devices found: %d", found_devices);
    }

    ESP_LOGI(TAG, "========================================");
    return found_devices;
}

static bool test_es8311_at_addr(uint8_t addr)
{
    uint8_t reg_addr = 0x00;
    uint8_t data = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ES8311 ACK at 0x%02X, reg00=0x%02X", addr, data);
        return true;
    }

    ESP_LOGW(TAG, "ES8311 no ACK at 0x%02X (%s)", addr, esp_err_to_name(ret));
    return false;
}

static bool try_different_frequencies(void)
{
    uint32_t frequencies[] = {10000, 50000, 100000, 400000};
    bool found = false;

    ESP_LOGI(TAG, "Trying fallback I2C frequencies...");

    for (int i = 0; i < (int)(sizeof(frequencies) / sizeof(frequencies[0])); i++) {
        uint32_t freq = frequencies[i];
        ESP_LOGI(TAG, "--- Frequency: %lu Hz ---", (unsigned long)freq);

        i2c_driver_delete(I2C_NUM);

        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_SDA_PIN,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_SCL_PIN,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = freq,
        };

        if (i2c_param_config(I2C_NUM, &conf) != ESP_OK) {
            ESP_LOGW(TAG, "i2c_param_config failed at %lu Hz", (unsigned long)freq);
            continue;
        }

        if (i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0) != ESP_OK) {
            ESP_LOGW(TAG, "i2c_driver_install failed at %lu Hz", (unsigned long)freq);
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        if (test_es8311_at_addr(0x18)) {
            found = true;
        }
    }

    i2c_driver_delete(I2C_NUM);
    i2c_master_init();
    return found;
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C Scanner for ES8311 Detection\n");
    printf("========================================\n");

    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed, abort");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    int found_devices = i2c_scan_bus();
    bool es8311_ok = test_es8311_at_addr(0x18);

    vTaskDelay(pdMS_TO_TICKS(1000));
    bool es8311_fallback_ok = try_different_frequencies();

    ESP_LOGI(TAG, "Final scan...");
    found_devices = i2c_scan_bus();

    bool pass = (found_devices > 0) && (es8311_ok || es8311_fallback_ok);

    if (pass) {
        ESP_LOGI(TAG, "=== I2C_SCAN_RESULT: PASS (ES8311 detected) ===");
    } else {
        ESP_LOGE(TAG, "=== I2C_SCAN_RESULT: FAIL (ES8311 not detected) ===");
    }

    ESP_LOGI(TAG, "Scan complete");
}
