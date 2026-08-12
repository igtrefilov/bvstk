#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_platform.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <time.h>

static int platform_ready;

const char *bvstk_platform_name(void)
{
    return "neutrino";
}

uint64_t bvstk_platform_now_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000) +
           (uint64_t)now.tv_nsec / UINT64_C(1000000);
}

int bvstk_platform_init(void)
{
    if (platform_ready) {
        return 0;
    }
    if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1) {
        return -1;
    }
    platform_ready = 1;
    return 0;
}

void bvstk_platform_shutdown(void)
{
    platform_ready = 0;
}

void bvstk_platform_sleep_ms(uint32_t milliseconds)
{
    struct timespec delay;

    delay.tv_sec = (time_t)(milliseconds / 1000U);
    delay.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
    }
}

int bvstk_mmio_region_open(bvstk_mmio_region_t *region,
                           uintptr_t physical_base,
                           size_t size)
{
    void *mapped;

    if (region == NULL || size == 0U) {
        errno = EINVAL;
        return -1;
    }
    if (bvstk_platform_init() != 0) {
        return -1;
    }

    mapped = mmap_device_memory(NULL,
                                size,
                                PROT_READ | PROT_WRITE | PROT_NOCACHE,
                                0,
                                (uint64_t)physical_base);
    if (mapped == MAP_FAILED) {
        return -1;
    }

    region->physical_base = physical_base;
    region->size = size;
    region->mapped_base = (volatile uint8_t *)mapped;
    return 0;
}

void bvstk_mmio_region_close(bvstk_mmio_region_t *region)
{
    if (region == NULL) {
        return;
    }
    if (region->mapped_base != NULL && region->size != 0U) {
        (void)munmap((void *)region->mapped_base, region->size);
    }
    region->mapped_base = NULL;
    region->physical_base = 0U;
    region->size = 0U;
}

static int check_access(const bvstk_mmio_region_t *region, size_t offset)
{
    if (region == NULL || region->mapped_base == NULL ||
        region->size < sizeof(uint32_t) ||
        (offset & 3U) != 0U || offset > region->size - sizeof(uint32_t)) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int bvstk_mmio_read32(const bvstk_mmio_region_t *region,
                      size_t offset,
                      uint32_t *value)
{
    const volatile uint32_t *address;

    if (value == NULL || check_access(region, offset) != 0) {
        errno = EINVAL;
        return -1;
    }
    address = (const volatile uint32_t *)(region->mapped_base + offset);
    *value = *address;
    __sync_synchronize();
    return 0;
}

int bvstk_mmio_write32(const bvstk_mmio_region_t *region,
                       size_t offset,
                       uint32_t value)
{
    volatile uint32_t *address;

    if (check_access(region, offset) != 0) {
        return -1;
    }
    address = (volatile uint32_t *)(region->mapped_base + offset);
    *address = value;
    __sync_synchronize();
    return 0;
}
