/**
 * @file i2c_scanner_legacy.c
 * @brief I2C设备扫描程序 - 使用旧版I2C API（ESP-IDF v4.x风格）
 * 
 * 使用与立创例程相同的旧版I2C驱动API
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCAN";

// Moji小智AI官方I2C引脚配置
#define I2C_SCL_PIN        GPIO_NUM_4     // I2C时钟
#define I2C_SDA_PIN        GPIO_NUM_5     // I2C数据
#define I2C_NUM            I2C_NUM_0      // I2C端口
#define I2C_FREQ_HZ        100000         // 100kHz
#define ES8311_I2C_ADDR    0x18           // ES8311地址

// 扫描范围
#define I2C_ADDR_START     0x08
#define I2C_ADDR_END       0x77

// 旧版I2C初始化
static esp_err_t i2c_legacy_init(void)
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
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ 旧版I2C驱动初始化成功");
    return ESP_OK;
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C设备扫描程序 (旧版API)\n");
    printf("  使用立创相同的I2C驱动\n");
    printf("  SCL = GPIO%d\n", I2C_SCL_PIN);
    printf("  SDA = GPIO%d\n", I2C_SDA_PIN);
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "初始化I2C（旧版API）...");
    
    if (i2c_legacy_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C初始化失败!");
        return;
    }

    ESP_LOGI(TAG, "开始扫描I2C设备 (0x%02X - 0x%02X)...", I2C_ADDR_START, I2C_ADDR_END);

    printf("\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");

    int found_count = 0;
    uint8_t found_addrs[16];

    for (uint8_t addr = I2C_ADDR_START; addr <= I2C_ADDR_END; addr++) {
        // 尝试写入（旧版API）
        esp_err_t ret = i2c_master_write_to_device(
            I2C_NUM,
            addr,
            NULL,  // 不发送数据
            0,     // 长度为0，只发送地址
            pdMS_TO_TICKS(100)
        );

        if (ret == ESP_OK) {
            // 设备存在
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
                ESP_LOGI(TAG, "     旧版I2C API成功检测到ES8311!");
            } else if (found_addrs[i] == 0x1A || found_addrs[i] == 0x1B) {
                ESP_LOGI(TAG, "     ⭐ 可能是ES8311（备用地址）");
            }
        }
        ESP_LOGI(TAG, "\n🎉 成功! 使用旧版I2C API检测到设备!");
    } else {
        ESP_LOGW(TAG, "❌ 未找到任何I2C设备!");
        ESP_LOGW(TAG, "旧版I2C API也无法检测到设备");
        ESP_LOGW(TAG, "请检查:");
        ESP_LOGW(TAG, "   1. I2C连线是否正确");
        ESP_LOGW(TAG, "      ESP32-S3 GPIO4 → ES8311 SCL");
        ESP_LOGW(TAG, "      ESP32-S3 GPIO5 → ES8311 SDA");
        ESP_LOGW(TAG, "   2. ES8311供电是否正常 (需要5V)");
        ESP_LOGW(TAG, "   3. ES8311的GND是否已连接");
        ESP_LOGW(TAG, "   4. 是否需要外部上拉电阻");
    }

    ESP_LOGI(TAG, "\n扫描完成!");
    
    // 卸载I2C驱动
    i2c_driver_delete(I2C_NUM);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
