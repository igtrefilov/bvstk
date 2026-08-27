#ifndef BVSTK_NEUTRINO_I2C_RUNTIME_H
#define BVSTK_NEUTRINO_I2C_RUNTIME_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "apps/neutrino/config/bvstk_i2c_config_store.h"
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "ports/neutrino-zynq7000/os/bvstk_sync_neutrino.h"
#include "ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/i2c/bvstk_i2c_slave_service.h"

typedef struct {
    bvstk_neutrino_i2c_config_store_t config_store;
    bvstk_i2c_master_hw_t master_hardware;
    bvstk_i2c_slave_hw_t slave_hardware;
    bvstk_i2c_devices_t devices;
    bvstk_i2c_cache_t cache;
    bvstk_i2c_policy_t policy;
    bvstk_i2c_master_service_t master_service;
    bvstk_i2c_slave_service_t slave_service;
    bvstk_i2c_slave_neutrino_t slave_adapter;
    bvstk_neutrino_mutex_t hardware_mutex;
    pthread_mutex_t state_mutex;
    uint8_t state_mutex_initialized;
    uint8_t hardware_mutex_initialized;
    uint8_t master_hardware_initialized;
    uint8_t slave_hardware_initialized;
    uint8_t master_service_initialized;
    uint8_t slave_service_initialized;
    uint8_t ready;
} bvstk_neutrino_i2c_runtime_t;

int bvstk_neutrino_i2c_runtime_init(
    bvstk_neutrino_i2c_runtime_t *runtime);
void bvstk_neutrino_i2c_runtime_shutdown(
    bvstk_neutrino_i2c_runtime_t *runtime);
int bvstk_neutrino_i2c_runtime_ready(
    const bvstk_neutrino_i2c_runtime_t *runtime);

bvstk_i2c_master_service_t *bvstk_neutrino_i2c_runtime_service(
    bvstk_neutrino_i2c_runtime_t *runtime);

void bvstk_neutrino_i2c_runtime_lock(
    bvstk_neutrino_i2c_runtime_t *runtime);
void bvstk_neutrino_i2c_runtime_unlock(
    bvstk_neutrino_i2c_runtime_t *runtime);

/* Caller must hold the runtime state lock. */
int bvstk_neutrino_i2c_runtime_sync_locked(
    bvstk_neutrino_i2c_runtime_t *runtime,
    size_t device_id,
    int save_to_storage);

/* Execute the same command grammar as the FreeRTOS `i2c` shell command. */
int bvstk_neutrino_i2c_runtime_command(
    bvstk_neutrino_i2c_runtime_t *runtime,
    const char *command_line,
    char *response,
    size_t response_capacity);

#endif /* BVSTK_NEUTRINO_I2C_RUNTIME_H */
