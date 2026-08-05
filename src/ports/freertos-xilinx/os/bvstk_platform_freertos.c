#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_platform.h"

#include <errno.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xil_io.h"

const char *bvstk_platform_name(void)
{
    return "freertos";
}

int bvstk_platform_init(void)
{
    return 0;
}

void bvstk_platform_shutdown(void)
{
}

void bvstk_platform_sleep_ms(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

int bvstk_mmio_region_open(bvstk_mmio_region_t *region,
                           uintptr_t physical_base,
                           size_t size)
{
    if (region == NULL || size == 0U) {
        errno = EINVAL;
        return -1;
    }

    region->physical_base = physical_base;
    region->size = size;
    region->mapped_base = (volatile uint8_t *)physical_base;
    return 0;
}

void bvstk_mmio_region_close(bvstk_mmio_region_t *region)
{
    if (region != NULL) {
        region->mapped_base = NULL;
        region->physical_base = 0U;
        region->size = 0U;
    }
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
    if (value == NULL || check_access(region, offset) != 0) {
        errno = EINVAL;
        return -1;
    }
    *value = Xil_In32((UINTPTR)(region->physical_base + offset));
    return 0;
}

int bvstk_mmio_write32(const bvstk_mmio_region_t *region,
                       size_t offset,
                       uint32_t value)
{
    if (check_access(region, offset) != 0) {
        return -1;
    }
    Xil_Out32((UINTPTR)(region->physical_base + offset), value);
    return 0;
}
