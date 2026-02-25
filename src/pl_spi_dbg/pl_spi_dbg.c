#include "pl_spi_dbg.h"

#define PL_SPI_DBG_REG_CSR      0x00U
#define PL_SPI_DBG_REG_IRQ      0x04U
#define PL_SPI_DBG_REG_PACKET   0x08U
#define PL_SPI_DBG_REG_TIMEOUT  0x0CU
#define PL_SPI_DBG_REG_TX_FIFO  0x10U
#define PL_SPI_DBG_REG_CLK_DIV  0x18U

#define PL_SPI_DBG_CSR_WR_SOFT_RESET_BIT 0U
#define PL_SPI_DBG_CSR_WR_START_BIT      1U
#define PL_SPI_DBG_CSR_WR_READ_EN_BIT    2U

#define PL_SPI_DBG_CSR_RD_TX_FULL_BIT    0U
#define PL_SPI_DBG_CSR_RD_TX_EMPTY_BIT   1U
#define PL_SPI_DBG_CSR_RD_RX_FULL_BIT    2U
#define PL_SPI_DBG_CSR_RD_RX_EMPTY_BIT   3U

#define PL_SPI_DBG_IRQ_WR_DISPEL_BIT     0U

static inline uint32_t bit_u32(unsigned bit)
{
    return (1U << bit);
}

static inline void reg_write(const pl_spi_dbg_t *ctx, uint32_t reg_offset, uint32_t value)
{
    Xil_Out32(ctx->base_addr + reg_offset, value);
}

static inline uint32_t reg_read(const pl_spi_dbg_t *ctx, uint32_t reg_offset)
{
    return Xil_In32(ctx->base_addr + reg_offset);
}

static void pl_spi_dbg_read_bram_words(const pl_spi_dbg_t *ctx,
                                       uint32_t word_offset,
                                       uint32_t *out_words,
                                       size_t word_count)
{
    size_t i;
    if (ctx == NULL || out_words == NULL || word_count == 0U) return;

    for (i = 0U; i < word_count; ++i) {
        uint32_t addr = ctx->bram_base_addr + ((word_offset + (uint32_t)i) * 4U);
        out_words[i] = Xil_In32(addr);
    }
}

void pl_spi_dbg_init_defaults(pl_spi_dbg_t *ctx)
{
    if (ctx == NULL) return;
    ctx->base_addr = PL_SPI_DBG_DEFAULT_BASEADDR;
    ctx->bram_base_addr = PL_SPI_DBG_DEFAULT_BRAM_BASEADDR;
    ctx->mode = PL_SPI_DBG_MODE_MULTI;
    ctx->read_en = true;
}

void pl_spi_dbg_configure(pl_spi_dbg_t *ctx,
                         pl_spi_dbg_mode_t mode,
                         uint16_t clk_div_even_ge_2,
                         uint32_t timeout_ticks_ge_1,
                         bool read_en)
{
    uint16_t clk_div = clk_div_even_ge_2;
    uint32_t timeout = timeout_ticks_ge_1;

    if (ctx == NULL) return;

    if (clk_div < 2U) clk_div = 2U;
    if ((clk_div & 0x1U) != 0U) clk_div++;
    if (timeout == 0U) timeout = 1U;
    if (((uint32_t)mode & 0x3U) == 0U) mode = PL_SPI_DBG_MODE_MULTI;

    ctx->mode = mode;
    ctx->read_en = read_en;

    reg_write(ctx, PL_SPI_DBG_REG_CLK_DIV, (uint32_t)clk_div);
    reg_write(ctx, PL_SPI_DBG_REG_TIMEOUT, timeout);
}

bool pl_spi_dbg_transfer_words(const pl_spi_dbg_t *ctx,
                              const uint32_t *tx_words,
                              size_t tx_word_count,
                              uint32_t *rx_words,
                              size_t rx_word_capacity,
                              uint32_t max_poll_iters)
{
    size_t i;
    uint32_t packets_num;
    uint32_t packet_reg;
    uint32_t csr_start;

    if (ctx == NULL || tx_words == NULL || tx_word_count == 0U) return false;
    if (max_poll_iters == 0U) max_poll_iters = 1U;

    packets_num = (uint32_t)(tx_word_count * 4U);
    packet_reg = (packets_num << 2) | ((uint32_t)ctx->mode & 0x3U);
    csr_start = bit_u32(PL_SPI_DBG_CSR_WR_START_BIT);
    if (ctx->read_en) csr_start |= bit_u32(PL_SPI_DBG_CSR_WR_READ_EN_BIT);

    pl_spi_dbg_irq_clear(ctx);
    reg_write(ctx, PL_SPI_DBG_REG_PACKET, packet_reg);

    for (i = 0U; i < tx_word_count; ++i) {
        reg_write(ctx, PL_SPI_DBG_REG_TX_FIFO, tx_words[i]);
    }

    reg_write(ctx, PL_SPI_DBG_REG_CSR, csr_start);

    for (i = 0U; i < (size_t)max_poll_iters; ++i) {
        uint32_t csr = reg_read(ctx, PL_SPI_DBG_REG_CSR);
        bool tx_empty = ((csr & bit_u32(PL_SPI_DBG_CSR_RD_TX_EMPTY_BIT)) != 0U);
        bool rx_empty = ((csr & bit_u32(PL_SPI_DBG_CSR_RD_RX_EMPTY_BIT)) != 0U);
        if (tx_empty && rx_empty) {
            if (ctx->read_en && rx_words != NULL && rx_word_capacity > 0U) {
                size_t n = tx_word_count;
                if (n > rx_word_capacity) n = rx_word_capacity;
                pl_spi_dbg_read_bram_words(ctx, 0U, rx_words, n);
            }
            return true;
        }
    }

    return false;
}

void pl_spi_dbg_irq_clear(const pl_spi_dbg_t *ctx)
{
    uint32_t dispel = bit_u32(PL_SPI_DBG_IRQ_WR_DISPEL_BIT);
    if (ctx == NULL) return;
    reg_write(ctx, PL_SPI_DBG_REG_IRQ, dispel);
    reg_write(ctx, PL_SPI_DBG_REG_IRQ, 0U);
}
