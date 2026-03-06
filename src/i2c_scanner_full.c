/**
 * @file i2c_scanner_full.c
 * @brief 扫描所有I2C地址
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C Scanner (All Addresses)\n");
    printf("========================================\n");
    printf("\n");

    i2c_master_bus_handle_t bus_handle;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_5,
        .scl_io_num = GPIO_NUM_4,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    if (i2c_new_master_bus(&bus_config, &bus_handle) != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    printf("Scanning I2C bus (SDA=GPIO5, SCL=GPIO4)...\n\n");

    int found_count = 0;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("0x00         ");

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t dev_handle;
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

        if (ret == ESP_OK) {
            // Try to read
            uint8_t data;
            ret = i2c_master_transmit_receive(dev_handle, NULL, 0, &data, 1, pdMS_TO_TICKS(100));

            if (ret == ESP_OK) {
                printf("%02X ", addr);
                found_count++;
            } else {
                printf("-- ");
            }

            i2c_master_bus_rm_device(dev_handle);
        } else {
            printf("-- ");
        }

        if ((addr + 1) % 16 == 0) {
            printf("\n0x%02X ", addr + 1);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    printf("\n\n");
    printf("Found %d device(s)\n", found_count);

    if (found_count == 0) {
        printf("\n[WARNING] No I2C devices found!\n");
        printf("Check:\n");
        printf("1. SDA=GPIO5, SCL=GPIO4 wiring\n");
        printf("2. Module power (VDD=5V, GND)\n");
        printf("3. Module 3.3V regulator output\n");
    } else {
        printf("\nCommon audio codec I2C addresses:\n");
        printf("0x18 - ES8311 (expected)\n");
        printf("0x1A - ES8311 alternate\n");
        printf("0x1B - ES8388\n");
        printf("0x26 - TAS5707\n");
        printf("0x4C - WM8960\n");
        printf("0x1A - AC101\n");
    }

    printf("\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
