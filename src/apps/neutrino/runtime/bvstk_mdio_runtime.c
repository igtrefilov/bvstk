#include "apps/neutrino/runtime/bvstk_mdio_runtime.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    bvstk_neutrino_mdio_runtime_t *runtime;
    size_t id;
} acquisition_context_t;

static int acquisition_ready(void *context)
{
    acquisition_context_t *c = context;
    return bvstk_mdio_service_ready(&c->runtime->service, c->id);
}
static int acquisition_cached(void *context, unsigned reg, uint16_t *value)
{
    acquisition_context_t *c = context;
    bvstk_mdio_cache_entry_t entry;
    if (bvstk_mdio_service_cache_entry(&c->runtime->service, c->id,
            (uint8_t)reg, &entry) != BVSTK_OK || !entry.valid || entry.seeded) return 0;
    *value = entry.value;
    return 1;
}
static bvstk_status_t acquisition_read(void *context, unsigned first, unsigned count)
{
    acquisition_context_t *c = context;
    if (count != 1U) return BVSTK_ERR_RANGE;
    ++c->runtime->acquisition_count[c->id];
    return bvstk_mdio_service_sample(&c->runtime->service, c->id, (uint8_t)first, 20U);
}
static void acquisition_invalidate(void *context, unsigned first, unsigned count)
{
    acquisition_context_t *c = context;
    (void)bvstk_mdio_service_invalidate_range(&c->runtime->service, c->id, first, count);
}

void bvstk_neutrino_mdio_runtime_poll_locked(bvstk_neutrino_mdio_runtime_t *runtime, uint64_t now)
{
    if (!runtime || !runtime->ready) return;
    (void)bvstk_mdio_service_step(&runtime->service, now, 20U);
    size_t count = bvstk_mdio_service_device_count(&runtime->service);
    for (size_t n = 0; n < count; ++n) {
        size_t id = runtime->acquisition_device++ % count;
        const mdio_phy_config_t *config =
            bvstk_neutrino_mdio_config_store_get(&runtime->config_store, id);
        if (!config || !config->profile.acquisition.enabled) continue;
        acquisition_context_t context = {runtime, id};
        bvstk_acquisition_io_t io = {&context, acquisition_ready, acquisition_cached,
            acquisition_read, acquisition_invalidate};
        if (bvstk_acquisition_step(&config->profile.acquisition, &runtime->acquisition[id],
                &io, now)) break;
    }
}

void bvstk_neutrino_mdio_runtime_lock(bvstk_neutrino_mdio_runtime_t *runtime)
{
    if (runtime && runtime->state_initialized) pthread_mutex_lock(&runtime->state_mutex);
}
void bvstk_neutrino_mdio_runtime_unlock(bvstk_neutrino_mdio_runtime_t *runtime)
{
    if (runtime && runtime->state_initialized) pthread_mutex_unlock(&runtime->state_mutex);
}
static void background_step(void *context)
{
    bvstk_neutrino_mdio_runtime_t *runtime = context;
    if (pthread_mutex_trylock(&runtime->state_mutex)) return;
    bvstk_neutrino_mdio_runtime_poll_locked(runtime, bvstk_platform_now_ms());
    pthread_mutex_unlock(&runtime->state_mutex);
}
int bvstk_neutrino_mdio_runtime_start_background(bvstk_neutrino_mdio_runtime_t *runtime)
{
    return runtime && runtime->ready &&
        bvstk_neutrino_worker_start(&runtime->worker, background_step, runtime) == BVSTK_OK;
}

static void publish_event(void *context, const bvstk_event_t *event)
{
    (void)context;
    if (event && (event->type == BVSTK_EVENT_REG_DENIED || event->type == BVSTK_EVENT_FAULT))
        fprintf(stderr, "bvstkd: MDIO event type=%u status=%u phy=%u reg=%u value=0x%04x\n",
            (unsigned)event->type, (unsigned)event->status, (unsigned)event->arg0,
            (unsigned)event->arg1, (unsigned)event->arg2);
}

int bvstk_neutrino_mdio_runtime_init(bvstk_neutrino_mdio_runtime_t *runtime)
{
    return bvstk_neutrino_mdio_runtime_init_observed(runtime, NULL, NULL);
}

int bvstk_neutrino_mdio_runtime_init_observed(bvstk_neutrino_mdio_runtime_t *runtime,
    bvstk_cache_registry_t *registry, const bvstk_event_sink_t *sink)
{
#if !BVSTK_PL_HAS_MDIO_CORE
    (void)runtime;
    (void)registry;
    (void)sink;
    return 0;
#else
    if (!runtime) return 0;
    memset(runtime, 0, sizeof(*runtime));
    if (!bvstk_neutrino_mdio_config_store_init(&runtime->config_store, NULL, NULL)) goto fail;
    if (pthread_mutex_init(&runtime->state_mutex, NULL)) goto fail;
    runtime->state_initialized = 1;
    if (bvstk_neutrino_mutex_init(&runtime->hardware_mutex) != BVSTK_OK) goto fail;
    runtime->hardware_mutex_initialized = 1;
    if (bvstk_neutrino_mutex_init(&runtime->service_mutex) != BVSTK_OK) goto fail;
    runtime->service_mutex_initialized = 1;
    if (bvstk_mdio_core_init(&runtime->core, NULL,
            &runtime->hardware_mutex.public_mutex) != BVSTK_OK) goto fail;
    runtime->core_initialized = 1;
    bvstk_event_sink_t events = {NULL, publish_event};
    if (sink) events = *sink;
    if (bvstk_mdio_service_init(&runtime->service, &runtime->core, NULL,
            runtime->config_store.devices, runtime->config_store.device_count, &events) != BVSTK_OK) goto fail;
    runtime->service.mutex = runtime->service_mutex.public_mutex;
    if (registry && bvstk_cache_attach_mdio(&runtime->cache_bridge, registry,
            &runtime->service) != BVSTK_OK) goto fail;
    runtime->ready = 1;
    if (!bvstk_neutrino_mdio_runtime_start_background(runtime)) {
        fprintf(stderr, "bvstkd: MDIO worker failed to start\n");
        goto fail;
    }
    return 1;
fail:
    bvstk_neutrino_mdio_runtime_shutdown(runtime);
    return 0;
#endif
}

void bvstk_neutrino_mdio_runtime_shutdown(bvstk_neutrino_mdio_runtime_t *runtime)
{
    if (!runtime) return;
    bvstk_neutrino_worker_stop(&runtime->worker);
    runtime->ready = 0;
    bvstk_cache_detach_mdio(&runtime->cache_bridge);
    bvstk_mdio_service_shutdown(&runtime->service);
    if (runtime->core_initialized) bvstk_mdio_core_shutdown(&runtime->core);
    if (runtime->hardware_mutex_initialized) bvstk_neutrino_mutex_destroy(&runtime->hardware_mutex);
    if (runtime->service_mutex_initialized) bvstk_neutrino_mutex_destroy(&runtime->service_mutex);
    if (runtime->state_initialized) pthread_mutex_destroy(&runtime->state_mutex);
    memset(runtime, 0, sizeof(*runtime));
}

bvstk_mdio_service_t *bvstk_neutrino_mdio_runtime_service(bvstk_neutrino_mdio_runtime_t *runtime)
{
    return runtime && runtime->ready ? &runtime->service : NULL;
}
int bvstk_neutrino_mdio_runtime_sync_locked(bvstk_neutrino_mdio_runtime_t *runtime, size_t id)
{
    mdio_phy_config_t config;
    return runtime && runtime->ready &&
        bvstk_mdio_service_get_config(&runtime->service, id, &config) == BVSTK_OK &&
        bvstk_neutrino_mdio_config_store_update(&runtime->config_store, id, &config);
}
int bvstk_neutrino_mdio_runtime_save_locked(bvstk_neutrino_mdio_runtime_t *runtime, size_t id)
{
    return bvstk_neutrino_mdio_runtime_sync_locked(runtime, id) &&
        bvstk_neutrino_mdio_config_store_save(&runtime->config_store,
            bvstk_neutrino_mdio_config_store_get(&runtime->config_store, id));
}
