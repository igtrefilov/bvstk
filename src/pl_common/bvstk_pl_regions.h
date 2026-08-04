#ifndef BVSTK_PL_REGIONS_H
#define BVSTK_PL_REGIONS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    BVSTK_PL_I2C_MASTER = 0,
    BVSTK_PL_I2C_SLAVE,
    BVSTK_PL_I2C_BRAM,
    BVSTK_PL_SMI_MASTER,
    BVSTK_PL_SMI_SLAVE,
    BVSTK_PL_SMI_BRAM,
    BVSTK_PL_SPI_MASTER,
    BVSTK_PL_SPI_BRAM,
    BVSTK_PL_REGION_COUNT
} bvstk_pl_region_id_t;

typedef enum {
    BVSTK_PL_REGION_CONTROL = 0,
    BVSTK_PL_REGION_BRAM = 1
} bvstk_pl_region_kind_t;

typedef struct {
    bvstk_pl_region_id_t id;
    const char *name;
    uintptr_t physical_base;
    size_t size;
    bvstk_pl_region_kind_t kind;
} bvstk_pl_region_desc_t;

const bvstk_pl_region_desc_t *bvstk_pl_region_get(bvstk_pl_region_id_t id);
const bvstk_pl_region_desc_t *bvstk_pl_region_find(const char *name);
size_t bvstk_pl_region_count(void);

#endif
