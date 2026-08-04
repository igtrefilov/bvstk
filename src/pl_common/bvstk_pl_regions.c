#include "bvstk_pl_regions.h"

#include <string.h>

#include "bvstk_hw_config.h"

static const bvstk_pl_region_desc_t regions[BVSTK_PL_REGION_COUNT] = {
    {BVSTK_PL_I2C_MASTER, "i2c-master", BVSTK_I2C_MASTER_BASE, BVSTK_I2C_MASTER_SIZE, BVSTK_PL_REGION_CONTROL},
    {BVSTK_PL_I2C_SLAVE,  "i2c-slave",  BVSTK_I2C_SLAVE_BASE,  BVSTK_I2C_SLAVE_SIZE,  BVSTK_PL_REGION_CONTROL},
    {BVSTK_PL_I2C_BRAM,   "i2c-bram",   BVSTK_I2C_BRAM_BASE,   BVSTK_I2C_BRAM_SIZE,   BVSTK_PL_REGION_BRAM},
    {BVSTK_PL_SMI_MASTER, "smi-master", BVSTK_SMI_MASTER_BASE, BVSTK_SMI_MASTER_SIZE, BVSTK_PL_REGION_CONTROL},
    {BVSTK_PL_SMI_SLAVE,  "smi-slave",  BVSTK_SMI_SLAVE_BASE,  BVSTK_SMI_SLAVE_SIZE,  BVSTK_PL_REGION_CONTROL},
    {BVSTK_PL_SMI_BRAM,   "smi-bram",   BVSTK_SMI_BRAM_BASE,   BVSTK_SMI_BRAM_SIZE,   BVSTK_PL_REGION_BRAM},
    {BVSTK_PL_SPI_MASTER, "spi-master", BVSTK_SPI_MASTER_BASE, BVSTK_SPI_MASTER_SIZE, BVSTK_PL_REGION_CONTROL},
    {BVSTK_PL_SPI_BRAM,   "spi-bram",   BVSTK_SPI_BRAM_BASE,   BVSTK_SPI_BRAM_SIZE,   BVSTK_PL_REGION_BRAM},
};

const bvstk_pl_region_desc_t *bvstk_pl_region_get(bvstk_pl_region_id_t id)
{
    if ((unsigned int)id >= (unsigned int)BVSTK_PL_REGION_COUNT) {
        return NULL;
    }
    return &regions[id];
}

const bvstk_pl_region_desc_t *bvstk_pl_region_find(const char *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < BVSTK_PL_REGION_COUNT; ++i) {
        if (strcmp(regions[i].name, name) == 0) {
            return &regions[i];
        }
    }
    return NULL;
}

size_t bvstk_pl_region_count(void)
{
    return BVSTK_PL_REGION_COUNT;
}
