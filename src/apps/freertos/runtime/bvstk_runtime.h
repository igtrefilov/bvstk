#ifndef BVSTK_FREERTOS_RUNTIME_H
#define BVSTK_FREERTOS_RUNTIME_H

#include <stddef.h>

#include "drivers/pl/spi/bvstk_spi_core.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "services/smi/bvstk_smi_service.h"

/* Composition root for the portable services used by the FreeRTOS image. */
void bvstk_runtime_start(void);
bvstk_i2c_master_service_t *bvstk_runtime_i2c_master_service(void);
bvstk_i2c_slave_service_t *bvstk_runtime_i2c_slave_service(void);
int bvstk_runtime_i2c_ready(void);
int bvstk_runtime_i2c_sync_device(size_t device_id, int save_to_storage);
int bvstk_runtime_i2c_apply_config(const i2c_device_config_t *config);

bvstk_smi_service_t *bvstk_runtime_smi_service(void);
int bvstk_runtime_smi_ready(void);
int bvstk_runtime_smi_sync_device(size_t device_id, int save_to_storage);
int bvstk_runtime_smi_apply_config(const smi_phy_config_t *config);

bvstk_spi_core_t *bvstk_runtime_spi_core(void);
int bvstk_runtime_spi_ready(void);

#endif
