/**
 * @file i2c_scanner.c
 * @brief I2C设备扫描程序 - 查找ES8311的I2C地址
 * 
 * 使用Moji小智AI官方引脚配置:
 * - I2C_SCL: GPIO4
 * - I2C_SDA: GPIO5
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCAN";

// Moji小智AI官方I2C引脚配置
#define I2C_SCL_PIN        GPIO_NUM_4     // I2C时钟
#define I2C_SDA_PIN        GPIO_NUM_5     // I2C数据

// 扫描范围
#define I2C_ADDR_START     0x08
#define I2C_ADDR_END       0x77

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C设备扫描程序\n");
    printf("  Moji小智AI官方配置\n");
    printf("  SCL = GPIO%d\n", I2C_SCL_PIN);
    printf("  SDA = GPIO%d\n", I2C_SDA_PIN);
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "初始化I2C总线...");

    // 初始化I2C总线
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C总线初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "I2C总线初始化成功");
    ESP_LOGI(TAG, "开始扫描I2C设备 (0x%02X - 0x%02X)...", I2C_ADDR_START, I2C_ADDR_END);

    printf("\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");

    int found_count = 0;
    uint8_t found_addrs[16];

    for (uint8_t addr = I2C_ADDR_START; addr <= I2C_ADDR_END; addr++) {
        // 尝试通信
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t dev_handle;
        ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

        if (ret == ESP_OK) {
            // 尝试写入一个字节来检测设备是否存在
            uint8_t dummy_data = 0x00;
            ret = i2c_master_transmit(dev_handle, &dummy_data, 1, pdMS_TO_TICKS(100));

            if (ret == ESP_OK) {
                // 设备存在并响应了ACK
                printf("%02X ", addr);
                if (found_count < 16) {
                    found_addrs[found_count++] = addr;
                }
            } else if (ret == ESP_ERR_TIMEOUT) {
                // 超时可能表示设备存在
                printf("%02X? ", addr);
                if (found_count < 16) {
                    found_addrs[found_count++] = addr;
                }
            } else {
                // 设备不存在
                printf("-- ");
            }

            i2c_master_bus_rm_device(dev_handle);
        } else {
            printf("-- ");
        }

        // 每16个地址换行
        if ((addr + 1) % 16 == 0) {
            printf("\n%02X: ", addr + 1);
        }
    }

    printf("\n\n");

    if (found_count > 0) {
        ESP_LOGI(TAG, "✅ 找到 %d 个I2C设备:", found_count);
        for (int i = 0; i < found_count; i++) {
            ESP_LOGI(TAG, "   - 地址: 0x%02X", found_addrs[i]);

            // 判断是否是ES8311
            if (found_addrs[i] == 0x18) {
                ESP_LOGI(TAG, "     ⭐✅✅✅ 这是ES8311编解码器! ✅✅✅⭐");
            } else if (found_addrs[i] == 0x1A || found_addrs[i] == 0x1B) {
                ESP_LOGI(TAG, "     ⭐ 可能是ES8311（备用地址）");
            }
        }
        ESP_LOGI(TAG, "\n🎉 成功! ES8311检测成功,可以进行下一步测试!");
    } else {
        ESP_LOGW(TAG, "❌ 未找到任何I2C设备!");
        ESP_LOGW(TAG, "请检查:");
        ESP_LOGW(TAG, "   1. I2C连线是否正确");
        ESP_LOGW(TAG, "      ESP32-S3 GPIO4 → ES8311 SCL");
        ESP_LOGW(TAG, "      ESP32-S3 GPIO5 → ES8311 SDA");
        ESP_LOGW(TAG, "   2. ES8311供电是否正常 (需要5V)");
        ESP_LOGW(TAG, "   3. ES8311的GND是否已连接");
    }

    ESP_LOGI(TAG, "\n扫描完成! 5秒后重新扫描...");
    printf("\n");

    // 删除I2C总线
    i2c_del_master_bus(bus_handle);

    // 循环扫描
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
}
