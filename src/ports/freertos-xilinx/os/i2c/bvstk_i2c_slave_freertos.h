#ifndef BVSTK_I2C_SLAVE_FREERTOS_H
#define BVSTK_I2C_SLAVE_FREERTOS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "shared/base/bvstk_status.h"

typedef struct {
    bvstk_i2c_slave_hw_t *hardware;
    bvstk_i2c_slave_service_t *service;
    QueueHandle_t event_queue;
    TaskHandle_t task;
    uint8_t initialized;
} bvstk_i2c_slave_freertos_t;

bvstk_status_t bvstk_i2c_slave_freertos_start(
    bvstk_i2c_slave_freertos_t *adapter,
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_service_t *service);
void bvstk_i2c_slave_freertos_stop(bvstk_i2c_slave_freertos_t *adapter);

#endif /* BVSTK_I2C_SLAVE_FREERTOS_H */
