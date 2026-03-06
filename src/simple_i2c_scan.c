/**
 * @file simple_i2c_scan.c
 * @brief 简单I2C扫描
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("\n=== SIMPLE I2C SCANNER ===\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("I2C Pins: SDA=GPIO5, SCL=GPIO4\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_5,
        .scl_io_num = GPIO_NUM_4,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    printf("Initializing I2C bus...\n");
    fflush(stdout);

    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        printf("ERROR: I2C init failed: %d\n", ret);
        fflush(stdout);
        return;
    }

    printf("I2C bus initialized!\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("Scanning addresses 0x08-0x77...\n");
    fflush(stdout);

    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t dev_handle;
        ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

        if (ret == ESP_OK) {
            // Try simple write test
            uint8_t dummy = 0;
            ret = i2c_master_transmit(dev_handle, &dummy, 1, pdMS_TO_TICKS(50));

            if (ret == ESP_OK) {
                printf("Found device at 0x%02X\n", addr);
                fflush(stdout);
                found++;
            }
            i2c_master_bus_rm_device(dev_handle);
        }

        if (addr % 16 == 15) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    printf("\nScan complete. Found %d device(s)\n", found);
    fflush(stdout);

    if (found == 0) {
        printf("\n[WARNING] No I2C devices found!\n");
        printf("Please check:\n");
        printf("1. SDA (GPIO5) and SCL (GPIO4) wiring\n");
        printf("2. Module power (VDD=5V, GND)\n");
        printf("3. Module's 3.3V regulator output\n");
        fflush(stdout);
    }

    printf("\nEntering loop...\n");
    fflush(stdout);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("Still running...\n");
        fflush(stdout);
    }
}
