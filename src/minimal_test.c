/**
 * @file minimal_test.c
 * @brief Minimal test - just printf
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("Minimal Test Program\n");
    printf("========================================\n");
    printf("\n");
    printf("Program started!\n");
    printf("Free heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("\n");
    printf("========================================\n");
    printf("\n");

    while(1) {
        printf("Running... (print every 5 seconds)\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
