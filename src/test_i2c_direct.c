/**
 * @file test_i2c_direct.c
 * @brief 直接I2C测试 - 简单输出
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"

void app_main(void)
{
    // 启动延迟，让监视器有足够时间连接
    vTaskDelay(pdMS_TO_TICKS(5000));

    printf("\n\n========================================\n");
    printf("I2C Test Start\n");
    printf("========================================\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("I2C Pins: SDA=GPIO5, SCL=GPIO4\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("Initializing I2C bus...\n");
    fflush(stdout);

    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_5,
        .scl_io_num = GPIO_NUM_4,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        printf("ERROR: I2C init failed (%d)\n", ret);
        fflush(stdout);
        return;
    }

    printf("I2C bus OK!\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Scanning common I2C addresses...\n");
    fflush(stdout);

    uint8_t test_addrs[] = {0x18, 0x19, 0x1A, 0x1B, 0x26, 0x46, 0x4C, 0x50};
    int num_addrs = sizeof(test_addrs) / sizeof(test_addrs[0]);
    int found = 0;

    for (int i = 0; i < num_addrs; i++) {
        uint8_t addr = test_addrs[i];
        printf("Testing 0x%02X... ", addr);
        fflush(stdout);

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t dev_handle;
        ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

        if (ret == ESP_OK) {
            uint8_t reg = 0xFD;  // Chip ID reg
            ret = i2c_master_transmit(dev_handle, &reg, 1, pdMS_TO_TICKS(100));

            if (ret == ESP_OK) {
                printf("FOUND!\n");
                found++;
            } else {
                printf("no response\n");
            }
            i2c_master_bus_rm_device(dev_handle);
        } else {
            printf("no device\n");
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    printf("\n========================================\n");
    printf("Scan complete. Found %d device(s)\n", found);
    printf("========================================\n");
    fflush(stdout);

    if (found == 0) {
        printf("\nWARNING: No I2C devices!\n");
        printf("Check:\n");
        printf("  1. SDA=GPIO5, SCL=GPIO4 wiring\n");
        printf("  2. Module VDD=5V, GND\n");
        printf("  3. Module 3.3V regulator\n");
        fflush(stdout);
    }

    printf("\nEntering loop...\n");
    fflush(stdout);

    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("[%d] Alive\n", count++);
        fflush(stdout);
    }
}
