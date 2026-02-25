#include "pl_spi_dbg_examples.h"

#include "pl_spi_dbg.h"

#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"

static pl_spi_dbg_t g_pl_spi_ctx;

static volatile bool g_spi_irq_seen = false;
static bool g_spi_irq_inited = false;

static void pl_spi_dbg_example_isr(void *callback_ref)
{
    const pl_spi_dbg_t *ctx = (const pl_spi_dbg_t *)callback_ref;
    g_spi_irq_seen = true;
    pl_spi_dbg_irq_clear(ctx);
}

static bool pl_spi_dbg_irq_init(void)
{
    if (g_spi_irq_inited) return true;
    xPortInstallInterruptHandler(PL_SPI_DBG_IRQ_ID, pl_spi_dbg_example_isr, &g_pl_spi_ctx);
    vPortEnableInterrupt(PL_SPI_DBG_IRQ_ID);
    pl_spi_dbg_irq_clear(&g_pl_spi_ctx);
    g_spi_irq_inited = true;
    return true;
}

void start_pl_spi_dbg(void)
{
    pl_spi_dbg_init_defaults(&g_pl_spi_ctx);
    pl_spi_dbg_configure(&g_pl_spi_ctx, PL_SPI_DBG_MODE_MULTI, 512U, 1U, true);
    (void)pl_spi_dbg_irq_init();
    xil_printf("PL SPI DBG: ready (base=0x%08lX, bram=0x%08lX)\r\n",
               (unsigned long)g_pl_spi_ctx.base_addr,
               (unsigned long)g_pl_spi_ctx.bram_base_addr);
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
    pl_spi_dbg_configure(&g_pl_spi_ctx, PL_SPI_DBG_MODE_MULTI, 512U, 1U, false);
    (void)pl_spi_dbg_transfer_words(&g_pl_spi_ctx, &tx, 1U, NULL, 0U, 1000U);

    for (i = 0U; i < 200U; ++i) {
        if (g_spi_irq_seen) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    xil_printf("PL SPI DBG irq example: %s\r\n", g_spi_irq_seen ? "irq received" : "irq timeout");
    return g_spi_irq_seen;
}
