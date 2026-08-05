#ifndef BVSTK_SHARED_MMIO_H
#define BVSTK_SHARED_MMIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t physical_base;
    size_t size;
    volatile uint8_t *mapped_base;
} bvstk_mmio_region_t;

int bvstk_mmio_region_open(bvstk_mmio_region_t *region,
                           uintptr_t physical_base,
                           size_t size);
void bvstk_mmio_region_close(bvstk_mmio_region_t *region);
int bvstk_mmio_read32(const bvstk_mmio_region_t *region,
                      size_t offset,
                      uint32_t *value);
int bvstk_mmio_write32(const bvstk_mmio_region_t *region,
                       size_t offset,
                       uint32_t value);

#endif /* BVSTK_SHARED_MMIO_H */
