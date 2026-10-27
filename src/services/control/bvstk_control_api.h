#ifndef BVSTK_CONTROL_API_H
#define BVSTK_CONTROL_API_H

#include <stddef.h>
#include <stdint.h>

#include "drivers/pl/spi/bvstk_spi_core.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/mdio/bvstk_mdio_service.h"
#include "shared/pl/access/bvstk_pl_service.h"
#include "shared/interfaces/bvstk_memory.h"

/*
 * Protocol adapters depend on this facade instead of importing individual
 * drivers and the configuration store.  The facade is intentionally a thin
 * composition object; policy and validation remain in the underlying service.
 */
typedef struct {
    bvstk_pl_service_t *pl;
    bvstk_i2c_master_service_t *i2c;
    bvstk_mdio_service_t *mdio;
    bvstk_spi_core_t *spi;
    const bvstk_memory_t *memory;
} bvstk_control_api_t;

/* Validate the entire operation before its first hardware access. */
bvstk_status_t bvstk_control_mem_check(const bvstk_control_api_t *api,
    uint32_t address, unsigned width, size_t count, int autoincrement, int writing);
bvstk_status_t bvstk_control_mem_read(const bvstk_control_api_t *api,
    uint32_t address, unsigned width, uint64_t *value);
bvstk_status_t bvstk_control_mem_write(const bvstk_control_api_t *api,
    uint32_t address, unsigned width, uint64_t value);

/* RAM-only configuration operations. The owner holds its state guard across
 * the service snapshot and RAM-store update. File I/O is an explicit caller
 * operation, outside short cache/critical-section guards. stored supplies
 * profile metadata and configured seeds; capture_cache is used only by the
 * I2C console/device snapshot, never by a policy-only save. */
bvstk_status_t bvstk_control_i2c_snapshot(const bvstk_control_api_t *api,
    size_t device_id, const i2c_device_config_t *stored, int capture_cache,
    i2c_device_config_t *out);
bvstk_status_t bvstk_control_i2c_apply(bvstk_control_api_t *api,
    size_t device_id, const i2c_device_config_t *config);

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
bvstk_status_t bvstk_control_i2c_read_from(bvstk_control_api_t *, size_t, uint8_t,
    uint8_t *, bvstk_event_source_t, uint32_t);
bvstk_status_t bvstk_control_mdio_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint16_t *value,
                                      uint32_t timeout_ms);
bvstk_status_t bvstk_control_mdio_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint16_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms);
bvstk_status_t bvstk_control_mdio_read_from(bvstk_control_api_t *, size_t, uint8_t,
    uint16_t *, bvstk_event_source_t, uint32_t);
bvstk_status_t bvstk_control_spi_transfer(bvstk_control_api_t *api,
                                          const uint32_t *tx_words,
                                          size_t tx_count,
                                          uint32_t *rx_words,
                                          size_t rx_capacity,
                                          uint32_t timeout_ms);

#endif
