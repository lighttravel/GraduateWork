/**
 * @file test_continuous.c
 * @brief 持续输出测试
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    // 立即开始输出
    printf("\n\n\n");

    while (1) {
        printf("Testing serial output...\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
