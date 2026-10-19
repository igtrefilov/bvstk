#include "apps/freertos/services/dcp2/dcp2_policy_store.h"

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"

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
