#ifndef BVSTK_I2C_SLAVE_NEUTRINO_H
#define BVSTK_I2C_SLAVE_NEUTRINO_H

#include <pthread.h>
#include <stdint.h>

#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "shared/base/bvstk_status.h"

typedef struct {
    pthread_t thread;
    pthread_mutex_t startup_mutex;
    pthread_cond_t startup_condition;
    pthread_mutex_t *state_mutex;
    bvstk_i2c_slave_hw_t *hardware;
    bvstk_i2c_slave_service_t *service;
    int interrupt_id;
    int startup_complete;
    bvstk_status_t startup_status;
    uint8_t initialized;
} bvstk_i2c_slave_neutrino_t;

bvstk_status_t bvstk_i2c_slave_neutrino_start(
    bvstk_i2c_slave_neutrino_t *adapter,
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_service_t *service,
    pthread_mutex_t *state_mutex);

/* The adapter is owned for the lifetime of bvstkd. */
void bvstk_i2c_slave_neutrino_stop(
    bvstk_i2c_slave_neutrino_t *adapter);

#endif /* BVSTK_I2C_SLAVE_NEUTRINO_H */
