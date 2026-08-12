#ifndef BVSTK_SMI_CORE_H
#define BVSTK_SMI_CORE_H

#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_sync.h"

/* Portable MDIO/SMI master path.  IRQ delivery remains a target concern. */
typedef struct {
    bvstk_mmio_region_t master;
    bvstk_mmio_region_t slave;
    bvstk_mmio_region_t bram;
    bvstk_clock_t clock;
    bvstk_mutex_t mutex;
    uint8_t initialized;
} bvstk_smi_core_t;

bvstk_status_t bvstk_smi_core_init(bvstk_smi_core_t *core,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex);
void bvstk_smi_core_shutdown(bvstk_smi_core_t *core);

bvstk_status_t bvstk_smi_core_write(bvstk_smi_core_t *core,
                                    uint8_t phy_addr,
                                    uint8_t reg,
                                    uint16_t value,
                                    uint32_t timeout_ms);
bvstk_status_t bvstk_smi_core_read(bvstk_smi_core_t *core,
                                   uint8_t phy_addr,
                                   uint8_t reg,
                                   uint16_t *value,
                                   uint32_t timeout_ms);

#endif /* BVSTK_SMI_CORE_H */
