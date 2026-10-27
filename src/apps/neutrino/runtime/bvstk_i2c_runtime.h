#ifndef BVSTK_NEUTRINO_I2C_RUNTIME_H
#define BVSTK_NEUTRINO_I2C_RUNTIME_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "apps/neutrino/config/bvstk_i2c_config_store.h"
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "ports/neutrino-zynq7000/os/bvstk_sync_neutrino.h"
#include "ports/neutrino-zynq7000/os/bvstk_worker.h"
#include "services/acquisition/bvstk_acquisition.h"
#include "ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "services/cache/bvstk_cache_bridges.h"

typedef struct {
    bvstk_neutrino_i2c_config_store_t config_store;
    bvstk_i2c_master_hw_t master_hardware;
    bvstk_i2c_slave_hw_t slave_hardware;
    bvstk_i2c_devices_t devices;
    bvstk_i2c_cache_t cache;
    bvstk_i2c_policy_t policy;
    bvstk_cache_i2c_bridge_t cache_bridge;
    bvstk_i2c_master_service_t master_service;
    bvstk_i2c_slave_service_t slave_service;
    bvstk_i2c_slave_neutrino_t slave_adapter;
    bvstk_neutrino_mutex_t hardware_mutex;
    pthread_mutex_t state_mutex;
    bvstk_neutrino_worker_t worker;
    bvstk_acquisition_state_t acquisition[I2C_CFG_MAX_DEVICES];
    uint64_t probe_count[I2C_CFG_MAX_DEVICES];
    uint64_t acquisition_count[I2C_CFG_MAX_DEVICES];
    uint64_t next_probe_ms;
    size_t probe_device, acquisition_device;
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
/* Install the catalog and event sink before IRQ/background producers start. */
int bvstk_neutrino_i2c_runtime_init_observed(bvstk_neutrino_i2c_runtime_t *,
    bvstk_cache_registry_t *, const bvstk_event_sink_t *);
void bvstk_neutrino_i2c_runtime_shutdown(
    bvstk_neutrino_i2c_runtime_t *runtime);
int bvstk_neutrino_i2c_runtime_ready(
    const bvstk_neutrino_i2c_runtime_t *runtime);
/* Bounded background turn. Caller holds state_mutex; also used with recording
 * backends and a supplied monotonic time in the runtime acceptance tests. */
void bvstk_neutrino_i2c_runtime_poll_locked(bvstk_neutrino_i2c_runtime_t *, uint64_t);
int bvstk_neutrino_i2c_runtime_start_background(bvstk_neutrino_i2c_runtime_t *);

bvstk_i2c_master_service_t *bvstk_neutrino_i2c_runtime_service(
    bvstk_neutrino_i2c_runtime_t *runtime);

void bvstk_neutrino_i2c_runtime_lock(
    bvstk_neutrino_i2c_runtime_t *runtime);
void bvstk_neutrino_i2c_runtime_unlock(
    bvstk_neutrino_i2c_runtime_t *runtime);

/* Caller holds state_mutex. Mirror the live device and valid cache to RAM;
 * this function never accesses profile storage. */
int bvstk_neutrino_i2c_runtime_sync_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id);

/* Explicit console SAVE; captures valid cache without reading the bus. */
int bvstk_neutrino_i2c_runtime_save_device_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id);

/* Caller holds state lock. Mirror policy only, without saving or seeding settings. */
int bvstk_neutrino_i2c_runtime_sync_policy_locked(
    bvstk_neutrino_i2c_runtime_t *runtime,
    size_t device_id);

/* Caller holds state lock. Persist the current profile without seeding settings. */
int bvstk_neutrino_i2c_runtime_save_policy_locked(
    bvstk_neutrino_i2c_runtime_t *runtime,
    size_t device_id);

/* Execute the same command grammar as the FreeRTOS `i2c` shell command. */
int bvstk_neutrino_i2c_runtime_command(
    bvstk_neutrino_i2c_runtime_t *runtime,
    const char *command_line,
    char *response,
    size_t response_capacity);

#endif /* BVSTK_NEUTRINO_I2C_RUNTIME_H */
