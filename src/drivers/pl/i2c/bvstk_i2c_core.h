#ifndef BVSTK_I2C_CORE_H
#define BVSTK_I2C_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

/*
 * Portable master-side implementation of the BVSTK I2C PL protocol.
 *
 * The object owns only MMIO mappings.  Interrupt delivery and scheduling are
 * optional ports: the current transaction path is deliberately synchronous
 * and polls the same completion bit used by the FreeRTOS implementation.
 * This gives both targets one hardware algorithm while allowing a later IRQ
 * backend without changing the service API.
 */
typedef struct {
    bvstk_mmio_region_t master;
    bvstk_mmio_region_t bram;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    uint8_t initialized;
} bvstk_i2c_core_t;

bvstk_status_t bvstk_i2c_core_init(bvstk_i2c_core_t *core,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex);
void bvstk_i2c_core_shutdown(bvstk_i2c_core_t *core);

bvstk_status_t bvstk_i2c_core_write(bvstk_i2c_core_t *core,
                                    uint8_t addr_7b,
                                    const uint8_t *payload,
                                    size_t payload_size,
                                    uint32_t timeout_ms);
bvstk_status_t bvstk_i2c_core_read_reg(bvstk_i2c_core_t *core,
                                       uint8_t addr_7b,
                                       uint8_t reg,
                                       uint8_t *value,
                                       uint32_t timeout_ms);
bvstk_status_t bvstk_i2c_core_write_reg(bvstk_i2c_core_t *core,
                                        uint8_t addr_7b,
                                        uint8_t reg,
                                        uint8_t value,
                                        uint32_t timeout_ms);

#endif
