/**
 * @file i2c_scanner_es8311.c
 * @brief 扫描所有可能的ES8311 I2C地址
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCAN";

void app_main(void)
{
    // Wait for USB CDC to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n");
    fflush(stdout);
    printf("========================================\n");
    fflush(stdout);
    printf("  ES8311 I2C Scanner\n");
    fflush(stdout);
    printf("========================================\n");
    fflush(stdout);
    printf("\n");
    fflush(stdout);

    printf("Scanning for ES8311 at all possible addresses...\n");
    fflush(stdout);
    printf("Common ES8311 addresses: 0x18, 0x1A, 0x26, 0x46\n");
    fflush(stdout);
    printf("\n");
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

    if (i2c_new_master_bus(&bus_config, &bus_handle) != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    // 扫描范围：0x08到0x77
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("0x00         ");

    uint8_t found_addrs[16];
    int found_count = 0;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t dev_handle;
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

        if (ret == ESP_OK) {
            // 尝试读取寄存器
            uint8_t reg = 0xFD;  // Chip ID寄存器
            uint8_t data;
            ret = i2c_master_transmit_receive(dev_handle, &reg, 1, &data, 1, pdMS_TO_TICKS(50));

            if (ret == ESP_OK) {
                printf("%02X ", addr);
                if (found_count < 16) {
                    found_addrs[found_count] = addr;
                    found_count++;
                }
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
    printf("Found %d I2C device(s)\n", found_count);

    if (found_count > 0) {
        printf("\nDetailed scan of found devices:\n");
        printf("========================================\n");

        for (int i = 0; i < found_count; i++) {
            uint8_t addr = found_addrs[i];
            printf("\nAddress: 0x%02X\n", addr);

            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = 100000,
            };

            i2c_master_dev_handle_t dev_handle;
            if (i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle) == ESP_OK) {
                // 读取ES8311的关键寄存器
                uint8_t reg_fd, reg_fe, reg_ff, reg00;

                uint8_t r = 0xFD;
                if (i2c_master_transmit_receive(dev_handle, &r, 1, &reg_fd, 1, pdMS_TO_TICKS(50)) == ESP_OK) {
                    printf("  REG_FD (Chip ID): 0x%02X %s\n", reg_fd,
                           (reg_fd == 0x83) ? "[ES8311 confirmed!]" :
                           (reg_fd == 0x00) ? "[possible ES8388/ES8374]" :
                           (reg_fd == 0xB0) ? "[unknown chip]" : "[other]");
                }

                r = 0xFE;
                if (i2c_master_transmit_receive(dev_handle, &r, 1, &reg_fe, 1, pdMS_TO_TICKS(50)) == ESP_OK) {
                    printf("  REG_FE (Version): 0x%02X\n", reg_fe);
                }

                r = 0xFF;
                if (i2c_master_transmit_receive(dev_handle, &r, 1, &reg_ff, 1, pdMS_TO_TICKS(50)) == ESP_OK) {
                    printf("  REG_FF:          0x%02X\n", reg_ff);
                }

                r = 0x00;
                if (i2c_master_transmit_receive(dev_handle, &r, 1, &reg00, 1, pdMS_TO_TICKS(50)) == ESP_OK) {
                    printf("  REG_00 (Control): 0x%02X\n", reg00);
                }

                i2c_master_bus_rm_device(dev_handle);
            }
        }
    } else {
        printf("\n[WARNING] No I2C devices found!\n");
        printf("\nPlease check:\n");
        printf("1. SDA (GPIO5) and SCL (GPIO4) wiring\n");
        printf("2. Module VDD (5V) and GND connections\n");
        printf("3. Module's 3.3V regulator output\n");
        printf("4. Try reseating the module in its socket\n");
    }

    printf("\n");
    printf("========================================\n");
    printf("Scan complete\n");
    printf("========================================\n\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
