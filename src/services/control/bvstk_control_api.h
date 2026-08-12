#ifndef BVSTK_CONTROL_API_H
#define BVSTK_CONTROL_API_H

#include <stddef.h>
#include <stdint.h>

#include "drivers/pl/spi/bvstk_spi_core.h"
#include "services/i2c/bvstk_i2c_service.h"
#include "services/smi/bvstk_smi_service.h"
#include "shared/pl/access/bvstk_pl_service.h"

/*
 * Protocol adapters depend on this facade instead of importing individual
 * drivers and the configuration store.  The facade is intentionally a thin
 * composition object; policy and validation remain in the underlying service.
 */
typedef struct {
    bvstk_pl_service_t *pl;
    bvstk_i2c_service_t *i2c;
    bvstk_smi_service_t *smi;
    bvstk_spi_core_t *spi;
} bvstk_control_api_t;

bvstk_status_t bvstk_control_pl_read32(bvstk_control_api_t *api,
                                       bvstk_pl_region_id_t region,
                                       size_t offset,
                                       uint32_t *value);
bvstk_status_t bvstk_control_pl_write32(bvstk_control_api_t *api,
                                        bvstk_pl_region_id_t region,
                                        size_t offset,
                                        uint32_t value);
bvstk_status_t bvstk_control_i2c_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint8_t *value,
                                      uint32_t timeout_ms);
bvstk_status_t bvstk_control_i2c_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint8_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms);
bvstk_status_t bvstk_control_smi_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint16_t *value,
                                      uint32_t timeout_ms);
bvstk_status_t bvstk_control_smi_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint16_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms);
bvstk_status_t bvstk_control_spi_transfer(bvstk_control_api_t *api,
                                          const uint32_t *tx_words,
                                          size_t tx_count,
                                          uint32_t *rx_words,
                                          size_t rx_capacity,
                                          uint32_t timeout_ms);

#endif
