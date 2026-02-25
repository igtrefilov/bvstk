#include "pl_spi_dbg.h"

#include <stddef.h>
#include <stdint.h>

#include "xil_io.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"

#define PL_SPI_DBG_DEFAULT_BASEADDR      XPAR_SPI_MASTER_0_BASEADDR
#define PL_SPI_DBG_DEFAULT_BRAM_BASEADDR XPAR_AXI_BRAM_CTRL_2_S_AXI_BASEADDR
#define PL_SPI_DBG_IRQ_ID                XPAR_FABRIC_SPI_MASTER_0_IRQ_INTR

#define PL_SPI_DBG_REG_CSR      0x00U
#define PL_SPI_DBG_REG_IRQ      0x04U
#define PL_SPI_DBG_REG_PACKET   0x08U
#define PL_SPI_DBG_REG_TIMEOUT  0x0CU
#define PL_SPI_DBG_REG_TX_FIFO  0x10U
#define PL_SPI_DBG_REG_CLK_DIV  0x18U

#define PL_SPI_DBG_CSR_WR_START_BIT      1U
#define PL_SPI_DBG_CSR_WR_READ_EN_BIT    2U

#define PL_SPI_DBG_CSR_RD_TX_EMPTY_BIT   1U
#define PL_SPI_DBG_CSR_RD_RX_EMPTY_BIT   3U

#define PL_SPI_DBG_IRQ_WR_DISPEL_BIT     0U

#define PL_SPI_DBG_MODE_MULTI            0x2U

typedef struct {
    uint32_t base_addr;
    uint32_t bram_base_addr;
    uint8_t mode;
    bool read_en;
} pl_spi_dbg_ctx_t;

static pl_spi_dbg_ctx_t g_ctx = {
    .base_addr = PL_SPI_DBG_DEFAULT_BASEADDR,
    .bram_base_addr = PL_SPI_DBG_DEFAULT_BRAM_BASEADDR,
    .mode = PL_SPI_DBG_MODE_MULTI,
    .read_en = true,
};

static volatile bool g_spi_irq_seen = false;
static bool g_spi_irq_inited = false;

static inline uint32_t bit_u32(unsigned bit)
{
    return (1U << bit);
}

static inline void reg_write(uint32_t reg_offset, uint32_t value)
{
    Xil_Out32(g_ctx.base_addr + reg_offset, value);
}

static inline uint32_t reg_read(uint32_t reg_offset)
{
    return Xil_In32(g_ctx.base_addr + reg_offset);
}

static void pl_spi_dbg_irq_clear_local(void)
{
    uint32_t dispel = bit_u32(PL_SPI_DBG_IRQ_WR_DISPEL_BIT);
    reg_write(PL_SPI_DBG_REG_IRQ, dispel);
    reg_write(PL_SPI_DBG_REG_IRQ, 0U);
}

static void pl_spi_dbg_configure_local(uint16_t clk_div_even_ge_2,
                                       uint32_t timeout_ticks_ge_1,
                                       bool read_en)
{
    uint16_t clk_div = clk_div_even_ge_2;
    uint32_t timeout = timeout_ticks_ge_1;

    if (clk_div < 2U) clk_div = 2U;
    if ((clk_div & 0x1U) != 0U) clk_div++;
    if (timeout == 0U) timeout = 1U;

    g_ctx.mode = PL_SPI_DBG_MODE_MULTI;
    g_ctx.read_en = read_en;

    reg_write(PL_SPI_DBG_REG_CLK_DIV, (uint32_t)clk_div);
    reg_write(PL_SPI_DBG_REG_TIMEOUT, timeout);
}

static bool pl_spi_dbg_transfer_words_local(const uint32_t *tx_words,
                                            size_t tx_word_count,
                                            uint32_t max_poll_iters)
{
    size_t i;
    uint32_t packets_num;
    uint32_t packet_reg;
    uint32_t csr_start;

    if (tx_words == NULL || tx_word_count == 0U) return false;
    if (max_poll_iters == 0U) max_poll_iters = 1U;

    packets_num = (uint32_t)(tx_word_count * 4U);
    packet_reg = (packets_num << 2) | ((uint32_t)g_ctx.mode & 0x3U);
    csr_start = bit_u32(PL_SPI_DBG_CSR_WR_START_BIT);
    if (g_ctx.read_en) csr_start |= bit_u32(PL_SPI_DBG_CSR_WR_READ_EN_BIT);

    pl_spi_dbg_irq_clear_local();
    reg_write(PL_SPI_DBG_REG_PACKET, packet_reg);

    for (i = 0U; i < tx_word_count; ++i) {
        reg_write(PL_SPI_DBG_REG_TX_FIFO, tx_words[i]);
    }

    reg_write(PL_SPI_DBG_REG_CSR, csr_start);

    for (i = 0U; i < (size_t)max_poll_iters; ++i) {
        uint32_t csr = reg_read(PL_SPI_DBG_REG_CSR);
        bool tx_empty = ((csr & bit_u32(PL_SPI_DBG_CSR_RD_TX_EMPTY_BIT)) != 0U);
        bool rx_empty = ((csr & bit_u32(PL_SPI_DBG_CSR_RD_RX_EMPTY_BIT)) != 0U);
        if (tx_empty && rx_empty) return true;
    }

    return false;
}

static void pl_spi_dbg_isr(void *callback_ref)
{
    (void)callback_ref;
    g_spi_irq_seen = true;
    pl_spi_dbg_irq_clear_local();
}

static bool pl_spi_dbg_irq_init_local(void)
{
    if (g_spi_irq_inited) return true;
    xPortInstallInterruptHandler(PL_SPI_DBG_IRQ_ID, pl_spi_dbg_isr, NULL);
    vPortEnableInterrupt(PL_SPI_DBG_IRQ_ID);
    pl_spi_dbg_irq_clear_local();
    g_spi_irq_inited = true;
    return true;
}

void start_pl_spi_dbg(void)
{
    g_ctx.base_addr = PL_SPI_DBG_DEFAULT_BASEADDR;
    g_ctx.bram_base_addr = PL_SPI_DBG_DEFAULT_BRAM_BASEADDR;
    pl_spi_dbg_configure_local(512U, 1U, true);
    (void)pl_spi_dbg_irq_init_local();

    xil_printf("PL SPI DBG: ready (base=0x%08lX, bram=0x%08lX)\r\n",
               (unsigned long)g_ctx.base_addr,
               (unsigned long)g_ctx.bram_base_addr);
}

bool pl_spi_dbg_transfer(void)
{
    uint32_t tx = 0xFF000000U;
    uint32_t i;

    if (!g_spi_irq_inited) {
        xil_printf("PL SPI DBG transfer: irq is not initialized\r\n");
        return false;
    }

    g_spi_irq_seen = false;
    g_ctx.read_en = false;
    (void)pl_spi_dbg_transfer_words_local(&tx, 1U, 1000U);

    for (i = 0U; i < 200U; ++i) {
        if (g_spi_irq_seen) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    xil_printf("PL SPI DBG irq example: %s\r\n", g_spi_irq_seen ? "irq received" : "irq timeout");
    return g_spi_irq_seen;
}

