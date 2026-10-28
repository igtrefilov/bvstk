#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "services/control/bvstk_control_api.h"
#include "services/cli/bvstk_cli.h"

enum { BVSTK_I2C_OPERATION_TIMEOUT_MS = 100 };

static void publish_event(void *context, const bvstk_event_t *event)
{
    (void)context;
    if (event != NULL &&
        (event->type == BVSTK_EVENT_REG_DENIED ||
         event->type == BVSTK_EVENT_FAULT)) {
        fprintf(stderr,
                "bvstkd: I2C event type=%u status=%u source=%u "
                "addr=0x%02x reg=0x%02x value=0x%02x\n",
                (unsigned)event->type,
                (unsigned)event->status,
                (unsigned)event->source,
                (unsigned)event->arg0,
                (unsigned)event->arg1,
                (unsigned)event->arg2);
    }
}

void bvstk_neutrino_i2c_runtime_lock(
    bvstk_neutrino_i2c_runtime_t *runtime)
{
    if (runtime != NULL && runtime->state_mutex_initialized != 0U) {
        (void)pthread_mutex_lock(&runtime->state_mutex);
    }
}

void bvstk_neutrino_i2c_runtime_unlock(
    bvstk_neutrino_i2c_runtime_t *runtime)
{
    if (runtime != NULL && runtime->state_mutex_initialized != 0U) {
        (void)pthread_mutex_unlock(&runtime->state_mutex);
    }
}

static int snapshot_i2c_config_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id,
    int include_cache_values, i2c_device_config_t *config)
{
    if (runtime == NULL || !runtime->ready) return 0;
    const bvstk_control_api_t control = {.i2c = &runtime->master_service};
    const i2c_device_config_t *stored =
        bvstk_neutrino_i2c_config_store_get(&runtime->config_store, device_id);
    return bvstk_control_i2c_snapshot(&control, device_id, stored,
               include_cache_values, config) == BVSTK_OK &&
        bvstk_neutrino_i2c_config_store_update(&runtime->config_store, device_id, config);
}

int bvstk_neutrino_i2c_runtime_sync_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id)
{
    i2c_device_config_t config;
    return snapshot_i2c_config_locked(runtime, device_id, 1, &config);
}

int bvstk_neutrino_i2c_runtime_sync_policy_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id)
{
    i2c_device_config_t config;
    return snapshot_i2c_config_locked(runtime, device_id, 0, &config);
}

int bvstk_neutrino_i2c_runtime_save_device_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id)
{
    i2c_device_config_t config;
    return snapshot_i2c_config_locked(runtime, device_id, 1, &config) &&
        bvstk_neutrino_i2c_config_store_save(&runtime->config_store, &config);
}

int bvstk_neutrino_i2c_runtime_save_policy_locked(
    bvstk_neutrino_i2c_runtime_t *runtime, size_t device_id)
{
    i2c_device_config_t config;
    return snapshot_i2c_config_locked(runtime, device_id, 0, &config) &&
        bvstk_neutrino_i2c_config_store_save(&runtime->config_store, &config);
}

int bvstk_neutrino_i2c_runtime_apply_locked(bvstk_neutrino_i2c_runtime_t *runtime,
                               size_t device_id,
                               const i2c_device_config_t *config)
{
    const i2c_device_config_t *stored;
    i2c_device_config_t previous;
    int target_changed;

    if (runtime == NULL || runtime->ready == 0U || config == NULL) {
        return 0;
    }
    stored = bvstk_neutrino_i2c_config_store_get(&runtime->config_store,
                                                  device_id);
    if (stored == NULL) {
        return 0;
    }
    previous = *stored;
    target_changed = runtime->slave_service_initialized != 0U &&
                     bvstk_i2c_slave_service_target(&runtime->slave_service) ==
                         device_id &&
                     previous.addr_7b != config->addr_7b;
    bvstk_control_api_t control = {.i2c = &runtime->master_service};
    if (bvstk_control_i2c_apply(&control, device_id, config) != BVSTK_OK) {
        return 0;
    }
    if (target_changed &&
        bvstk_i2c_slave_hw_set_address(&runtime->slave_hardware,
                                       config->addr_7b) != BVSTK_OK) {
        (void)bvstk_i2c_master_service_set_config(&runtime->master_service,
                                                   device_id,
                                                   &previous);
        return 0;
    }
    if (!bvstk_neutrino_i2c_config_store_update(&runtime->config_store,
                                                 device_id,
                                                 config)) {
        (void)bvstk_i2c_master_service_set_config(&runtime->master_service,
                                                   device_id,
                                                   &previous);
        if (target_changed) {
            (void)bvstk_i2c_slave_hw_set_address(&runtime->slave_hardware,
                                                  previous.addr_7b);
        }
        return 0;
    }
    return 1;
}

typedef struct {
    bvstk_neutrino_i2c_runtime_t *runtime;
    size_t id;
} acquisition_context_t;

static int neutrino_i2c_neutrino_i2c_acquisition_ready(void *context)
{
    acquisition_context_t *c = context;
    bvstk_i2c_presence_t presence;
    if (bvstk_i2c_master_service_device_presence(&c->runtime->master_service,
            c->id, &presence) != BVSTK_OK) return -1;
    return presence == BVSTK_I2C_PRESENCE_RESPONDING;
}
static int neutrino_i2c_acquisition_cached(void *context, unsigned reg, uint16_t *value)
{
    acquisition_context_t *c = context;
    bvstk_i2c_cache_entry_t entry;
    if (bvstk_i2c_master_service_cache_entry(&c->runtime->master_service,
            c->id, (uint8_t)reg, &entry) != BVSTK_OK || !entry.valid || entry.seeded) return 0;
    *value = entry.value;
    return 1;
}
static bvstk_status_t neutrino_i2c_acquisition_read(void *context, unsigned first, unsigned count)
{
    acquisition_context_t *c = context;
    ++c->runtime->acquisition_count[c->id];
    return bvstk_i2c_master_service_sample(&c->runtime->master_service,
        c->id, first, count, BVSTK_I2C_OPERATION_TIMEOUT_MS);
}
static void neutrino_i2c_acquisition_invalidate(void *context, unsigned first, unsigned count)
{
    acquisition_context_t *c = context;
    bvstk_cache_sink_enter(&c->runtime->cache.sink);
    for (unsigned r = first; r < first + count; ++r)
        (void)bvstk_i2c_cache_invalidate(&c->runtime->cache, c->id, (uint8_t)r);
    bvstk_cache_sink_leave(&c->runtime->cache.sink);
}

void bvstk_neutrino_i2c_runtime_poll_locked(bvstk_neutrino_i2c_runtime_t *runtime, uint64_t now)
{
    if (!runtime || !runtime->ready) return;
    size_t count = bvstk_i2c_master_service_device_count(&runtime->master_service);
    if (!count) return;
    if (now >= runtime->next_probe_ms) {
        size_t id = runtime->probe_device++ % count;
        bvstk_i2c_device_t device;
        runtime->next_probe_ms = now + 250U;
        if (bvstk_i2c_master_service_device_info(&runtime->master_service, id, &device) == BVSTK_OK &&
            device.presence_check_enabled) {
            ++runtime->probe_count[id];
            (void)bvstk_i2c_master_service_probe(&runtime->master_service, id,
                device.presence_check_reg, BVSTK_I2C_OPERATION_TIMEOUT_MS);
            return; /* At most one probe or acquisition chunk in this turn. */
        }
    }
    for (size_t n = 0; n < count; ++n) {
        size_t id = runtime->acquisition_device++ % count;
        const i2c_device_config_t *config =
            bvstk_neutrino_i2c_config_store_get(&runtime->config_store, id);
        if (!config || !config->profile.acquisition.enabled) continue;
        acquisition_context_t context = {runtime, id};
        bvstk_acquisition_io_t io = {&context, neutrino_i2c_neutrino_i2c_acquisition_ready, neutrino_i2c_acquisition_cached,
            neutrino_i2c_acquisition_read, neutrino_i2c_acquisition_invalidate};
        if (bvstk_acquisition_step(&config->profile.acquisition, &runtime->acquisition[id],
                &io, now)) break;
    }
}
static void background_step(void *context)
{
    bvstk_neutrino_i2c_runtime_t *runtime = context;
    /* Give local commands and the IRQ owner priority over periodic work. */
    if (pthread_mutex_trylock(&runtime->state_mutex)) return;
    bvstk_neutrino_i2c_runtime_poll_locked(runtime, bvstk_platform_now_ms());
    pthread_mutex_unlock(&runtime->state_mutex);
}
int bvstk_neutrino_i2c_runtime_start_background(bvstk_neutrino_i2c_runtime_t *runtime)
{
    return runtime && runtime->ready &&
        bvstk_neutrino_worker_start(&runtime->worker, background_step, runtime) == BVSTK_OK;
}

int bvstk_neutrino_i2c_runtime_init(
    bvstk_neutrino_i2c_runtime_t *runtime)
{
    return bvstk_neutrino_i2c_runtime_init_observed(runtime, NULL, NULL);
}

int bvstk_neutrino_i2c_runtime_init_observed(bvstk_neutrino_i2c_runtime_t *runtime,
    bvstk_cache_registry_t *registry, const bvstk_event_sink_t *sink)
{
#if !BVSTK_PL_HAS_I2C_CORE
    (void)runtime;
    (void)registry;
    (void)sink;
    return 0;
#else
    const i2c_device_config_t *configs;
    const i2c_device_config_t *first_config;
    size_t config_count;
    size_t device_id;
    bvstk_event_sink_t events;
    bvstk_status_t status;

    if (runtime == NULL) {
        return 0;
    }
    memset(runtime, 0, sizeof(*runtime));
    if (!bvstk_neutrino_i2c_config_store_init(&runtime->config_store,
                                               NULL,
                                               NULL)) {
        fprintf(stderr, "bvstkd: no valid I2C device configurations\n");
        return 0;
    }
    configs = bvstk_neutrino_i2c_config_store_devices(&runtime->config_store);
    config_count = bvstk_neutrino_i2c_config_store_count(&runtime->config_store);
    if (configs == NULL || config_count == 0U ||
        pthread_mutex_init(&runtime->state_mutex, NULL) != 0) {
        goto fail;
    }
    runtime->state_mutex_initialized = 1U;
    status = bvstk_i2c_devices_init_from_config(&runtime->devices,
                                                configs,
                                                config_count);
    if (status != BVSTK_OK) goto fail;
    status = bvstk_i2c_cache_init(&runtime->cache, config_count);
    if (status != BVSTK_OK) goto fail;
    status = bvstk_i2c_policy_init(&runtime->policy, configs, config_count);
    if (status != BVSTK_OK) goto fail;
    for (device_id = 0U; device_id < config_count; ++device_id) {
        size_t setting;

        for (setting = 0U;
             setting < configs[device_id].settings_len;
             ++setting) {
            (void)bvstk_i2c_cache_seed(
                &runtime->cache,
                device_id,
                configs[device_id].settings[setting].reg,
                configs[device_id].settings[setting].val);
        }
    }
    if (bvstk_neutrino_mutex_init(&runtime->hardware_mutex) != BVSTK_OK) {
        goto fail;
    }
    runtime->hardware_mutex_initialized = 1U;
    status = bvstk_i2c_master_hw_init(
        &runtime->master_hardware,
        NULL,
        &runtime->hardware_mutex.public_mutex);
    if (status != BVSTK_OK) goto fail;
    runtime->master_hardware_initialized = 1U;
    memset(&events, 0, sizeof(events));
    events.publish = publish_event;
    if (sink) events = *sink;
    status = bvstk_i2c_master_service_init(&runtime->master_service,
                                           &runtime->master_hardware,
                                           NULL,
                                           &runtime->devices,
                                           &runtime->cache,
                                           &runtime->policy,
                                           &events);
    if (status != BVSTK_OK) goto fail;
    runtime->master_service_initialized = 1U;
    if (registry && bvstk_cache_attach_i2c(&runtime->cache_bridge, registry,
            &runtime->master_service, configs) != BVSTK_OK) goto fail;

    first_config = bvstk_neutrino_i2c_config_store_get(&runtime->config_store,
                                                        0U);
    if (first_config == NULL ||
        bvstk_i2c_slave_hw_init(&runtime->slave_hardware) != BVSTK_OK) {
        goto fail;
    }
    runtime->slave_hardware_initialized = 1U;
    uint8_t addresses[128];
    size_t address_count = 1U;
    for (size_t i = 0; i < runtime->devices.count; ++i) {
        const bvstk_i2c_device_t *d = &runtime->devices.items[i];
        addresses[i] = d->addr_7b;
        if (d->endpoint.parent[0] || d->endpoint.transfer.configured)
            address_count = runtime->devices.count;
    }
    if (address_count == runtime->devices.count)
        address_count = bvstk_i2c_devices_addresses(&runtime->devices, addresses);
    if (bvstk_i2c_slave_hw_set_addresses(&runtime->slave_hardware,
                                        addresses, address_count) != BVSTK_OK ||
        bvstk_i2c_slave_service_init(&runtime->slave_service,
                                     &runtime->devices,
                                     &runtime->cache,
                                     &runtime->master_service,
                                     0U) != BVSTK_OK) {
        goto fail;
    }
    runtime->slave_service_initialized = 1U;
    status = bvstk_i2c_slave_neutrino_start(&runtime->slave_adapter,
                                            &runtime->slave_hardware,
                                            &runtime->slave_service,
                                            &runtime->state_mutex);
    if (status != BVSTK_OK) {
        fprintf(stderr,
                "bvstkd: I2C slave IRQ initialization failed: %s\n",
                bvstk_status_string(status));
        goto fail;
    }
    runtime->ready = 1U;
    if (!bvstk_neutrino_i2c_runtime_start_background(runtime))
        fprintf(stderr, "bvstkd: I2C background worker failed to start\n");
    return 1;

fail:
    bvstk_neutrino_i2c_runtime_shutdown(runtime);
    return 0;
#endif
}

void bvstk_neutrino_i2c_runtime_shutdown(
    bvstk_neutrino_i2c_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }
    bvstk_neutrino_worker_stop(&runtime->worker);
    runtime->ready = 0U;
    if (runtime->slave_adapter.initialized != 0U) {
        /* The IRQ adapter is intentionally process-lifetime. */
        bvstk_i2c_slave_neutrino_stop(&runtime->slave_adapter);
        return;
    }
    if (runtime->slave_service_initialized != 0U) {
        bvstk_i2c_slave_service_shutdown(&runtime->slave_service);
    }
    bvstk_cache_detach_i2c(&runtime->cache_bridge);
    if (runtime->slave_hardware_initialized != 0U) {
        bvstk_i2c_slave_hw_shutdown(&runtime->slave_hardware);
    }
    if (runtime->master_service_initialized != 0U) {
        bvstk_i2c_master_service_shutdown(&runtime->master_service);
    }
    if (runtime->master_hardware_initialized != 0U) {
        bvstk_i2c_master_hw_shutdown(&runtime->master_hardware);
    }
    bvstk_i2c_policy_shutdown(&runtime->policy);
    bvstk_i2c_cache_shutdown(&runtime->cache);
    bvstk_i2c_devices_shutdown(&runtime->devices);
    if (runtime->hardware_mutex_initialized != 0U) {
        bvstk_neutrino_mutex_destroy(&runtime->hardware_mutex);
    }
    if (runtime->state_mutex_initialized != 0U) {
        (void)pthread_mutex_destroy(&runtime->state_mutex);
    }
    bvstk_neutrino_i2c_config_store_shutdown(&runtime->config_store);
    memset(runtime, 0, sizeof(*runtime));
}

int bvstk_neutrino_i2c_runtime_ready(
    const bvstk_neutrino_i2c_runtime_t *runtime)
{
    return runtime != NULL && runtime->ready != 0U;
}

bvstk_i2c_master_service_t *bvstk_neutrino_i2c_runtime_service(
    bvstk_neutrino_i2c_runtime_t *runtime)
{
    return runtime != NULL && runtime->ready != 0U
               ? &runtime->master_service : NULL;
}

static const i2c_device_config_t *cli_config(void *context, size_t id)
{ return bvstk_neutrino_i2c_config_store_get(&((bvstk_neutrino_i2c_runtime_t *)context)->config_store, id); }
static bvstk_status_t cli_apply(void *context, size_t id, const i2c_device_config_t *config)
{ return bvstk_neutrino_i2c_runtime_apply_locked(context, id, config) ? BVSTK_OK : BVSTK_ERR_INTERNAL; }
static bvstk_status_t cli_sync(void *context, size_t id)
{ return bvstk_neutrino_i2c_runtime_sync_locked(context, id) ? BVSTK_OK : BVSTK_ERR_INTERNAL; }
static bvstk_status_t cli_save(void *context, bool mdio, size_t id)
{ return mdio ? BVSTK_ERR_UNSUPPORTED : bvstk_neutrino_i2c_runtime_save_device_locked(context, id) ? BVSTK_OK : BVSTK_ERR_IO; }
void bvstk_neutrino_i2c_cli_init(bvstk_cli_t *cli, bvstk_neutrino_i2c_runtime_t *runtime,
    const bvstk_output_t *output)
{
    *cli = (bvstk_cli_t){0}; cli->output = *output; cli->context = runtime;
    cli->i2c = bvstk_neutrino_i2c_runtime_service(runtime);
    cli->i2c_config = cli_config; cli->i2c_apply = cli_apply;
    cli->i2c_sync = cli_sync; cli->save = cli_save;
}
int bvstk_neutrino_i2c_runtime_command(bvstk_neutrino_i2c_runtime_t *runtime,
    const char *command, char *response, size_t capacity)
{
    if (!runtime || !command || !response || !capacity) return 2;
    response[0] = '\0';
    char line[512], *argv[40];
    if (strlen(command) >= sizeof(line)) { snprintf(response, capacity, "ERR command too long\n"); return 2; }
    strcpy(line, command);
    int argc = bvstk_cli_tokens(line, argv, 40);
    if (argc < 0) { snprintf(response, capacity, "ERR too many arguments\n"); return 2; }
    int first = argc && !strcasecmp(argv[0], "i2c");
    bvstk_cli_buffer_t buffer = {response, capacity, 0, false};
    bvstk_output_t output = {bvstk_cli_buffer_write, &buffer};
    bvstk_cli_t cli;
    bvstk_neutrino_i2c_runtime_lock(runtime);
    bvstk_neutrino_i2c_cli_init(&cli, runtime, &output);
    (void)bvstk_i2c_cli(&cli, argc - first, argv + first);
    bvstk_neutrino_i2c_runtime_unlock(runtime);
    return cli.result;
}
