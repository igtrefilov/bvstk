#include "apps/neutrino/runtime/bvstk_i2c_runtime.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "services/control/bvstk_control_api.h"

enum {
    BVSTK_I2C_COMMAND_MAX = 512,
    BVSTK_I2C_COMMAND_ARGS_MAX = 12,
    BVSTK_I2C_OPERATION_TIMEOUT_MS = 100
};

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    int truncated;
} response_writer_t;

static void response_append(response_writer_t *writer,
                            const char *format,
                            ...)
{
    va_list arguments;
    int length;

    if (writer == NULL || writer->data == NULL || writer->capacity == 0U ||
        writer->truncated != 0 || writer->length >= writer->capacity) {
        return;
    }
    va_start(arguments, format);
    length = vsnprintf(writer->data + writer->length,
                       writer->capacity - writer->length,
                       format,
                       arguments);
    va_end(arguments);
    if (length < 0) {
        writer->truncated = 1;
        return;
    }
    if ((size_t)length >= writer->capacity - writer->length) {
        writer->length = writer->capacity - 1U;
        writer->data[writer->length] = '\0';
        writer->truncated = 1;
        return;
    }
    writer->length += (size_t)length;
}

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

static int apply_config_locked(bvstk_neutrino_i2c_runtime_t *runtime,
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

static int parse_number(const char *text, uint32_t maximum, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || text[0] == '\0' || value == NULL) {
        return 0;
    }
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > maximum) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static const i2c_device_config_t *select_device(
    bvstk_neutrino_i2c_runtime_t *runtime,
    const char *selector,
    size_t *device_id)
{
    uint32_t address;

    if (selector == NULL || device_id == NULL) {
        return NULL;
    }
    if (selector[0] == '@') {
        return parse_number(selector + 1, UINT8_C(0x7F), &address)
                   ? bvstk_neutrino_i2c_config_store_find_addr(
                         &runtime->config_store,
                         (uint8_t)address,
                         device_id)
                   : NULL;
    }
    {
        const i2c_device_config_t *config =
            bvstk_neutrino_i2c_config_store_find_name(&runtime->config_store,
                                                       selector,
                                                       device_id);
        if (config != NULL) {
            return config;
        }
    }
    return parse_number(selector, UINT8_C(0x7F), &address)
               ? bvstk_neutrino_i2c_config_store_find_addr(
                     &runtime->config_store,
                     (uint8_t)address,
                     device_id)
               : NULL;
}

static int parse_register(const i2c_device_config_t *config,
                          const char *text, uint32_t *value)
{
    const char *colon;
    uint32_t local;
    if (!config->endpoint.space_count) return parse_number(text, UINT8_MAX, value);
    if (!text || !(colon = strchr(text, ':')) ||
        !parse_number(colon + 1, UINT8_MAX, &local)) return 0;
    for (unsigned i = 0; i < config->endpoint.space_count; ++i) {
        const bvstk_address_space_t *space = &config->endpoint.spaces[i];
        if (strlen(space->id) == (size_t)(colon - text) &&
            !strncmp(space->id, text, (size_t)(colon - text)) && local < space->count) {
            *value = space->first + local;
            return 1;
        }
    }
    return 0;
}

static void command_help(response_writer_t *writer)
{
    response_append(writer,
                    "i2c usage:\n"
                    "  i2c list\n"
                    "  i2c <name> [info]\n"
                    "  i2c <name> r <reg>\n"
                    "  i2c <name> w <reg> <val>\n"
                    "  i2c <name> addr <addr_7b>\n"
                    "  i2c <name> policy\n"
                    "  i2c <name> policy show [rules|whitelist|blacklist]\n"
                    "  i2c <name> policy set <whitelist|blacklist>\n"
                    "  i2c <name> policy whitelist add|del <reg> <val>\n"
                    "  i2c <name> policy whitelist clear\n"
                    "  i2c <name> policy blacklist add|del <reg> <val>\n"
                    "  i2c <name> policy blacklist clear\n"
                    "  i2c <name> save\n"
                    "  Changes apply in RAM; save persists the profile.\n"
                    "  (address selector: @0xNN or numeric address)\n"
                    "  Multiple address spaces: use <name> and <space>:<reg>.\n");
}

static void show_rule_list(response_writer_t *writer,
                           const char *name,
                           const i2c_rule_entry_t *rules,
                           size_t count,
                           const bvstk_i2c_endpoint_t *endpoint)
{
    size_t index;

    response_append(writer, "%s (%u):\n", name, (unsigned)count);
    for (index = 0U; index < count; ++index) {
        const bvstk_address_space_t *space =
            bvstk_i2c_register_space(endpoint, rules[index].reg);
        response_append(writer,
                        "  { reg:%s%s0x%02X val:0x%02X }\n",
                        space ? space->id : "", space ? ":" : "",
                        (unsigned)(rules[index].reg - (space ? space->first : 0U)),
                        (unsigned)rules[index].val);
    }
}

static int show_policy(bvstk_neutrino_i2c_runtime_t *runtime,
                       size_t device_id,
                       const char *what,
                       response_writer_t *writer)
{
    bvstk_i2c_policy_entry_t policy;
    bvstk_i2c_device_t device;

    if (bvstk_i2c_master_service_device_info(&runtime->master_service,
                                            device_id, &device) != BVSTK_OK ||
        bvstk_i2c_master_service_get_policy(&runtime->master_service,
                                            device_id,
                                            &policy) != BVSTK_OK) {
        response_append(writer, "ERR (no device)\n");
        return 1;
    }
    if (what == NULL) {
        response_append(writer,
                        "POLICY=%s\n",
                        policy.mode == I2C_POLICY_BLACKLIST
                            ? "BLACKLIST" : "WHITELIST");
        return 0;
    }
    if (strcasecmp(what, "rules") == 0) {
        response_append(writer,
                        "POLICY=%s\n",
                        policy.mode == I2C_POLICY_BLACKLIST
                            ? "BLACKLIST" : "WHITELIST");
        show_rule_list(writer,
                       "WHITELIST",
                       policy.whitelist,
                       policy.whitelist_len, &device.endpoint);
        show_rule_list(writer,
                       "BLACKLIST",
                       policy.blacklist,
                       policy.blacklist_len, &device.endpoint);
        return 0;
    }
    if (strcasecmp(what, "whitelist") == 0) {
        show_rule_list(writer,
                       "WHITELIST",
                       policy.whitelist,
                       policy.whitelist_len, &device.endpoint);
        return 0;
    }
    if (strcasecmp(what, "blacklist") == 0) {
        show_rule_list(writer,
                       "BLACKLIST",
                       policy.blacklist,
                       policy.blacklist_len, &device.endpoint);
        return 0;
    }
    response_append(writer, "ERR\n");
    return 2;
}

static int policy_edit(bvstk_neutrino_i2c_runtime_t *runtime,
                       size_t device_id,
                       const i2c_device_config_t *config,
                       char **arguments,
                       size_t argument_count,
                       response_writer_t *writer)
{
    i2c_device_config_t next = *config;
    const char *subcommand = argument_count > 2U ? arguments[2] : NULL;

    if (subcommand == NULL) {
        return show_policy(runtime, device_id, NULL, writer);
    }
    if (strcasecmp(subcommand, "show") == 0) {
        if (argument_count > 4U) {
            response_append(writer, "ERR\n");
            return 2;
        }
        return show_policy(runtime,
                           device_id,
                           argument_count == 4U ? arguments[3] : NULL,
                           writer);
    }
    if (strcasecmp(subcommand, "set") == 0) {
        int apply_result;

        if (argument_count != 4U) {
            response_append(writer, "ERR\n");
            return 2;
        }
        if (strcasecmp(arguments[3], "whitelist") == 0) {
            next.policy = I2C_POLICY_WHITELIST;
        } else if (strcasecmp(arguments[3], "blacklist") == 0) {
            next.policy = I2C_POLICY_BLACKLIST;
        } else {
            response_append(writer, "ERR\n");
            return 2;
        }
        apply_result = apply_config_locked(runtime, device_id, &next);
        if (!apply_result) {
            response_append(writer, "ERR\n");
            return 1;
        }
        response_append(writer, "OK (RAM; use save to persist)\n");
        return 0;
    }
    if (strcasecmp(subcommand, "whitelist") == 0 ||
        strcasecmp(subcommand, "blacklist") == 0) {
        i2c_rule_entry_t *rules;
        size_t *rules_count;
        const char *action;
        uint32_t reg;
        uint32_t value;
        size_t index;

        if (argument_count == 3U) {
            return show_policy(runtime, device_id, subcommand, writer);
        }
        action = arguments[3];
        rules = strcasecmp(subcommand, "whitelist") == 0
                    ? next.whitelist : next.blacklist;
        rules_count = strcasecmp(subcommand, "whitelist") == 0
                          ? &next.whitelist_len : &next.blacklist_len;
        if (strcasecmp(action, "clear") == 0) {
            if (argument_count != 4U) {
                response_append(writer, "ERR\n");
                return 2;
            }
            *rules_count = 0U;
        } else {
            int found = 0;

            if (argument_count != 6U ||
                !parse_register(config, arguments[4], &reg) ||
                !parse_number(arguments[5], UINT8_MAX, &value) ||
                reg >= next.reg_count || value > next.max_value_code) {
                response_append(writer, "ERR\n");
                return 2;
            }
            for (index = 0U; index < *rules_count; ++index) {
                if (rules[index].reg == (uint8_t)reg &&
                    rules[index].val == (uint8_t)value) {
                    found = 1;
                    break;
                }
            }
            if (strcasecmp(action, "add") == 0) {
                if (!found && *rules_count < I2C_CFG_RULES_MAX) {
                    rules[*rules_count].reg = (uint8_t)reg;
                    rules[*rules_count].val = (uint8_t)value;
                    ++(*rules_count);
                }
            } else if (strcasecmp(action, "del") == 0 ||
                       strcasecmp(action, "delete") == 0) {
                size_t output = 0U;

                for (index = 0U; index < *rules_count; ++index) {
                    if (rules[index].reg == (uint8_t)reg &&
                        rules[index].val == (uint8_t)value) {
                        continue;
                    }
                    rules[output++] = rules[index];
                }
                *rules_count = output;
            } else {
                response_append(writer, "ERR\n");
                return 2;
            }
        }
        {
            int apply_result = apply_config_locked(runtime, device_id, &next);
            if (!apply_result) {
                response_append(writer, "ERR\n");
                return 1;
            }
        }
        response_append(writer, "OK (RAM; use save to persist)\n");
        return 0;
    }
    response_append(writer, "ERR\n");
    return 2;
}

static int execute_locked(bvstk_neutrino_i2c_runtime_t *runtime,
                          char **arguments,
                          size_t argument_count,
                          response_writer_t *writer)
{
    const i2c_device_config_t *config;
    size_t device_id = 0U;
    const char *operation;

    if (argument_count == 0U ||
        strcasecmp(arguments[0], "-h") == 0 ||
        strcasecmp(arguments[0], "--help") == 0) {
        command_help(writer);
        return 0;
    }
    if (strcasecmp(arguments[0], "list") == 0) {
        size_t index;

        if (argument_count != 1U) {
            response_append(writer, "ERR\n");
            return 2;
        }
        response_append(writer,
                        "I2C devices: %u\n",
                        (unsigned)runtime->config_store.device_count);
        for (index = 0U;
             index < runtime->config_store.device_count;
             ++index) {
            bvstk_i2c_device_t device;

            if (bvstk_i2c_master_service_device_info(&runtime->master_service,
                                                      index,
                                                      &device) == BVSTK_OK) {
                response_append(writer,
                                "  %u: %s addr=0x%02X regs=%u max_value=%u\n",
                                (unsigned)index,
                                device.name,
                                (unsigned)device.addr_7b,
                                (unsigned)device.reg_count,
                                (unsigned)device.max_value_code);
            }
        }
        return 0;
    }
    config = select_device(runtime, arguments[0], &device_id);
    if (config == NULL) {
        response_append(writer, "ERR (device not found)\n");
        return 1;
    }
    operation = argument_count > 1U ? arguments[1] : "info";
    char qualified[48];
    if (config->endpoint.space_count && strcasecmp(arguments[0], config->name)) {
        if (strcasecmp(operation, "policy") == 0) {
            response_append(writer, "Use the device name and <space>:<reg> for multi-space policies.\n");
            return 2;
        }
        if (argument_count >= 3U &&
            (!strcasecmp(operation, "r") || !strcasecmp(operation, "read") ||
             !strcasecmp(operation, "w") || !strcasecmp(operation, "write"))) {
            uint32_t address;
            const char *selector = arguments[0] + (arguments[0][0] == '@');
            if (!parse_number(selector, 0x7fU, &address) || strchr(arguments[2], ':') ||
                strlen(arguments[2]) > 12U) { response_append(writer, "ERR\n"); return 2; }
            int space = bvstk_i2c_address_space(&config->endpoint, config->addr_7b, (uint8_t)address);
            if (space < 0) { response_append(writer, "ERR\n"); return 2; }
            snprintf(qualified, sizeof(qualified), "%s:%s", config->endpoint.spaces[space].id,
                     arguments[2]);
            arguments[2] = qualified;
        }
    }
    if (strcasecmp(operation, "info") == 0) {
        bvstk_i2c_device_t device;
        bvstk_i2c_policy_entry_t policy;

        if (argument_count > 2U ||
            bvstk_i2c_master_service_device_info(&runtime->master_service,
                                                  device_id,
                                                  &device) != BVSTK_OK ||
            bvstk_i2c_master_service_get_policy(&runtime->master_service,
                                                 device_id,
                                                 &policy) != BVSTK_OK) {
            response_append(writer, "ERR (no device)\n");
            return 1;
        }
        response_append(writer,
                        "selected: %s addr=0x%02X regs=%u max_value=%u "
                        "policy=%s\n",
                        device.name,
                        (unsigned)device.addr_7b,
                        (unsigned)device.reg_count,
                        (unsigned)device.max_value_code,
                        policy.mode == I2C_POLICY_BLACKLIST
                            ? "blacklist" : "whitelist");
        response_append(writer,
                        "file=%s whitelist_len=%u blacklist_len=%u "
                        "settings_len=%u\n",
                        config->file_name[0] != '\0' ? config->file_name : "?",
                        (unsigned)policy.whitelist_len,
                        (unsigned)policy.blacklist_len,
                        (unsigned)config->settings_len);
        bvstk_i2c_presence_t presence = BVSTK_I2C_PRESENCE_UNKNOWN;
        unsigned valid = 0, physical = 0;
        (void)bvstk_i2c_master_service_device_presence(&runtime->master_service, device_id, &presence);
        for (unsigned reg = 0; reg < config->reg_count; ++reg) {
            bvstk_i2c_cache_entry_t entry;
            if (bvstk_i2c_master_service_cache_entry(&runtime->master_service,
                    device_id, (uint8_t)reg, &entry) == BVSTK_OK && entry.valid) {
                ++valid;
                if (!entry.seeded) ++physical;
            }
        }
        response_append(writer, "presence=%s cache_valid=%u cache_physical=%u probes=%llu acquisition=%llu\n",
            presence == BVSTK_I2C_PRESENCE_RESPONDING ? "responding" :
            presence == BVSTK_I2C_PRESENCE_UNAVAILABLE ? "unavailable" : "unknown",
            valid, physical, (unsigned long long)runtime->probe_count[device_id],
            (unsigned long long)runtime->acquisition_count[device_id]);
        for (unsigned i = 0; i < config->endpoint.space_count; ++i)
            response_append(writer, "space=%s addr=0x%02X regs=%u\n",
                            config->endpoint.spaces[i].id,
                            (unsigned)config->endpoint.spaces[i].address,
                            (unsigned)config->endpoint.spaces[i].count);
        return 0;
    }
    if (strcasecmp(operation, "r") == 0 ||
        strcasecmp(operation, "read") == 0) {
        uint32_t reg;
        uint8_t value = 0U;
        bvstk_status_t status;

        if (argument_count != 3U ||
            !parse_register(config, arguments[2], &reg)) {
            response_append(writer, "ERR\n");
            return 2;
        }
        status = bvstk_i2c_master_service_read_from(&runtime->master_service,
                                                device_id,
                                                (uint8_t)reg,
                                                &value,
                                                BVSTK_EVENT_SOURCE_CONSOLE,
                                                BVSTK_I2C_OPERATION_TIMEOUT_MS);
        if (status != BVSTK_OK) {
            response_append(writer,
                            "ERR READ_FAILED %s\n",
                            bvstk_status_string(status));
            return 1;
        }
        if (config->endpoint.space_count)
            response_append(writer, "OK REG %s = 0x%02X %u\n",
                            arguments[2], (unsigned)value, (unsigned)value);
        else response_append(writer,
                        "OK REG 0x%02X = 0x%02X %u\n",
                        (unsigned)reg,
                        (unsigned)value,
                        (unsigned)value);
        return 0;
    }
    if (strcasecmp(operation, "w") == 0 ||
        strcasecmp(operation, "write") == 0) {
        uint32_t reg;
        uint32_t value;
        bvstk_status_t status;

        if (argument_count != 4U ||
            !parse_register(config, arguments[2], &reg) ||
            !parse_number(arguments[3], UINT8_MAX, &value)) {
            response_append(writer, "ERR\n");
            return 2;
        }
        if (reg >= config->reg_count || value > config->max_value_code) {
            response_append(writer, "ERR DENIED outside configured range\n");
            return 1;
        }
        status = bvstk_i2c_master_service_write(&runtime->master_service,
                                                 device_id,
                                                 (uint8_t)reg,
                                                 (uint8_t)value,
                                                 BVSTK_EVENT_SOURCE_CONSOLE,
                                                 BVSTK_I2C_OPERATION_TIMEOUT_MS);
        if (status == BVSTK_ERR_DENIED) {
            response_append(writer, "ERR DENIED by policy\n");
            return 1;
        }
        if (status != BVSTK_OK ||
            !bvstk_neutrino_i2c_runtime_sync_locked(runtime, device_id)) {
            response_append(writer,
                            "ERR WRITE_FAILED%s%s\n",
                            status != BVSTK_OK ? " " : "",
                            status != BVSTK_OK
                                ? bvstk_status_string(status) : "");
            return 1;
        }
        response_append(writer, "OK\n");
        return 0;
    }
    if (strcasecmp(operation, "addr") == 0 ||
        strcasecmp(operation, "address") == 0) {
        i2c_device_config_t next = *config;
        uint32_t address;
        int apply_result;

        if (argument_count != 3U ||
            !parse_number(arguments[2], UINT8_C(0x7F), &address)) {
            response_append(writer, "ERR\n");
            return 2;
        }
        next.addr_7b = (uint8_t)address;
        apply_result = apply_config_locked(runtime, device_id, &next);
        if (!apply_result) {
            response_append(writer, "ERR (failed to apply configuration)\n");
            return 1;
        }
        response_append(writer, "OK (RAM; use save to persist)\n");
        return 0;
    }
    if (strcasecmp(operation, "save") == 0) {
        if (argument_count != 2U) { response_append(writer, "ERR\n"); return 2; }
        if (!bvstk_neutrino_i2c_runtime_save_device_locked(runtime, device_id)) {
            response_append(writer, "ERR IO save failed; RAM configuration remains active\n");
            return 1;
        }
        response_append(writer, "OK saved /flash/config/i2c/%s\n", config->file_name);
        return 0;
    }
    if (strcasecmp(operation, "policy") == 0) {
        return policy_edit(runtime,
                           device_id,
                           config,
                           arguments,
                           argument_count,
                           writer);
    }
    response_append(writer, "ERR\n");
    return 2;
}

int bvstk_neutrino_i2c_runtime_command(
    bvstk_neutrino_i2c_runtime_t *runtime,
    const char *command_line,
    char *response,
    size_t response_capacity)
{
    response_writer_t writer;
    char line[BVSTK_I2C_COMMAND_MAX];
    char *arguments[BVSTK_I2C_COMMAND_ARGS_MAX];
    char *save = NULL;
    char *token;
    size_t argument_count = 0U;
    int result;

    if (response == NULL || response_capacity == 0U) {
        return 2;
    }
    memset(&writer, 0, sizeof(writer));
    writer.data = response;
    writer.capacity = response_capacity;
    response[0] = '\0';
    if (runtime == NULL || runtime->ready == 0U || command_line == NULL) {
        response_append(&writer, "ERR (I2C not ready)\n");
        return 1;
    }
    if (strlen(command_line) >= sizeof(line)) {
        response_append(&writer, "ERR (command too long)\n");
        return 2;
    }
    strcpy(line, command_line);
    token = strtok_r(line, " \t\r\n", &save);
    if (token != NULL && strcasecmp(token, "i2c") == 0) {
        token = strtok_r(NULL, " \t\r\n", &save);
    }
    while (token != NULL && argument_count < BVSTK_I2C_COMMAND_ARGS_MAX) {
        arguments[argument_count++] = token;
        token = strtok_r(NULL, " \t\r\n", &save);
    }
    if (token != NULL) {
        response_append(&writer, "ERR (too many arguments)\n");
        return 2;
    }
    bvstk_neutrino_i2c_runtime_lock(runtime);
    result = execute_locked(runtime, arguments, argument_count, &writer);
    bvstk_neutrino_i2c_runtime_unlock(runtime);
    return result;
}
