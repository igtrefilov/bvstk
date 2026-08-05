#ifndef BVSTK_PL_SERVICE_H
#define BVSTK_PL_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/boards/ax7020/bvstk_pl_regions.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_mmio.h"

typedef struct {
    bvstk_mmio_region_t mappings[BVSTK_PL_REGION_COUNT];
    unsigned char mapped[BVSTK_PL_REGION_COUNT];
} bvstk_pl_service_t;

void bvstk_pl_service_init(bvstk_pl_service_t *service);
void bvstk_pl_service_shutdown(bvstk_pl_service_t *service);
bvstk_status_t bvstk_pl_service_read32(bvstk_pl_service_t *service,
                                       bvstk_pl_region_id_t region,
                                       size_t offset,
                                       uint32_t *value);
bvstk_status_t bvstk_pl_service_write32(bvstk_pl_service_t *service,
                                        bvstk_pl_region_id_t region,
                                        size_t offset,
                                        uint32_t value);

#endif
