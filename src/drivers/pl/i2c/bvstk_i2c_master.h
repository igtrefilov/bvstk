#ifndef BVSTK_I2C_MASTER_H
#define BVSTK_I2C_MASTER_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

/* Maximum result that can fit in the master BRAM mailbox. */
#define BVSTK_I2C_MASTER_RESULT_MAX_BYTES \
    (BVSTK_I2C_BRAM_MASTER_WINDOW_SIZE - sizeof(uint32_t))

/* OS-independent master-side implementation of the PL I2C protocol. */
typedef struct {
    bvstk_mmio_region_t master;
    bvstk_mmio_region_t bram;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    uint8_t initialized;
} bvstk_i2c_master_hw_t;

bvstk_status_t bvstk_i2c_master_hw_init(bvstk_i2c_master_hw_t *hardware,
                                         const bvstk_clock_t *clock,
                                         const bvstk_mutex_t *mutex);
void bvstk_i2c_master_hw_shutdown(bvstk_i2c_master_hw_t *hardware);

/* Execute one write, read, or repeated-start write-then-read transaction. */
bvstk_status_t bvstk_i2c_master_hw_transfer(
    bvstk_i2c_master_hw_t *hardware,
    uint8_t addr_7b,
    const uint8_t *write_data,
    size_t write_size,
    uint8_t *read_data,
    size_t read_size,
    uint32_t timeout_ms);

bvstk_status_t bvstk_i2c_master_hw_read_reg(bvstk_i2c_master_hw_t *hardware,
                                            uint8_t addr_7b,
                                            uint8_t reg,
                                            uint8_t *value,
                                            uint32_t timeout_ms);
bvstk_status_t bvstk_i2c_master_hw_write_reg(bvstk_i2c_master_hw_t *hardware,
                                             uint8_t addr_7b,
                                             uint8_t reg,
                                             uint8_t value,
                                             uint32_t timeout_ms);

#endif /* BVSTK_I2C_MASTER_H */
