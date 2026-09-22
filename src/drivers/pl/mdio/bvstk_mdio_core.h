#ifndef BVSTK_MDIO_CORE_H
#define BVSTK_MDIO_CORE_H

#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

/* Serialized Clause 22 transactions and the fixed PL BRAM mailbox contract. */
typedef struct {
    bvstk_mmio_region_t master;
    bvstk_mmio_region_t slave;
    bvstk_mmio_region_t bram;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    uint8_t initialized;
} bvstk_mdio_core_t;

bvstk_status_t bvstk_mdio_core_init(bvstk_mdio_core_t *core,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex);
void bvstk_mdio_core_shutdown(bvstk_mdio_core_t *core);

bvstk_status_t bvstk_mdio_core_write(bvstk_mdio_core_t *core,
                                    uint8_t phy_addr,
                                    uint8_t reg,
                                    uint16_t value,
                                    uint32_t timeout_ms);
bvstk_status_t bvstk_mdio_core_read(bvstk_mdio_core_t *core,
                                   uint8_t phy_addr,
                                   uint8_t reg,
                                   uint16_t *value,
                                   uint32_t timeout_ms);

/* Upper 16 bits cannot be written by PL: marks a consumed slave mailbox. */
#define BVSTK_MDIO_MAILBOX_EMPTY UINT32_C(0x10000)
bvstk_status_t bvstk_mdio_core_prepare_slave(bvstk_mdio_core_t *core);
bvstk_status_t bvstk_mdio_core_take_write(bvstk_mdio_core_t *core,
    uint8_t phy, uint8_t reg, uint16_t *value);
bvstk_status_t bvstk_mdio_core_publish(bvstk_mdio_core_t *core,
    uint8_t phy, uint8_t reg, uint16_t value);
bvstk_status_t bvstk_mdio_core_ack_slave(bvstk_mdio_core_t *core);
#endif /* BVSTK_MDIO_CORE_H */
