#include "apps/freertos/runtime/bvstk_runtime.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_observer.h"
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "drivers/pl/mdio/bvstk_mdio_core.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"
#include "ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.h"
#include "ports/freertos-xilinx/os/mdio/bvstk_mdio_freertos.h"
#include "services/cache/bvstk_cache_bridges.h"
#include "services/acquisition/bvstk_acquisition.h"
#include "xil_printf.h"

static bvstk_cache_registry_t s_cache_registry;
static bvstk_cache_i2c_bridge_t s_cache_i2c;
static bvstk_cache_mdio_bridge_t s_cache_mdio;

static void cache_enter(void *context) { (void)context; taskENTER_CRITICAL(); }
static void cache_leave(void *context) { (void)context; taskEXIT_CRITICAL(); }

bvstk_cache_registry_t *bvstk_runtime_cache_registry(void)
{
    return &s_cache_registry;
}

static bvstk_i2c_master_hw_t s_i2c_master_hw;
static bvstk_i2c_slave_hw_t s_i2c_slave_hw;
static bvstk_i2c_devices_t s_i2c_devices;
static bvstk_i2c_cache_t s_i2c_cache;
static bvstk_i2c_policy_t s_i2c_policy;
static bvstk_i2c_master_service_t s_i2c_master_service;
static bvstk_i2c_slave_service_t s_i2c_slave_service;
static bvstk_i2c_slave_freertos_t s_i2c_slave_adapter;
static bvstk_freertos_mutex_t s_i2c_mutex;
static bvstk_mdio_core_t s_mdio_core;
static bvstk_mdio_service_t s_mdio_service;
static bvstk_freertos_mutex_t s_mdio_mutex;
static bvstk_freertos_mutex_t s_mdio_service_mutex;
static bvstk_spi_core_t s_spi_core;
static bvstk_freertos_mutex_t s_spi_mutex;
static volatile int s_i2c_ready;
static volatile int s_i2c_slave_ready;
static volatile int s_mdio_ready;
static volatile int s_spi_ready;
#if BVSTK_PL_RUNTIME_ENABLED
static TaskHandle_t s_runtime_task;
#if BVSTK_PL_HAS_I2C_CORE
static TaskHandle_t s_i2c_health_task;
#endif
#endif

static void update_setting(i2c_device_config_t *config,
                           uint8_t reg,
                           uint8_t value)
{
    size_t i;

    for (i = 0U; i < config->settings_len; ++i) {
        if (config->settings[i].reg == reg) {
            config->settings[i].val = value;
            return;
        }
    }
    if (config->settings_len < I2C_CFG_SETTINGS_MAX) {
        config->settings[config->settings_len].reg = reg;
        config->settings[config->settings_len].val = value;
        config->settings_len++;
    }
}

#if BVSTK_PL_RUNTIME_ENABLED
#if BVSTK_PL_HAS_I2C_CORE
static int initialize_i2c(const i2c_device_config_t *configs,
                          size_t config_count,
                          const bvstk_event_sink_t *events)
{
    uint8_t addresses[128];
    size_t address_count = config_count ? 1U : 0U;
    bvstk_status_t status;

    status = bvstk_i2c_devices_init_from_config(&s_i2c_devices,
                                                configs,
                                                config_count);
    if (status != BVSTK_OK ||
        bvstk_i2c_cache_init(&s_i2c_cache, config_count) != BVSTK_OK ||
        bvstk_i2c_policy_init(&s_i2c_policy, configs, config_count) != BVSTK_OK) {
        return 0;
    }

    for (size_t device_id = 0U; device_id < config_count; ++device_id) {
        for (size_t i = 0U; i < configs[device_id].settings_len; ++i) {
            (void)bvstk_i2c_cache_seed(&s_i2c_cache,
                                        device_id,
                                        configs[device_id].settings[i].reg,
                                        configs[device_id].settings[i].val);
        }
    }

    if (bvstk_freertos_mutex_init(&s_i2c_mutex) != BVSTK_OK ||
        bvstk_i2c_master_hw_init(&s_i2c_master_hw,
                                 NULL,
                                 &s_i2c_mutex.public_mutex) != BVSTK_OK) {
        bvstk_freertos_mutex_destroy(&s_i2c_mutex);
        return 0;
    }
    status = bvstk_i2c_master_service_init(&s_i2c_master_service,
                                           &s_i2c_master_hw,
                                           NULL,
                                           &s_i2c_devices,
                                           &s_i2c_cache,
                                           &s_i2c_policy,
                                           events);
    if (status != BVSTK_OK) {
        bvstk_i2c_master_hw_shutdown(&s_i2c_master_hw);
        bvstk_freertos_mutex_destroy(&s_i2c_mutex);
        return 0;
    }

    if (bvstk_cache_attach_i2c(&s_cache_i2c, &s_cache_registry, &s_i2c_master_service, configs) != BVSTK_OK)
        xil_printf("[cache] I2C catalog registration failed\r\n");
    s_i2c_ready = 1;
    for (size_t i = 0; i < config_count; ++i) {
        addresses[i] = configs[i].addr_7b;
        if (configs[i].endpoint.parent[0] || configs[i].endpoint.transfer.configured)
            address_count = config_count;
    }
    if (address_count == config_count)
        address_count = bvstk_i2c_devices_addresses(&s_i2c_devices, addresses);
    if (config_count != 0U &&
        bvstk_i2c_slave_hw_init(&s_i2c_slave_hw) == BVSTK_OK &&
        bvstk_i2c_slave_hw_set_addresses(&s_i2c_slave_hw, addresses, address_count) == BVSTK_OK &&
        bvstk_i2c_slave_service_init(&s_i2c_slave_service,
                                     &s_i2c_devices,
                                     &s_i2c_cache,
                                     &s_i2c_master_service,
                                     0U) == BVSTK_OK &&
        bvstk_i2c_slave_freertos_start(&s_i2c_slave_adapter,
                                       &s_i2c_slave_hw,
                                       &s_i2c_slave_service) == BVSTK_OK) {
        s_i2c_slave_ready = 1;
    }
    return 1;
}
#endif

#if BVSTK_PL_HAS_I2C_CORE
enum {
    /* One read per turn keeps the bus available to external masters and
     * lets a six-device topology settle in roughly 1.5 seconds. */
    BVSTK_I2C_HEALTH_PERIOD_MS = 250U,
    BVSTK_I2C_HEALTH_TIMEOUT_MS = 100U,
    BVSTK_I2C_HEALTH_STACK = 1536U,
    BVSTK_I2C_HEALTH_PRIORITY = tskIDLE_PRIORITY + 1
};

static void i2c_health_task(void *argument)
{
    size_t device_id = 0U;
    (void)argument;

    for (;;) {
        size_t count = bvstk_i2c_master_service_device_count(&s_i2c_master_service);
        if (s_i2c_ready != 0 && count != 0U) {
            bvstk_i2c_device_t device;
            if (device_id >= count) device_id = 0U;
            if (bvstk_i2c_master_service_device_info(&s_i2c_master_service,
                                                     device_id, &device) == BVSTK_OK &&
                device.reg_count != 0U) {
                if (device.presence_check_enabled) {
                    (void)bvstk_i2c_master_service_probe(
                        &s_i2c_master_service, device_id,
                        device.presence_check_reg,
                        BVSTK_I2C_HEALTH_TIMEOUT_MS);
                }
            }
            device_id = (device_id + 1U) % count;
        }
        vTaskDelay(pdMS_TO_TICKS(BVSTK_I2C_HEALTH_PERIOD_MS));
    }
}

static void start_i2c_health_task(void)
{
    if (s_i2c_health_task != NULL || s_i2c_ready == 0) return;
    if (xTaskCreate(i2c_health_task, "i2c_health", BVSTK_I2C_HEALTH_STACK,
                    NULL, BVSTK_I2C_HEALTH_PRIORITY, &s_i2c_health_task) != pdPASS) {
        s_i2c_health_task = NULL;
        xil_printf("[i2c] health poll task creation failed\r\n");
    }
}
#endif

#if BVSTK_PL_HAS_I2C_CORE || BVSTK_PL_HAS_MDIO_CORE
typedef struct {
    bvstk_acquisition_plan_t plan;
    bvstk_acquisition_state_t state;
    size_t device_id;
    uint8_t mdio;
} acquisition_device_t;
static acquisition_device_t s_acquisition[I2C_CFG_MAX_DEVICES + MDIO_CFG_MAX_DEVICES];
static size_t s_acquisition_count;

static int acquisition_ready(void *context)
{
    const acquisition_device_t *d = context;
    if (d->mdio) return bvstk_mdio_service_ready(&s_mdio_service, d->device_id);
    bvstk_i2c_presence_t presence;
    if (bvstk_i2c_master_service_device_presence(&s_i2c_master_service, d->device_id, &presence) != BVSTK_OK)
        return -1;
    return presence == BVSTK_I2C_PRESENCE_RESPONDING;
}
static int acquisition_cached(void *context, unsigned resource, uint16_t *value)
{
    const acquisition_device_t *d = context;
    if (d->mdio) {
        bvstk_mdio_cache_entry_t e;
        if (bvstk_mdio_service_cache_entry(&s_mdio_service, d->device_id, (uint8_t)resource, &e) != BVSTK_OK ||
            !e.valid || e.seeded) return 0;
        *value = e.value;
    } else {
        bvstk_i2c_cache_entry_t e;
        if (bvstk_i2c_master_service_cache_entry(&s_i2c_master_service, d->device_id, (uint8_t)resource, &e) != BVSTK_OK ||
            !e.valid || e.seeded) return 0;
        *value = e.value;
    }
    return 1;
}
static bvstk_status_t acquisition_read(void *context, unsigned first, unsigned count)
{
    const acquisition_device_t *d = context;
    if (d->mdio) return count == 1 ?
        bvstk_mdio_service_sample(&s_mdio_service, d->device_id, (uint8_t)first, 100U) : BVSTK_ERR_RANGE;
    return bvstk_i2c_master_service_sample(&s_i2c_master_service, d->device_id, first, count, 100U);
}
static void acquisition_invalidate(void *context, unsigned first, unsigned count)
{
    const acquisition_device_t *d = context;
    if (d->mdio) {
        (void)bvstk_mdio_service_invalidate_range(&s_mdio_service, d->device_id, first, count);
        return;
    }
    bvstk_cache_sink_enter(&s_i2c_cache.sink);
    for (unsigned r = first; r < first + count; ++r)
        (void)bvstk_i2c_cache_invalidate(&s_i2c_cache, d->device_id, (uint8_t)r);
    bvstk_cache_sink_leave(&s_i2c_cache.sink);
}
static void acquisition_task(void *argument)
{
    (void)argument;
    size_t next = 0;
    for (;;) {
        for (size_t n = 0; n < s_acquisition_count; ++n) {
            acquisition_device_t *d = &s_acquisition[next++ % s_acquisition_count];
            bvstk_acquisition_io_t io = {d, acquisition_ready, acquisition_cached,
                acquisition_read, acquisition_invalidate};
            if (bvstk_acquisition_step(&d->plan, &d->state, &io, bvstk_platform_now_ms())) break;
        }
        /* One chunk globally per turn; the PL slave and shell retain priority. */
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
static void start_acquisition_task(void)
{
    for (unsigned mdio = 0; mdio < 2; ++mdio) {
        size_t count = mdio ? (s_mdio_ready ? config_store_get_mdio_device_count() : 0) :
            (s_i2c_ready ? config_store_get_i2c_device_count() : 0);
        for (size_t id = 0; id < count; ++id) {
            const bvstk_acquisition_plan_t *plan = mdio ?
                &config_store_get_mdio_devices()[id].profile.acquisition :
                &config_store_get_i2c_devices()[id].profile.acquisition;
            if (!plan->enabled) continue;
            acquisition_device_t *d = &s_acquisition[s_acquisition_count++];
            d->plan = *plan; d->device_id = id; d->mdio = (uint8_t)mdio;
        }
    }
    if (s_acquisition_count && xTaskCreate(acquisition_task, "acquisition", 2048U,
        NULL, tskIDLE_PRIORITY + 1U, NULL) != pdPASS)
        xil_printf("[cache] acquisition task creation failed\r\n");
}
#endif

static void runtime_task(void *argument)
{
#if BVSTK_PL_HAS_I2C_CORE || BVSTK_PL_HAS_MDIO_CORE
    bvstk_event_sink_t events;
#endif
#if BVSTK_PL_HAS_SPI_CORE
    bvstk_spi_core_config_t spi_config;
#endif
#if BVSTK_PL_HAS_I2C_CORE
    const i2c_device_config_t *devices;
    size_t device_count;
#endif
#if BVSTK_PL_HAS_MDIO_CORE
    const mdio_phy_config_t *mdio_devices;
    size_t mdio_device_count;
#endif

    (void)argument;
#if BVSTK_PL_HAS_SPI_CORE
    memset(&spi_config, 0, sizeof(spi_config));
    spi_config.packets_mode = BVSTK_SPI_MODE_MULTI;
    spi_config.timeout_ticks = 1U;
    spi_config.p_clk_div = 512U;
    spi_config.read_enable = true;
    if (bvstk_freertos_mutex_init(&s_spi_mutex) == BVSTK_OK &&
        bvstk_spi_core_init(&s_spi_core,
                            &spi_config,
                            NULL,
                            &s_spi_mutex.public_mutex) == BVSTK_OK) {
        s_spi_ready = 1;
    } else {
        bvstk_freertos_mutex_destroy(&s_spi_mutex);
    }
#endif

#if BVSTK_PL_HAS_I2C_CORE || BVSTK_PL_HAS_MDIO_CORE
    if (config_store_wait_ready(35000U) != 0) {
        memset(&events, 0, sizeof(events));
        events.publish = bvstk_observer_publish;
#if BVSTK_PL_HAS_I2C_CORE
        devices = config_store_get_i2c_devices();
        device_count = config_store_get_i2c_device_count();
        if (initialize_i2c(devices, device_count, &events))
            start_i2c_health_task();
#endif

#if BVSTK_PL_HAS_MDIO_CORE
        mdio_devices = config_store_get_mdio_devices();
        mdio_device_count = config_store_get_mdio_device_count();
        if (bvstk_freertos_mutex_init(&s_mdio_service_mutex) == BVSTK_OK &&
            bvstk_freertos_mutex_init(&s_mdio_mutex) == BVSTK_OK &&
            bvstk_mdio_core_init(&s_mdio_core,
                                bvstk_mdio_freertos_clock(),
                                &s_mdio_mutex.public_mutex) == BVSTK_OK) {
            if (bvstk_mdio_service_init(&s_mdio_service,
                                       &s_mdio_core,
                                       NULL,
                                       mdio_devices,
                                       mdio_device_count,
                                       &events) == BVSTK_OK) {
                s_mdio_service.mutex = s_mdio_service_mutex.public_mutex;
                if (bvstk_cache_attach_mdio(&s_cache_mdio, &s_cache_registry, &s_mdio_service) != BVSTK_OK)
                    xil_printf("[cache] MDIO catalog registration failed\r\n");
                if (bvstk_mdio_freertos_start(&s_mdio_service) == BVSTK_OK) {
                    s_mdio_ready = 1;
                } else {
                    bvstk_mdio_service_shutdown(&s_mdio_service);
                    bvstk_mdio_core_shutdown(&s_mdio_core);
                    bvstk_freertos_mutex_destroy(&s_mdio_mutex);
                    bvstk_freertos_mutex_destroy(&s_mdio_service_mutex);
                }
            } else {
                bvstk_mdio_core_shutdown(&s_mdio_core);
                bvstk_freertos_mutex_destroy(&s_mdio_mutex);
                bvstk_freertos_mutex_destroy(&s_mdio_service_mutex);
            }
        } else {
            bvstk_freertos_mutex_destroy(&s_mdio_mutex);
            bvstk_freertos_mutex_destroy(&s_mdio_service_mutex);
        }
#endif
        start_acquisition_task();
    }
#endif
    s_runtime_task = NULL;
    vTaskDelete(NULL);
}
#endif

void bvstk_runtime_start(void)
{
    /* BSS is initially empty; install guards before any provider or reader. */
    s_cache_registry.guard.enter = cache_enter;
    s_cache_registry.guard.leave = cache_leave;
#if !BVSTK_PL_RUNTIME_ENABLED
    return;
#else
    if (s_runtime_task != NULL || s_i2c_ready != 0 || s_mdio_ready != 0 || s_spi_ready != 0) {
        return;
    }
    (void)xTaskCreate(runtime_task,
                      "bvstk_runtime",
                      1536U,
                      NULL,
                      tskIDLE_PRIORITY + 2U,
                      &s_runtime_task);
#endif
}

bvstk_i2c_master_service_t *bvstk_runtime_i2c_master_service(void)
{
    return s_i2c_ready != 0 ? &s_i2c_master_service : NULL;
}

bvstk_i2c_slave_service_t *bvstk_runtime_i2c_slave_service(void)
{
    return s_i2c_slave_ready != 0 ? &s_i2c_slave_service : NULL;
}

int bvstk_runtime_i2c_ready(void)
{
    return s_i2c_ready;
}

static int snapshot_i2c_device_config(size_t device_id, int include_cache_values,
                                       i2c_device_config_t *config)
{
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;
    const i2c_device_config_t *stored;

    if (config == NULL || s_i2c_ready == 0 ||
        bvstk_i2c_master_service_device_info(&s_i2c_master_service,
                                             device_id,
                                             &device) != BVSTK_OK ||
        bvstk_i2c_master_service_get_policy(&s_i2c_master_service,
                                            device_id,
                                            &policy) != BVSTK_OK) {
        return 0;
    }
    stored = config_store_find_i2c_device_by_name(device.name);
    if (stored == NULL) {
        return 0;
    }
    *config = *stored;
    config->addr_7b = device.addr_7b;
    config->reg_count = device.reg_count;
    config->max_value_code = device.max_value_code;
    config->presence_check_enabled = device.presence_check_enabled;
    config->presence_check_reg = device.presence_check_reg;
    config->endpoint = device.endpoint;
    config->policy = policy.mode;
    memcpy(config->whitelist,
           policy.whitelist,
           policy.whitelist_len * sizeof(policy.whitelist[0]));
    config->whitelist_len = policy.whitelist_len;
    memcpy(config->blacklist,
           policy.blacklist,
           policy.blacklist_len * sizeof(policy.blacklist[0]));
    config->blacklist_len = policy.blacklist_len;
    for (size_t reg = 0U; include_cache_values && reg < config->reg_count; ++reg) {
        if (bvstk_i2c_cache_is_valid(&s_i2c_cache, device_id, (uint8_t)reg)) {
            uint8_t value = 0U;
            if (bvstk_i2c_cache_read(&s_i2c_cache,
                                     device_id,
                                     (uint8_t)reg,
                                     &value) == BVSTK_OK) {
                update_setting(config, (uint8_t)reg, value);
            }
        }
    }
    return 1;
}

static int sync_i2c_device_config(size_t device_id, int include_cache_values)
{
    i2c_device_config_t config;
    return snapshot_i2c_device_config(device_id, include_cache_values, &config) &&
           config_store_set_i2c_device(&config);
}

int bvstk_runtime_i2c_policy_snapshot(size_t device_id, i2c_device_config_t *config)
{
    int result;
    taskENTER_CRITICAL();
    result = snapshot_i2c_device_config(device_id, 0, config);
    taskEXIT_CRITICAL();
    return result;
}

int bvstk_runtime_i2c_sync_device(size_t device_id)
{
    int result;
    /* Serialize this mirror with policy-only synchronization as well. */
    taskENTER_CRITICAL();
    result = sync_i2c_device_config(device_id, 1);
    taskEXIT_CRITICAL();
    return result;
}

int bvstk_runtime_i2c_sync_policy(size_t device_id)
{
    int result;
    /* Keep the policy snapshot and RAM store update together. Another task
     * must not publish a newer mirror between this read and its write. */
    taskENTER_CRITICAL();
    result = sync_i2c_device_config(device_id, 0);
    taskEXIT_CRITICAL();
    return result;
}

int bvstk_runtime_i2c_apply_config(const i2c_device_config_t *config)
{
    size_t device_id;

    if (config == NULL || s_i2c_ready == 0) {
        return 0;
    }
    if (bvstk_i2c_master_service_find_by_name(&s_i2c_master_service,
                                              config->name,
                                              &device_id) != BVSTK_OK &&
        bvstk_i2c_master_service_find_by_addr(&s_i2c_master_service,
                                              config->addr_7b,
                                              &device_id) != BVSTK_OK) {
        return 0;
    }
    return bvstk_i2c_master_service_set_config(&s_i2c_master_service,
                                               device_id,
                                               config) == BVSTK_OK;
}

bvstk_mdio_service_t *bvstk_runtime_mdio_service(void)
{
    return s_mdio_ready != 0 ? &s_mdio_service : NULL;
}

int bvstk_runtime_mdio_ready(void)
{
    return s_mdio_ready;
}


bvstk_spi_core_t *bvstk_runtime_spi_core(void)
{
    return s_spi_ready != 0 ? &s_spi_core : NULL;
}

int bvstk_runtime_spi_ready(void)
{
    return s_spi_ready;
}
