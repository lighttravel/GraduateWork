/**
 * @file hello_test.c
 * @brief 简单的串口测试程序
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"

static const char *TAG = "HELLO_TEST";

void app_main(void)
{
    // 等待USB稳定
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("\n\n");
    printf("========================================\n");
    printf("  Hello ESP32-S3!\n");
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "ESP_LOGI test message");
    ESP_LOGW(TAG, "ESP_LOGW warning message");
    ESP_LOGE(TAG, "ESP_LOGE error message");

    // 获取芯片信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("\nChip Model: %s\n",
           chip_info.model == CHIP_ESP32S3 ? "ESP32-S3" : "Unknown");
    printf("Chip Cores: %d\n", chip_info.cores);
    printf("Chip Revision: %d\n", chip_info.revision);
    printf("SDK Version: %s\n", esp_get_idf_version());
    printf("\n");

    printf("========================================\n");
    printf("  If you can see this, serial works!\n");
    printf("========================================\n");
    printf("\n");

    // 持续输出
    int count = 0;
    while (1) {
        printf("[%d] Heartbeat... ", count++);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("OK\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}
