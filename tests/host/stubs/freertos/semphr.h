#ifndef FREERTOS_SEMPHR_H
#define FREERTOS_SEMPHR_H

typedef void *SemaphoreHandle_t;
#define portMAX_DELAY -1

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
static inline void vSemaphoreDelete(SemaphoreHandle_t sem) { (void)sem; }
static inline void xSemaphoreTake(SemaphoreHandle_t sem, int ticks) { (void)sem; (void)ticks; }
static inline void xSemaphoreGive(SemaphoreHandle_t sem) { (void)sem; }

#endif
