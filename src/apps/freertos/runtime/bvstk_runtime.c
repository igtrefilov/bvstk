#include "apps/freertos/runtime/bvstk_runtime.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/services/dcp2/dcp2_notify.h"
#include "drivers/pl/i2c/bvstk_i2c_master.h"
#include "drivers/pl/i2c/bvstk_i2c_slave.h"
#include "drivers/pl/smi/bvstk_smi_core.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"
#include "ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.h"

static bvstk_i2c_master_hw_t s_i2c_master_hw;
static bvstk_i2c_slave_hw_t s_i2c_slave_hw;
static bvstk_i2c_devices_t s_i2c_devices;
static bvstk_i2c_cache_t s_i2c_cache;
static bvstk_i2c_policy_t s_i2c_policy;
static bvstk_i2c_master_service_t s_i2c_master_service;
static bvstk_i2c_slave_service_t s_i2c_slave_service;
static bvstk_i2c_slave_freertos_t s_i2c_slave_adapter;
static bvstk_freertos_mutex_t s_i2c_mutex;
static bvstk_smi_core_t s_smi_core;
static bvstk_smi_service_t s_smi_service;
static bvstk_freertos_mutex_t s_smi_mutex;
static bvstk_spi_core_t s_spi_core;
static bvstk_freertos_mutex_t s_spi_mutex;
static volatile int s_i2c_ready;
static volatile int s_i2c_slave_ready;
static volatile int s_smi_ready;
static volatile int s_spi_ready;
#if BVSTK_PL_RUNTIME_ENABLED
static TaskHandle_t s_runtime_task;
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
static void publish_event(void *context, const bvstk_event_t *event)
{
    (void)context;
    if (event == NULL) {
        return;
    }
    dcp2_notify_publish_simple(event->type,
                               event->status,
                               (dcp2_notify_source_t)event->source,
                               (dcp2_notify_bus_t)event->bus,
                               (dcp2_notify_op_t)event->operation,
                               event->arg0,
                               event->arg1,
                               event->arg2);
}

#if BVSTK_PL_HAS_I2C_CORE
static int initialize_i2c(const i2c_device_config_t *configs,
                          size_t config_count,
                          const bvstk_event_sink_t *events)
{
    bvstk_i2c_device_t target_device;
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
            (void)bvstk_i2c_cache_write(&s_i2c_cache,
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

    s_i2c_ready = 1;
    if (config_count != 0U &&
        bvstk_i2c_master_service_device_info(&s_i2c_master_service,
                                             0U,
                                             &target_device) == BVSTK_OK &&
        bvstk_i2c_slave_hw_init(&s_i2c_slave_hw) == BVSTK_OK &&
        bvstk_i2c_slave_hw_set_address(&s_i2c_slave_hw,
                                       target_device.addr_7b) == BVSTK_OK &&
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

static void runtime_task(void *argument)
{
#if BVSTK_PL_HAS_I2C_CORE || BVSTK_PL_HAS_SMI_CORE
    bvstk_event_sink_t events;
#endif
#if BVSTK_PL_HAS_SPI_CORE
    bvstk_spi_core_config_t spi_config;
#endif
#if BVSTK_PL_HAS_I2C_CORE
    const i2c_device_config_t *devices;
    size_t device_count;
#endif
#if BVSTK_PL_HAS_SMI_CORE
    const smi_phy_config_t *smi_devices;
    size_t smi_device_count;
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

#if BVSTK_PL_HAS_I2C_CORE || BVSTK_PL_HAS_SMI_CORE
    if (config_store_wait_ready(10000U) != 0) {
#if BVSTK_PL_HAS_I2C_CORE
        devices = config_store_get_i2c_devices();
        device_count = config_store_get_i2c_device_count();
        memset(&events, 0, sizeof(events));
        events.publish = publish_event;
        (void)initialize_i2c(devices, device_count, &events);
#endif

#if BVSTK_PL_HAS_SMI_CORE
        smi_devices = config_store_get_smi_devices();
        smi_device_count = config_store_get_smi_device_count();
        if (bvstk_freertos_mutex_init(&s_smi_mutex) == BVSTK_OK &&
            bvstk_smi_core_init(&s_smi_core,
                                NULL,
                                &s_smi_mutex.public_mutex) == BVSTK_OK) {
            if (bvstk_smi_service_init(&s_smi_service,
                                       &s_smi_core,
                                       NULL,
                                       smi_devices,
                                       smi_device_count,
                                       &events) == BVSTK_OK) {
                s_smi_ready = 1;
            } else {
                bvstk_smi_core_shutdown(&s_smi_core);
                bvstk_freertos_mutex_destroy(&s_smi_mutex);
            }
        } else {
            bvstk_freertos_mutex_destroy(&s_smi_mutex);
        }
#endif
    }
#endif
    s_runtime_task = NULL;
    vTaskDelete(NULL);
}
#endif

void bvstk_runtime_start(void)
{
#if !BVSTK_PL_RUNTIME_ENABLED
    return;
#else
    if (s_runtime_task != NULL || s_i2c_ready != 0) {
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

int bvstk_runtime_i2c_sync_device(size_t device_id, int save_to_storage)
{
    bvstk_i2c_device_t device;
    bvstk_i2c_policy_entry_t policy;
    const i2c_device_config_t *stored;
    i2c_device_config_t config;

    if (s_i2c_ready == 0 ||
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
    config = *stored;
    config.addr_7b = device.addr_7b;
    config.reg_count = device.reg_count;
    config.max_value_code = device.max_value_code;
    config.policy = policy.mode;
    memcpy(config.whitelist,
           policy.whitelist,
           policy.whitelist_len * sizeof(policy.whitelist[0]));
    config.whitelist_len = policy.whitelist_len;
    memcpy(config.blacklist,
           policy.blacklist,
           policy.blacklist_len * sizeof(policy.blacklist[0]));
    config.blacklist_len = policy.blacklist_len;
    for (size_t reg = 0U; reg < config.reg_count; ++reg) {
        if (bvstk_i2c_cache_is_valid(&s_i2c_cache, device_id, (uint8_t)reg)) {
            uint8_t value = 0U;
            if (bvstk_i2c_cache_read(&s_i2c_cache,
                                     device_id,
                                     (uint8_t)reg,
                                     &value) == BVSTK_OK) {
                update_setting(&config, (uint8_t)reg, value);
            }
        }
    }
    if (config_store_set_i2c_device(&config) == 0) {
        return 0;
    }
    return save_to_storage == 0 || config_store_save_i2c_device(&config) != 0;
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

bvstk_smi_service_t *bvstk_runtime_smi_service(void)
{
    return s_smi_ready != 0 ? &s_smi_service : NULL;
}

int bvstk_runtime_smi_ready(void)
{
    return s_smi_ready;
}

int bvstk_runtime_smi_sync_device(size_t device_id, int save_to_storage)
{
    smi_phy_config_t config;

    if (s_smi_ready == 0 ||
        bvstk_smi_service_get_config(&s_smi_service, device_id, &config) != BVSTK_OK ||
        config_store_set_smi_device(&config) == 0) {
        return 0;
    }
    return save_to_storage == 0 || config_store_save_smi_device(&config) != 0;
}

int bvstk_runtime_smi_apply_config(const smi_phy_config_t *config)
{
    size_t device_id;

    if (config == NULL || s_smi_ready == 0) {
        return 0;
    }
    if (bvstk_smi_service_find_by_name(&s_smi_service,
                                       config->name,
                                       &device_id) != BVSTK_OK &&
        bvstk_smi_service_find_by_phy(&s_smi_service,
                                      config->phy_addr,
                                      &device_id) != BVSTK_OK) {
        return 0;
    }
    return bvstk_smi_service_set_config(&s_smi_service,
                                        device_id,
                                        config) == BVSTK_OK;
}

bvstk_spi_core_t *bvstk_runtime_spi_core(void)
{
    return s_spi_ready != 0 ? &s_spi_core : NULL;
}

int bvstk_runtime_spi_ready(void)
{
    return s_spi_ready;
}
