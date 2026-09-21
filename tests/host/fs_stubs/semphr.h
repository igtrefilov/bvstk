#ifndef TEST_SEMPHR_H
#define TEST_SEMPHR_H
#include "FreeRTOS.h"
int xSemaphoreTake(SemaphoreHandle_t semaphore, unsigned ticks);
int xSemaphoreGive(SemaphoreHandle_t semaphore);
#endif
