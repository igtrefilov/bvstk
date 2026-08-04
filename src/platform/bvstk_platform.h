#ifndef BVSTK_PLATFORM_H
#define BVSTK_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t physical_base;
    size_t size;
    volatile uint8_t *mapped_base;
} bvstk_mmio_region_t;

const char *bvstk_platform_name(void);
int bvstk_platform_init(void);
void bvstk_platform_shutdown(void);
void bvstk_platform_sleep_ms(uint32_t milliseconds);

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

#endif
