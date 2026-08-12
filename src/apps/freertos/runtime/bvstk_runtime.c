#include "apps/freertos/runtime/bvstk_runtime.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/drivers/pl/i2c/bvstk_i2c.h"
#include "apps/freertos/services/dcp2/dcp2_notify.h"
#include "drivers/pl/smi/bvstk_smi_core.h"
#include "hardware/pl/spi/bvstk_spi_regs.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"

static bvstk_i2c_service_t s_i2c_service;
static bvstk_smi_core_t s_smi_core;
static bvstk_smi_service_t s_smi_service;
static bvstk_freertos_mutex_t s_smi_mutex;
static bvstk_spi_core_t s_spi_core;
static bvstk_freertos_mutex_t s_spi_mutex;
static volatile int s_i2c_ready;
static volatile int s_smi_ready;
static volatile int s_spi_ready;
static TaskHandle_t s_runtime_task;

static bvstk_status_t legacy_read(void *context,
                                  uint8_t addr_7b,
                                  uint8_t reg,
                                  uint8_t *value,
                                  uint32_t timeout_ms)
{
    size_t device_id;

    (void)context;
    (void)timeout_ms;
    if (value == NULL ||
        !i2cdev_find_device_index_by_addr(addr_7b, &device_id) ||
        !i2cdev_read_reg_dev(device_id, reg, value)) {
        return BVSTK_ERR_IO;
    }
    return BVSTK_OK;
}

static bvstk_status_t legacy_write(void *context,
                                   uint8_t addr_7b,
                                   uint8_t reg,
                                   uint8_t value,
                                   bvstk_event_source_t source,
                                   uint32_t timeout_ms)
{
    size_t device_id;

    (void)context;
    (void)timeout_ms;
    if (!i2cdev_find_device_index_by_addr(addr_7b, &device_id) ||
        !i2cdev_write_reg_dev_raw(device_id, reg, value)) {
        return BVSTK_ERR_IO;
    }
    (void)source;
    return BVSTK_OK;
}

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

static void runtime_task(void *argument)
{
    bvstk_i2c_bus_ops_t bus;
    bvstk_event_sink_t events;
    bvstk_spi_core_config_t spi_config;
    const i2c_device_config_t *devices;
    const smi_phy_config_t *smi_devices;
    size_t device_count;
    size_t smi_device_count;

    (void)argument;
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

    if (config_store_wait_ready(10000U) != 0) {
        devices = config_store_get_i2c_devices();
        device_count = config_store_get_i2c_device_count();
        memset(&bus, 0, sizeof(bus));
        bus.read_reg = legacy_read;
        bus.write_reg = legacy_write;
        memset(&events, 0, sizeof(events));
        events.publish = publish_event;
        if (bvstk_i2c_service_init(&s_i2c_service,
                                   NULL,
                                   &bus,
                                   devices,
                                   device_count,
                                   &events) == BVSTK_OK) {
            s_i2c_ready = 1;
        }

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
    }
    s_runtime_task = NULL;
    vTaskDelete(NULL);
}

void bvstk_runtime_start(void)
{
    if (s_runtime_task != NULL || s_i2c_ready != 0) {
        return;
    }
    (void)xTaskCreate(runtime_task,
                      "bvstk_runtime",
                      1536U,
                      NULL,
                      tskIDLE_PRIORITY + 2U,
                      &s_runtime_task);
}

bvstk_i2c_service_t *bvstk_runtime_i2c_service(void)
{
    return s_i2c_ready != 0 ? &s_i2c_service : NULL;
}

int bvstk_runtime_i2c_ready(void)
{
    return s_i2c_ready;
}

int bvstk_runtime_i2c_sync_device(size_t device_id, int save_to_storage)
{
    i2c_device_config_t config;

    if (s_i2c_ready == 0 ||
        bvstk_i2c_service_get_config(&s_i2c_service, device_id, &config) != BVSTK_OK ||
        config_store_set_i2c_device(&config) == 0) {
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
    if (bvstk_i2c_service_find_by_name(&s_i2c_service,
                                       config->name,
                                       &device_id) != BVSTK_OK &&
        bvstk_i2c_service_find_by_addr(&s_i2c_service,
                                       config->addr_7b,
                                       &device_id) != BVSTK_OK) {
        return 0;
    }
    return bvstk_i2c_service_set_config(&s_i2c_service,
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
