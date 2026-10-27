#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"
#include "shared/interfaces/bvstk_platform.h"
#include "shared/interfaces/bvstk_memory.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"

#include <errno.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xil_io.h"

static bvstk_status_t memory_check(void *context, uint32_t address, size_t span,
                                  unsigned width, int writing)
{
    (void)context; (void)width; (void)writing;
#if BVSTK_DCP2_ALLOW_ANY_MEM_ACCESS
    (void)address; (void)span;
    return BVSTK_OK;
#else
    static const struct { uint32_t base, size; } ranges[] = {
#if BVSTK_PL_HAS_I2C_CORE
        {BVSTK_I2C_MASTER_BASE, 0x1000U}, {BVSTK_I2C_SLAVE_BASE, 0x1000U},
        {BVSTK_I2C_BRAM_BASE, BVSTK_I2C_BRAM_SIZE},
#endif
#if BVSTK_PL_HAS_MDIO_CORE
        {BVSTK_MDIO_MASTER_BASE, 0x1000U}, {BVSTK_MDIO_SLAVE_BASE, 0x1000U},
        {BVSTK_MDIO_BRAM_BASE, BVSTK_MDIO_BRAM_SIZE},
#endif
#if BVSTK_PL_HAS_SPI_CORE
        {BVSTK_SPI_MASTER_BASE, 0x1000U}, {BVSTK_SPI_BRAM_BASE, BVSTK_SPI_BRAM_SIZE},
#endif
#if BVSTK_PL_HAS_SD_CONTROLLER
        {BVSTK_SD_CONTROLLER_BASE, BVSTK_SD_CONTROLLER_SIZE},
#endif
        {0U, 0U}
    };
    for (size_t i = 0U; i < sizeof(ranges) / sizeof(ranges[0]); ++i)
        if (address >= ranges[i].base &&
            (uint64_t)address + span <= (uint64_t)ranges[i].base + ranges[i].size)
            return BVSTK_OK;
    return BVSTK_ERR_RANGE;
#endif
}

static bvstk_status_t memory_read(void *context, uint32_t address, unsigned width, uint64_t *value)
{
    (void)context;
    switch (width) {
    case 8: *value = Xil_In8((UINTPTR)address); break;
    case 16: *value = Xil_In16((UINTPTR)address); break;
    case 32: *value = Xil_In32((UINTPTR)address); break;
    case 64:
        *value = (uint64_t)Xil_In32((UINTPTR)address) << 32U;
        *value |= Xil_In32((UINTPTR)(address + 4U));
        break;
    default: return BVSTK_ERR_UNSUPPORTED;
    }
    return BVSTK_OK;
}

static bvstk_status_t memory_write(void *context, uint32_t address, unsigned width, uint64_t value)
{
    (void)context;
    switch (width) {
    case 8: Xil_Out8((UINTPTR)address, (uint8_t)value); break;
    case 16: Xil_Out16((UINTPTR)address, (uint16_t)value); break;
    case 32: Xil_Out32((UINTPTR)address, (uint32_t)value); break;
    case 64:
        Xil_Out32((UINTPTR)address, (uint32_t)(value >> 32U));
        Xil_Out32((UINTPTR)(address + 4U), (uint32_t)value);
        break;
    default: return BVSTK_ERR_UNSUPPORTED;
    }
    return BVSTK_OK;
}

const bvstk_memory_t *bvstk_platform_memory(void)
{
    static const bvstk_memory_t memory = {
        NULL, BVSTK_MEMORY_ALL_WIDTHS, memory_check, memory_read, memory_write
    };
    return &memory;
}

const char *bvstk_platform_name(void)
{
    return "freertos";
}

uint64_t bvstk_platform_now_ms(void)
{
    return (uint64_t)xTaskGetTickCount() * (uint64_t)portTICK_PERIOD_MS;
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
