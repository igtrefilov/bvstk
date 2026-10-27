#include "apps/freertos/services/dcp2/dcp2_policy_store.h"
#include "FreeRTOS.h"
#include "task.h"

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"

bvstk_status_t bvstk_freertos_dcp2_policy_catalog(void *context, uint8_t service,
    size_t device_id, bvstk_dcp2_policy_catalog_t *catalog)
{
    bvstk_status_t status = BVSTK_ERR_NOT_FOUND;
    (void)context;
    taskENTER_CRITICAL();
    if (service == BVSTK_DCP2_SERVICE_I2C) {
        const i2c_device_config_t *devices = config_store_get_i2c_devices();
        if (devices && device_id < config_store_get_i2c_device_count())
            status = bvstk_dcp2_policy_catalog_from_profile(catalog,
                &devices[device_id].profile, devices[device_id].reg_count);
    } else if (service == BVSTK_DCP2_SERVICE_MDIO) {
        const mdio_phy_config_t *devices = config_store_get_mdio_devices();
        if (devices && device_id < config_store_get_mdio_device_count())
            status = bvstk_dcp2_policy_catalog_from_profile(catalog,
                &devices[device_id].profile, devices[device_id].reg_count);
    } else status = BVSTK_ERR_UNSUPPORTED;
    taskEXIT_CRITICAL();
    return status;
}

bvstk_status_t bvstk_freertos_dcp2_policy_changed(void *context, uint8_t service, size_t device_id)
{
    (void)context;
    return service != BVSTK_DCP2_SERVICE_I2C || bvstk_runtime_i2c_sync_policy(device_id) ?
        BVSTK_OK : BVSTK_ERR_INTERNAL;
}

bvstk_status_t bvstk_freertos_dcp2_policy_save(
    void *context, uint8_t service, size_t device_id)
{
    union {
        i2c_device_config_t i2c;
        mdio_phy_config_t mdio;
    } config;
    bvstk_status_t status;

    (void)context;
    if (!config_store_is_ready()) return BVSTK_ERR_BUSY;
    if (service == BVSTK_DCP2_SERVICE_I2C) {
        if (!bvstk_runtime_i2c_policy_snapshot(device_id, &config.i2c))
            return BVSTK_ERR_INTERNAL;
        /* The runtime critical section has ended before any filesystem I/O. */
        return config_store_save_i2c_device(&config.i2c) ? BVSTK_OK : BVSTK_ERR_IO;
    }
    if (service == BVSTK_DCP2_SERVICE_MDIO) {
        status = bvstk_mdio_service_get_config(
            bvstk_runtime_mdio_service(), device_id, &config.mdio);
        if (status != BVSTK_OK) return status;
        return config_store_save_mdio_device(&config.mdio) ? BVSTK_OK : BVSTK_ERR_IO;
    }
    return BVSTK_ERR_UNSUPPORTED;
}
