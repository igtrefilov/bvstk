#include "drivers/pl/smi/bvstk_smi_core.h"

#include <string.h>

#include "hardware/pl/smi/bvstk_smi_regs.h"

enum {
    BVSTK_SMI_DEFAULT_TIMEOUT_MS = 100,
    BVSTK_SMI_POLL_DELAY_MS = 1,
    BVSTK_SMI_READ_READY_BIT = 0x04
};

static uint32_t effective_timeout(uint32_t timeout_ms)
{
    return timeout_ms == 0U ? BVSTK_SMI_DEFAULT_TIMEOUT_MS : timeout_ms;
}

static bvstk_status_t read32(const bvstk_mmio_region_t *region,
                             size_t offset,
                             uint32_t *value)
{
    return bvstk_mmio_read32(region, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static bvstk_status_t write32(const bvstk_mmio_region_t *region,
                              size_t offset,
                              uint32_t value)
{
    return bvstk_mmio_write32(region, offset, value) == 0
               ? BVSTK_OK
               : BVSTK_ERR_IO;
}

static uint32_t make_frame(uint8_t phy_addr,
                           uint8_t reg,
                           uint16_t value,
                           uint8_t write)
{
    return (uint32_t)value |
           ((uint32_t)(reg & UINT8_C(0x1F)) << 16) |
           ((uint32_t)(phy_addr & UINT8_C(0x1F)) << 21) |
           ((uint32_t)(write != 0U) << 26);
}

bvstk_status_t bvstk_smi_core_init(bvstk_smi_core_t *core,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex)
{
    if (core == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(core, 0, sizeof(*core));
    if (clock != NULL) {
        core->clock = *clock;
    }
    if (mutex != NULL) {
        core->mutex = *mutex;
    }
    if (bvstk_mmio_region_open(&core->master,
                               BVSTK_SMI_MASTER_BASE,
                               BVSTK_SMI_MASTER_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&core->slave,
                               BVSTK_SMI_SLAVE_BASE,
                               BVSTK_SMI_SLAVE_SIZE) != 0) {
        bvstk_mmio_region_close(&core->master);
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&core->bram,
                               BVSTK_SMI_BRAM_BASE,
                               BVSTK_SMI_BRAM_SIZE) != 0) {
        bvstk_mmio_region_close(&core->slave);
        bvstk_mmio_region_close(&core->master);
        return BVSTK_ERR_IO;
    }
    core->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_smi_core_shutdown(bvstk_smi_core_t *core)
{
    if (core == NULL) {
        return;
    }
    if (core->initialized != 0U) {
        bvstk_mmio_region_close(&core->bram);
        bvstk_mmio_region_close(&core->slave);
        bvstk_mmio_region_close(&core->master);
    }
    memset(core, 0, sizeof(*core));
}

bvstk_status_t bvstk_smi_core_write(bvstk_smi_core_t *core,
                                    uint8_t phy_addr,
                                    uint8_t reg,
                                    uint16_t value,
                                    uint32_t timeout_ms)
{
    bvstk_status_t status;

    if (core == NULL || core->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (phy_addr > UINT8_C(0x1F) || reg > UINT8_C(0x1F)) {
        return BVSTK_ERR_RANGE;
    }
    status = bvstk_mutex_lock(&core->mutex, timeout_ms);
    if (status != BVSTK_OK) {
        return status;
    }
    status = write32(&core->master,
                     BVSTK_SMI_MSTR_TX_FIFO_OFFSET,
                     make_frame(phy_addr, reg, value, 1U));
    bvstk_mutex_unlock(&core->mutex);
    return status;
}

bvstk_status_t bvstk_smi_core_read(bvstk_smi_core_t *core,
                                   uint8_t phy_addr,
                                   uint8_t reg,
                                   uint16_t *value,
                                   uint32_t timeout_ms)
{
    bvstk_status_t status;
    uint64_t start;
    uint32_t csr = 0U;
    uint32_t result = 0U;

    if (core == NULL || core->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (value == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (phy_addr > UINT8_C(0x1F) || reg > UINT8_C(0x1F)) {
        return BVSTK_ERR_RANGE;
    }
    status = bvstk_mutex_lock(&core->mutex, timeout_ms);
    if (status != BVSTK_OK) {
        return status;
    }

    status = write32(&core->master,
                     BVSTK_SMI_MSTR_TX_FIFO_OFFSET,
                     make_frame(phy_addr, reg, 0U, 0U));
    if (status != BVSTK_OK) {
        bvstk_mutex_unlock(&core->mutex);
        return status;
    }

    start = bvstk_clock_now_ms(&core->clock);
    timeout_ms = effective_timeout(timeout_ms);
    for (;;) {
        status = read32(&core->slave, BVSTK_SMI_SLV_CSR_OFFSET, &csr);
        if (status != BVSTK_OK) {
            break;
        }
        if ((csr & BVSTK_SMI_READ_READY_BIT) != 0U) {
            status = read32(&core->slave,
                            BVSTK_SMI_SLV_S2H_OFFSET,
                            &result);
            if (status == BVSTK_OK) {
                *value = (uint16_t)(result & UINT32_C(0xFFFF));
                /* Match the PL's level-like IRQ acknowledge sequence. */
                status = write32(&core->slave,
                                 BVSTK_SMI_SLV_IRQ_OFFSET,
                                 UINT32_C(1));
            }
            break;
        }
        if (bvstk_clock_now_ms(&core->clock) - start >= timeout_ms) {
            status = BVSTK_ERR_TIMEOUT;
            break;
        }
        bvstk_clock_sleep_ms(&core->clock, BVSTK_SMI_POLL_DELAY_MS);
    }
    bvstk_mutex_unlock(&core->mutex);
    return status;
}
