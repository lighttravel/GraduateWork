#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H

#include "FreeRTOS.h"

static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }

#endif
