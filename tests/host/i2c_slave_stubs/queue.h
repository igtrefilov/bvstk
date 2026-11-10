#ifndef TEST_I2C_SLAVE_QUEUE_H
#define TEST_I2C_SLAVE_QUEUE_H
#include <stddef.h>
typedef void *QueueHandle_t;
#define pdFALSE 0
#define portMAX_DELAY ((unsigned)-1)
#define portYIELD_FROM_ISR(woken) ((void)(woken))
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
int xQueueSendFromISR(QueueHandle_t, const void *, int *);
int xQueueReceive(QueueHandle_t, void *, unsigned);
void vQueueDelete(QueueHandle_t);
void xPortInstallInterruptHandler(unsigned, void (*)(void *), void *);
void vPortEnableInterrupt(unsigned);
#endif
