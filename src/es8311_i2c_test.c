/**
 * @file es8311_i2c_test.c
 * @brief 简化I2C测试 - 纯ASCII输出
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C_TEST";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define ES8311_ADDR        0x18

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) return ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };

    return i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
}

static void test_i2c_scan(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C Device Scanner\n");
    printf("  SCL=GPIO4, SDA=GPIO5\n");
    printf("========================================\n");
    printf("Scanning for I2C devices...\n");

    // 扫描所有可能的I2C地址
    int found_count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // 临时创建设备
        i2c_master_dev_handle_t temp_dev = NULL;
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &temp_dev);

        if (ret == ESP_OK) {
            // 尝试写入
            uint8_t test_data[2] = {0x00, 0x00};
            ret = i2c_master_transmit(temp_dev, test_data, 2, pdMS_TO_TICKS(100));

            if (ret == ESP_OK) {
                printf("[FOUND] Device at address: 0x%02X (decimal: %d)\n", addr, addr);
                found_count++;

                if (addr == ES8311_ADDR) {
                    printf("[MATCH] This is ES8311 address!\n");
                }
            }

            // 移除设备
            i2c_master_bus_rm_device(temp_dev);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    printf("\n");
    printf("========================================\n");
    printf("Scan complete. Found %d devices\n", found_count);
    printf("========================================\n");

    if (found_count == 0) {
        printf("\n[ERROR] No I2C devices found!\n");
        printf("Please check:\n");
        printf("  1. SCL (GPIO4) and SDA (GPIO5) wiring\n");
        printf("  2. ES8311 power supply (3.3V or 5V)\n");
        printf("  3. I2C pull-up resistors\n");
        printf("  4. Ground connection\n");
    } else if (found_count > 0) {
        // 测试ES8311是否真的在工作
        printf("\nTesting ES8311 communication...\n");

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ES8311_ADDR,
            .scl_speed_hz = 100000,
        };

        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
        if (ret == ESP_OK) {
            // 尝试读取REG00
            uint8_t reg = 0x00;
            uint8_t value = 0xFF;
            ret = i2c_master_transmit_receive(i2c_dev, &reg, 1, &value, 1, pdMS_TO_TICKS(1000));

            if (ret == ESP_OK) {
                printf("[OK] ES8311 at 0x%02X is responding\n", ES8311_ADDR);
                printf("[OK] REG00 = 0x%02X\n", value);
                printf("\nThis means:");
                printf("  - I2C wiring is correct");
                printf("  - ES8311 is powered on");
                printf("  - ES8311 I2C interface is working");
            } else {
                printf("[FAIL] Cannot read from ES8311\n");
                printf("Error code: %d\n", ret);
            }

            i2c_master_bus_rm_device(i2c_dev);
        }
    }

    printf("\n");
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C Hardware Test for ES8311\n");
    printf("========================================\n");
    printf("\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C initialization failed\n");
        return;
    }

    printf("[OK] I2C bus initialized\n");
    printf("\n");

    test_i2c_scan();

    printf("\nTest complete. Restarting in 10 seconds...\n");

    // 每10秒重新扫描
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        printf("\n========================================\n");
        printf("Rescanning I2C bus...\n");
        printf("========================================\n");
        test_i2c_scan();
    }
}
