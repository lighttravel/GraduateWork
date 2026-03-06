/**
 * @file gpio_test.c
 * @brief GPIO测试 - 最简单的输出
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("GPIO Test\n");
    printf("========================================\n");

    // 连续输出，不延迟
    for (int count = 0; count < 100; count++) {
        printf("Count: %d\n", count);
    }
}
