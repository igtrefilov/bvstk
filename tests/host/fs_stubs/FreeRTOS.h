#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef void *SemaphoreHandle_t;
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
void *pvPortMalloc(size_t size);
void vPortFree(void *pointer);
#endif
