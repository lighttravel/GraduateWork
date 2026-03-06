/**
 * @file simple_i2c_test.c
 * @brief 超简单I2C扫描工具 - 专门用于测试ES8311连接
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_system.h"

static const char *TAG = "I2C_TEST";

#define I2C_SCL_PIN           GPIO_NUM_4
#define I2C_SDA_PIN           GPIO_NUM_5
#define I2C_FREQ_HZ           100000
#define I2C_PORT              I2C_NUM_0
#define I2C_TIMEOUT_MS        1000

// ESP8311 I2C地址（7位）
#define ES8311_I2C_ADDR_7BIT  0x18

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "超简单I2C测试工具");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "I2C配置:");
    ESP_LOGI(TAG, "  SDA: GPIO%d", I2C_SDA_PIN);
    ESP_LOGI(TAG, "  SCL: GPIO%d", I2C_SCL_PIN);
    ESP_LOGI(TAG, "  频率: %d Hz", I2C_FREQ_HZ);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    // I2C配置
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    // 安装I2C驱动
    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "I2C参数配置成功");

    ret = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "I2C驱动安装成功");
    ESP_LOGI(TAG, "");

    // 测试ESP8311 (0x18)
    ESP_LOGI(TAG, "测试ES8311 @ 0x%02X...", ES8311_I2C_ADDR_7BIT);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_I2C_ADDR_7BIT << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0xFD, true);  // 芯片ID寄存器
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_I2C_ADDR_7BIT << 1) | I2C_MASTER_READ, true);
    uint8_t chip_id;
    i2c_master_read_byte(cmd, &chip_id, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "✓ 成功检测到I2C设备!");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  地址: 0x%02X", ES8311_I2C_ADDR_7BIT);
        ESP_LOGI(TAG, "  芯片ID寄存器(0xFD): 0x%02X", chip_id);
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "ES8311硬件连接成功！");
        ESP_LOGI(TAG, "如果chip_id是0x00或其他值，说明ES8311工作正常。");
        ESP_LOGI(TAG, "========================================");
    } else {
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "========================================");
        ESP_LOGE(TAG, "✗ 未检测到I2C设备");
        ESP_LOGE(TAG, "========================================");
        ESP_LOGE(TAG, "  I2C地址: 0x%02X (ES8311)", ES8311_I2C_ADDR_7BIT);
        ESP_LOGE(TAG, "  错误: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "可能原因:");
        ESP_LOGE(TAG, "1. 供电问题 - 路小班模块5V是否连接到外部5V电源？");
        ESP_LOGE(TAG, "2. 接线错误 - SDA→GPIO5, SCL→GPIO4是否正确？");
        ESP_LOGE(TAG, "3. 缺少上拉电阻 - 路小班模块是否有板载4.7kΩ上拉？");
        ESP_LOGE(TAG, "4. I2C地址不对 - 某些ES8311模块可能使用其他地址");
        ESP_LOGE(TAG, "5. GPIO12/13未连接 - 尝试连接SD_MODE(GPIO12)和PA_EN(GPIO13)");
        ESP_LOGE(TAG, "========================================");
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "测试完成。10秒后重启...");
    ESP_LOGI(TAG, "");

    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
}
