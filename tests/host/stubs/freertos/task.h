#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H

#include "FreeRTOS.h"

typedef void *TaskHandle_t;
#define pdPASS 1

static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
static inline void vTaskDelete(void *task) { (void)task; }
static inline int xTaskCreate(void (*task)(void *), const char *name, int stack, void *params, int priority, TaskHandle_t *handle) { (void)task; (void)name; (void)stack; (void)params; (void)priority; *handle = (TaskHandle_t)1; return pdPASS; }

#endif
