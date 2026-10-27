#ifndef BVSTK_NEUTRINO_MDIO_RUNTIME_H
#define BVSTK_NEUTRINO_MDIO_RUNTIME_H
#include "apps/neutrino/config/bvstk_mdio_config_store.h"
#include "ports/neutrino-zynq7000/os/bvstk_sync_neutrino.h"
#include "ports/neutrino-zynq7000/os/bvstk_worker.h"
#include "services/mdio/bvstk_mdio_service.h"
#include "services/acquisition/bvstk_acquisition.h"
#include "services/cache/bvstk_cache_bridges.h"

typedef struct {
    bvstk_neutrino_mdio_config_store_t config_store;
    bvstk_mdio_core_t core;
    bvstk_mdio_service_t service;
    bvstk_cache_mdio_bridge_t cache_bridge;
    bvstk_neutrino_mutex_t hardware_mutex, service_mutex;
    pthread_mutex_t state_mutex;
    bvstk_neutrino_worker_t worker;
    bvstk_acquisition_state_t acquisition[MDIO_CFG_MAX_DEVICES];
    uint64_t acquisition_count[MDIO_CFG_MAX_DEVICES];
    size_t acquisition_device;
    uint8_t state_initialized, hardware_mutex_initialized, service_mutex_initialized;
    uint8_t core_initialized, ready;
} bvstk_neutrino_mdio_runtime_t;

int bvstk_neutrino_mdio_runtime_init(bvstk_neutrino_mdio_runtime_t *);
int bvstk_neutrino_mdio_runtime_init_observed(bvstk_neutrino_mdio_runtime_t *,
    bvstk_cache_registry_t *, const bvstk_event_sink_t *);
void bvstk_neutrino_mdio_runtime_shutdown(bvstk_neutrino_mdio_runtime_t *);
bvstk_mdio_service_t *bvstk_neutrino_mdio_runtime_service(bvstk_neutrino_mdio_runtime_t *);
void bvstk_neutrino_mdio_runtime_lock(bvstk_neutrino_mdio_runtime_t *);
void bvstk_neutrino_mdio_runtime_unlock(bvstk_neutrino_mdio_runtime_t *);
/* State lock serializes configuration snapshots with DCP2 policy changes. */
void bvstk_neutrino_mdio_runtime_poll_locked(bvstk_neutrino_mdio_runtime_t *, uint64_t);
int bvstk_neutrino_mdio_runtime_start_background(bvstk_neutrino_mdio_runtime_t *);
int bvstk_neutrino_mdio_runtime_sync_locked(bvstk_neutrino_mdio_runtime_t *, size_t);
int bvstk_neutrino_mdio_runtime_save_locked(bvstk_neutrino_mdio_runtime_t *, size_t);
#endif
