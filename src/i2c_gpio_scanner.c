/**
 * @file i2c_gpio_scanner.c
 * @brief I2C GPIO引脚扫描程序 - 测试不同I2C引脚组合
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "I2C_GPIO_SCAN";

// 可能的I2C引脚组合
typedef struct {
    gpio_num_t scl;
    gpio_num_t sda;
    const char *name;
} i2c_gpio_pair_t;

// 测试不同的I2C引脚组合
static const i2c_gpio_pair_t i2c_pairs[] = {
    {GPIO_NUM_1, GPIO_NUM_2, "GPIO1/GPIO2 (立创默认)"},
    {GPIO_NUM_8, GPIO_NUM_9, "GPIO8/GPIO9 (我们当前配置)"},
    {GPIO_NUM_4, GPIO_NUM_5, "GPIO4/GPIO5 (原ESP32配置)"},
    {GPIO_NUM_6, GPIO_NUM_7, "GPIO6/GPIO7"},
    {GPIO_NUM_10, GPIO_NUM_11, "GPIO10/GPIO11"},
};

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  I2C GPIO引脚扫描程序\n");
    printf("  测试不同I2C引脚组合\n");
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "开始扫描I2C引脚组合...");
    ESP_LOGI(TAG, "注意: 这个程序只是打印可能的引脚组合");
    ESP_LOGI(TAG, "      实际I2C扫描需要运行i2c_scanner程序");
    printf("\n");

    for (int i = 0; i < sizeof(i2c_pairs) / sizeof(i2c_gpio_pair_t); i++) {
        const i2c_gpio_pair_t *pair = &i2c_pairs[i];
        
        printf("[%d] %s\n", i+1, pair->name);
        printf("    SCL = GPIO%d\n", pair->scl);
        printf("    SDA = GPIO%d\n", pair->sda);
        printf("\n");
    }

    ESP_LOGI(TAG, "请确认您的路小班模块使用哪一组I2C引脚:");
    ESP_LOGI(TAG, "1. 查看路小班模块的丝印或文档");
    ESP_LOGI(TAG, "2. 查看模块的原理图");
    ESP_LOGI(TAG, "3. 用万用表测试连通性");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "确认后，修改i2c_scanner.c中的引脚定义:");
    ESP_LOGI(TAG, "  #define I2C_SCL_PIN  GPIO_NUM_?");
    ESP_LOGI(TAG, "  #define I2C_SDA_PIN  GPIO_NUM_?");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
