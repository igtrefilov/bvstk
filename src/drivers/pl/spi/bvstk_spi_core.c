#include "drivers/pl/spi/bvstk_spi_core.h"

#include <string.h>

#include "hardware/pl/spi/bvstk_spi_regs.h"

enum {
    BVSTK_SPI_DEFAULT_TIMEOUT_MS = 100,
    BVSTK_SPI_MAX_WORDS = 1024
};

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

static bvstk_status_t normalize_config(const bvstk_spi_core_config_t *input,
                                       bvstk_spi_core_config_t *output)
{
    if (input == NULL || output == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    *output = *input;
    output->packets_mode &= UINT8_C(0x03);
    if (output->packets_mode == 0U) {
        output->packets_mode = BVSTK_SPI_MODE_MULTI;
    }
    if (output->timeout_ticks == 0U) {
        output->timeout_ticks = 1U;
    }
    if (output->p_clk_div < 2U) {
        output->p_clk_div = 2U;
    }
    if ((output->p_clk_div & 1U) != 0U) {
        output->p_clk_div++;
    }
    return BVSTK_OK;
}

static uint32_t effective_timeout(uint32_t timeout_ms)
{
    return timeout_ms == 0U ? BVSTK_SPI_DEFAULT_TIMEOUT_MS : timeout_ms;
}

static bvstk_status_t wait_done(bvstk_spi_core_t *core, uint32_t timeout_ms)
{
    uint64_t start = bvstk_clock_now_ms(&core->clock);
    uint32_t csr = 0U;

    timeout_ms = effective_timeout(timeout_ms);
    for (;;) {
        bvstk_status_t status = read32(&core->master,
                                       BVSTK_SPI_CSR_OFFSET,
                                       &csr);
        if (status != BVSTK_OK) {
            return status;
        }
        /* The existing PL contract exposes both FIFOs empty as idle. */
        if ((csr & UINT32_C(0x0A)) == UINT32_C(0x0A)) {
            return BVSTK_OK;
        }
        if (bvstk_clock_now_ms(&core->clock) - start >= timeout_ms) {
            return BVSTK_ERR_TIMEOUT;
        }
        bvstk_clock_sleep_ms(&core->clock, 1U);
    }
}

static void clear_irq(bvstk_spi_core_t *core)
{
    (void)write32(&core->master, BVSTK_SPI_IRQ_OFFSET, UINT32_C(1));
    (void)write32(&core->master, BVSTK_SPI_IRQ_OFFSET, UINT32_C(0));
}

bvstk_status_t bvstk_spi_core_init(bvstk_spi_core_t *core,
                                   const bvstk_spi_core_config_t *config,
                                   const bvstk_clock_t *clock,
                                   const bvstk_mutex_t *mutex)
{
    bvstk_spi_core_config_t normalized;

    if (core == NULL || normalize_config(config, &normalized) != BVSTK_OK) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(core, 0, sizeof(*core));
    core->config = normalized;
    if (clock != NULL) {
        core->clock = *clock;
    }
    if (mutex != NULL) {
        core->mutex = *mutex;
    }
    if (bvstk_mmio_region_open(&core->master,
                               BVSTK_SPI_MASTER_BASE,
                               BVSTK_SPI_MASTER_SIZE) != 0) {
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&core->bram,
                               BVSTK_SPI_BRAM_BASE,
                               BVSTK_SPI_BRAM_SIZE) != 0) {
        bvstk_mmio_region_close(&core->master);
        return BVSTK_ERR_IO;
    }
    core->initialized = 1U;
    (void)write32(&core->master,
                  BVSTK_SPI_TIMEOUT_OFFSET,
                  core->config.timeout_ticks);
    (void)write32(&core->master,
                  BVSTK_SPI_SIGNATURE_OFFSET,
                  core->config.p_clk_div);
    clear_irq(core);
    return BVSTK_OK;
}

void bvstk_spi_core_shutdown(bvstk_spi_core_t *core)
{
    if (core == NULL) {
        return;
    }
    if (core->initialized != 0U) {
        bvstk_mmio_region_close(&core->bram);
        bvstk_mmio_region_close(&core->master);
    }
    memset(core, 0, sizeof(*core));
}

bvstk_status_t bvstk_spi_core_set_config(bvstk_spi_core_t *core,
                                         const bvstk_spi_core_config_t *config)
{
    bvstk_spi_core_config_t normalized;
    bvstk_status_t status;

    if (core == NULL || core->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    status = normalize_config(config, &normalized);
    if (status != BVSTK_OK) {
        return status;
    }
    core->config = normalized;
    status = write32(&core->master,
                     BVSTK_SPI_TIMEOUT_OFFSET,
                     normalized.timeout_ticks);
    if (status == BVSTK_OK) {
        status = write32(&core->master,
                         BVSTK_SPI_SIGNATURE_OFFSET,
                         normalized.p_clk_div);
    }
    return status;
}

bvstk_status_t bvstk_spi_core_get_config(const bvstk_spi_core_t *core,
                                         bvstk_spi_core_config_t *config)
{
    if (core == NULL || core->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (config == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    *config = core->config;
    return BVSTK_OK;
}

bvstk_status_t bvstk_spi_core_transfer(bvstk_spi_core_t *core,
                                       const uint32_t *tx_words,
                                       size_t tx_count,
                                       uint32_t *rx_words,
                                       size_t rx_capacity,
                                       uint32_t timeout_ms)
{
    bvstk_status_t status;
    size_t i;
    size_t read_count;
    uint32_t packet_count;
    uint32_t packet_reg;
    uint32_t csr;

    if (core == NULL || core->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    if (tx_words == NULL || tx_count == 0U || tx_count > BVSTK_SPI_MAX_WORDS) {
        return BVSTK_ERR_RANGE;
    }
    status = bvstk_mutex_lock(&core->mutex, timeout_ms);
    if (status != BVSTK_OK) {
        return status;
    }

    clear_irq(core);
    status = write32(&core->master,
                     BVSTK_SPI_TIMEOUT_OFFSET,
                     core->config.timeout_ticks);
    if (status == BVSTK_OK) {
        status = write32(&core->master,
                         BVSTK_SPI_SIGNATURE_OFFSET,
                         core->config.p_clk_div);
    }
    packet_count = (uint32_t)(tx_count * 4U);
    packet_reg = (packet_count << 2) |
                 ((uint32_t)core->config.packets_mode & UINT32_C(0x03));
    if (status == BVSTK_OK) {
        status = write32(&core->master, BVSTK_SPI_PACKET_OFFSET, packet_reg);
    }
    for (i = 0U; status == BVSTK_OK && i < tx_count; ++i) {
        status = write32(&core->master,
                         BVSTK_SPI_TX_FIFO_OFFSET,
                         tx_words[i]);
    }
    csr = (UINT32_C(1) << 1) |
          ((core->config.read_enable ? UINT32_C(1) : UINT32_C(0)) << 2);
    if (status == BVSTK_OK) {
        status = write32(&core->master, BVSTK_SPI_CSR_OFFSET, csr);
    }
    if (status == BVSTK_OK) {
        status = wait_done(core, timeout_ms);
    }
    if (status == BVSTK_OK && rx_words != NULL && rx_capacity != 0U &&
        core->config.read_enable) {
        read_count = tx_count < rx_capacity ? tx_count : rx_capacity;
        for (i = 0U; i < read_count; ++i) {
            status = read32(&core->bram, i * sizeof(uint32_t), &rx_words[i]);
            if (status != BVSTK_OK) {
                break;
            }
        }
    }
    clear_irq(core);
    bvstk_mutex_unlock(&core->mutex);
    return status;
}
