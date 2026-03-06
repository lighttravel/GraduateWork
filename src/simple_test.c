/**
 * @file simple_test.c
 * @brief Simple test to verify program execution
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "简单测试程序");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    ESP_LOGI(TAG, "程序已启动！");
    ESP_LOGI(TAG, "可用内存: %d bytes", esp_get_free_heap_size());

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "测试完成。");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    while(1) {
        ESP_LOGI(TAG, "运行中... (每5秒打印一次)");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
