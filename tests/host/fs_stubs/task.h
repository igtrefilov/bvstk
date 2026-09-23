#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
typedef void *TaskHandle_t;
typedef int BaseType_t;
typedef unsigned TickType_t;
#define tskIDLE_PRIORITY 0
#define pdPASS 1
void vTaskDelay(TickType_t ticks);
void vTaskDelete(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *task);
#endif
