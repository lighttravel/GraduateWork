/**
 * @file step_test.c
 * @brief 逐步调试程序
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "STEP_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "Step 1: app_main entered");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Step 2: after first delay");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Step 3: about to enter loop");
    fflush(stdout);

    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Loop count: %d", count++);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
