#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/boards/ax7020/bvstk_pl_regions.h"
#include "shared/base/bvstk_parse.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/pl/access/bvstk_pl_service.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                \
                    __FILE__, __LINE__, #condition);                         \
            return 1;                                                        \
        }                                                                    \
    } while (0)

int bvstk_mmio_region_open(bvstk_mmio_region_t *region,
                           uintptr_t physical_base,
                           size_t size)
{
    if (region == NULL || size == 0U) {
        errno = EINVAL;
        return -1;
    }
    region->mapped_base = calloc(1U, size);
    if (region->mapped_base == NULL) {
        errno = ENOMEM;
        return -1;
    }
    region->physical_base = physical_base;
    region->size = size;
    return 0;
}

void bvstk_mmio_region_close(bvstk_mmio_region_t *region)
{
    if (region == NULL) {
        return;
    }
    free((void *)region->mapped_base);
    memset(region, 0, sizeof(*region));
}

static int access_valid(const bvstk_mmio_region_t *region, size_t offset)
{
    return region != NULL && region->mapped_base != NULL &&
           (offset & 3U) == 0U && region->size >= sizeof(uint32_t) &&
           offset <= region->size - sizeof(uint32_t);
}

int bvstk_mmio_read32(const bvstk_mmio_region_t *region,
                      size_t offset,
                      uint32_t *value)
{
    if (value == NULL || !access_valid(region, offset)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(value, (const void *)(region->mapped_base + offset), sizeof(*value));
    return 0;
}

int bvstk_mmio_write32(const bvstk_mmio_region_t *region,
                       size_t offset,
                       uint32_t value)
{
    if (!access_valid(region, offset)) {
        errno = EINVAL;
        return -1;
    }
    memcpy((void *)(region->mapped_base + offset), &value, sizeof(value));
    return 0;
}

int main(void)
{
    const bvstk_pl_region_desc_t *spi;
    bvstk_pl_service_t service;
    bvstk_status_t status;
    uint32_t value = 0U;
    bool ok = false;

    CHECK(strcmp(bvstk_status_string(BVSTK_ERR_TIMEOUT), "timeout") == 0);
    CHECK(parse_num("42", &ok) == 42UL && ok);
    CHECK(parse_num("0x2a", &ok) == 42UL && ok);
    (void)parse_num("42x", &ok);
    CHECK(!ok);

    CHECK(bvstk_pl_region_count() == (size_t)BVSTK_PL_REGION_COUNT);
    spi = bvstk_pl_region_find("spi-master");
    CHECK(spi != NULL && spi->id == BVSTK_PL_SPI_MASTER);

    bvstk_pl_service_init(&service);
    status = bvstk_pl_service_write32(&service, BVSTK_PL_SPI_MASTER,
                                      0U, UINT32_C(0x12345678));
    CHECK(status == BVSTK_OK);
    status = bvstk_pl_service_read32(&service, BVSTK_PL_SPI_MASTER, 0U, &value);
    CHECK(status == BVSTK_OK && value == UINT32_C(0x12345678));
    status = bvstk_pl_service_read32(&service, BVSTK_PL_SPI_MASTER,
                                     spi->size, &value);
    CHECK(status == BVSTK_ERR_RANGE);
    bvstk_pl_service_shutdown(&service);

    puts("shared host tests passed");
    return 0;
}
