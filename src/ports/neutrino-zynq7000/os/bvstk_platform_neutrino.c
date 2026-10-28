#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_platform.h"
#include "shared/interfaces/bvstk_memory.h"
#include "shared/pl/access/bvstk_pl_service.h"

#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <time.h>

static int platform_ready;
static bvstk_pl_service_t memory_regions;
/* DCP2 and local commands share lazily created mappings and 64-bit accesses. */
static pthread_mutex_t memory_mutex = PTHREAD_MUTEX_INITIALIZER;

static bvstk_status_t memory_region(uint32_t address, size_t span, unsigned width,
    bvstk_pl_region_id_t *id, size_t *offset)
{
    for (size_t i = 0U; i < bvstk_pl_region_count(); ++i) {
        const bvstk_pl_region_desc_t *d = bvstk_pl_region_get((bvstk_pl_region_id_t)i);
        if (d && address >= d->physical_base &&
            (uint64_t)address + span <= (uint64_t)d->physical_base + d->size) {
            /* Control registers retain word accesses. BRAM supports all
             * widths with real byte/halfword stores, without read-modify-write. */
            if (d->kind != BVSTK_PL_REGION_BRAM && width != 32U)
                return BVSTK_ERR_UNSUPPORTED;
            if (id) *id = d->id;
            if (offset) *offset = (size_t)(address - d->physical_base);
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_RANGE;
}

static bvstk_status_t memory_check(void *context, uint32_t address, size_t span,
                                  unsigned width, int writing)
{
    (void)context; (void)writing;
    return memory_region(address, span, width, NULL, NULL);
}

static bvstk_status_t memory_mapping(void *context, uint32_t address, unsigned width,
    const bvstk_mmio_region_t **mapping, size_t *offset)
{
    bvstk_pl_region_id_t id;
    bvstk_status_t status = memory_region(address, width / 8U, width, &id, offset);
    return status == BVSTK_OK ? bvstk_pl_service_mapping(context, id, mapping) : status;
}

static bvstk_status_t memory_read_unlocked(void *context, uint32_t address, unsigned width, uint64_t *value)
{
    const bvstk_mmio_region_t *mapping;
    size_t offset;
    bvstk_status_t status = memory_mapping(context, address, width, &mapping, &offset);
    if (status != BVSTK_OK) return status;
    const volatile uint8_t *p = mapping->mapped_base + offset;
    switch (width) {
    case 8: *value = *p; break;
    case 16: *value = *(const volatile uint16_t *)p; break;
    case 32: *value = *(const volatile uint32_t *)p; break;
    case 64:
        *value = (uint64_t)*(const volatile uint32_t *)p << 32U;
        *value |= *(const volatile uint32_t *)(p + 4U);
        break;
    default: return BVSTK_ERR_UNSUPPORTED;
    }
    __sync_synchronize();
    return BVSTK_OK;
}

static bvstk_status_t memory_write_unlocked(void *context, uint32_t address, unsigned width, uint64_t value)
{
    const bvstk_mmio_region_t *mapping;
    size_t offset;
    bvstk_status_t status = memory_mapping(context, address, width, &mapping, &offset);
    if (status != BVSTK_OK) return status;
    volatile uint8_t *p = mapping->mapped_base + offset;
    switch (width) {
    case 8: *p = (uint8_t)value; break;
    case 16: *(volatile uint16_t *)p = (uint16_t)value; break;
    case 32: *(volatile uint32_t *)p = (uint32_t)value; break;
    case 64:
        *(volatile uint32_t *)p = (uint32_t)(value >> 32U);
        *(volatile uint32_t *)(p + 4U) = (uint32_t)value;
        break;
    default: return BVSTK_ERR_UNSUPPORTED;
    }
    __sync_synchronize();
    return BVSTK_OK;
}

static bvstk_status_t memory_read(void *context, uint32_t address, unsigned width, uint64_t *value)
{
    pthread_mutex_lock(&memory_mutex);
    bvstk_status_t status = memory_read_unlocked(context, address, width, value);
    pthread_mutex_unlock(&memory_mutex);
    return status;
}

static bvstk_status_t memory_write(void *context, uint32_t address, unsigned width, uint64_t value)
{
    pthread_mutex_lock(&memory_mutex);
    bvstk_status_t status = memory_write_unlocked(context, address, width, value);
    pthread_mutex_unlock(&memory_mutex);
    return status;
}

const bvstk_memory_t *bvstk_platform_memory(void)
{
    static const bvstk_memory_t memory = {
        &memory_regions, BVSTK_MEMORY_ALL_WIDTHS, memory_check, memory_read, memory_write
    };
    return &memory;
}

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
    pthread_mutex_lock(&memory_mutex);
    bvstk_pl_service_shutdown(&memory_regions);
    pthread_mutex_unlock(&memory_mutex);
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
