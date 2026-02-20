#include "bvstk_spi.h"

#include <string.h>

#include "xil_printf.h"

typedef struct {
    uint32_t csr_snapshot;
} spi_evt_t;

static QueueHandle_t q_spi_irq = NULL;
static SemaphoreHandle_t spi_bus_mutex = NULL;

static volatile spi_runtime_cfg_t s_cfg = {
    .packets_mode = SPI_MODE_MULTI,
    .timeout_ticks = 1,
    .p_clk_div = 512,
    .read_en = true,
};

static inline void reg_write32(uint32_t ofs, uint32_t v) { Xil_Out32(SPI_BASEADDR + ofs, v); }
static inline uint32_t reg_read32(uint32_t ofs) { return Xil_In32(SPI_BASEADDR + ofs); }

static inline void spi_irq_clear(void)
{
    /* irq_dispel is a level control bit in IRQ write register. */
    reg_write32(SPI_IRQ_REG_OFFSET, 0x1U);
    reg_write32(SPI_IRQ_REG_OFFSET, 0x0U);
}

#if SPI_HAS_IRQ
static void spi_ISR(void *CallBackRef)
{
    (void)CallBackRef;

    uint32_t csr = reg_read32(SPI_CSR_REG_OFFSET);
    spi_irq_clear();

    if (q_spi_irq) {
        BaseType_t hpw = pdFALSE;
        spi_evt_t evt = { .csr_snapshot = csr };
        (void)xQueueOverwriteFromISR(q_spi_irq, &evt, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}
#endif

void start_spi(void)
{
    spi_bus_mutex = xSemaphoreCreateMutex();
    configASSERT(spi_bus_mutex);

#if SPI_HAS_IRQ
    q_spi_irq = xQueueCreate(1, sizeof(spi_evt_t));
    configASSERT(q_spi_irq);
    xPortInstallInterruptHandler(SPI_IRQ_INTR, spi_ISR, NULL);
    vPortEnableInterrupt(SPI_IRQ_INTR);
#endif

    spi_irq_clear();
    reg_write32(SPI_TIMEOUT_OFFSET, s_cfg.timeout_ticks ? s_cfg.timeout_ticks : 1U);
    reg_write32(SPI_SIG_REG_OFFSET, (uint32_t)s_cfg.p_clk_div);

    xil_printf("SPI: started (base=0x%08lX, bram=0x%08lX, irq=%s)\r\n",
               (unsigned long)SPI_BASEADDR,
               (unsigned long)SPI_BRAM_BASEADDR,
#if SPI_HAS_IRQ
               "on"
#else
               "off"
#endif
    );
}

void spi_set_cfg(const spi_runtime_cfg_t *cfg)
{
    if (!cfg) return;

    taskENTER_CRITICAL();
    s_cfg.packets_mode = (cfg->packets_mode & 0x3U);
    if (s_cfg.packets_mode == 0U) s_cfg.packets_mode = SPI_MODE_MULTI;
    s_cfg.timeout_ticks = cfg->timeout_ticks ? cfg->timeout_ticks : 1U;
    s_cfg.p_clk_div = cfg->p_clk_div;
    if (s_cfg.p_clk_div < 2U) s_cfg.p_clk_div = 2U;
    if (s_cfg.p_clk_div & 0x1U) s_cfg.p_clk_div++;
    s_cfg.read_en = cfg->read_en;
    taskEXIT_CRITICAL();

    reg_write32(SPI_TIMEOUT_OFFSET, s_cfg.timeout_ticks);
    reg_write32(SPI_SIG_REG_OFFSET, (uint32_t)s_cfg.p_clk_div);
}

void spi_get_cfg(spi_runtime_cfg_t *out)
{
    if (!out) return;
    taskENTER_CRITICAL();
    *out = s_cfg;
    taskEXIT_CRITICAL();
}

static bool spi_wait_done(TickType_t timeout_ticks)
{
#if SPI_HAS_IRQ
    spi_evt_t evt;
    if (xQueueReceive(q_spi_irq, &evt, timeout_ticks) == pdTRUE) {
        (void)evt;
        return true;
    }
    return false;
#else
    TickType_t deadline = xTaskGetTickCount() + timeout_ticks;
    for (;;) {
        uint32_t csr = reg_read32(SPI_CSR_REG_OFFSET);
        /* tx_fifo_empty + rx_fifo_empty => idle path for most flows. */
        if ((csr & 0x0AU) == 0x0AU) return true;
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
#endif
}

bool spi_transfer_words(const uint32_t *tx_words,
                        size_t tx_count,
                        uint32_t *rx_words,
                        size_t rx_capacity,
                        TickType_t timeout_ticks)
{
    if (!tx_words || tx_count == 0) return false;

    if (!spi_bus_mutex) return false;
    xSemaphoreTake(spi_bus_mutex, portMAX_DELAY);

    spi_runtime_cfg_t cfg;
    spi_get_cfg(&cfg);

#if SPI_HAS_IRQ
    xQueueReset(q_spi_irq);
#endif

    spi_irq_clear();
    reg_write32(SPI_TIMEOUT_OFFSET, cfg.timeout_ticks ? cfg.timeout_ticks : 1U);
    reg_write32(SPI_SIG_REG_OFFSET, (uint32_t)cfg.p_clk_div);

    uint32_t packets_num = (uint32_t)(tx_count * 4U);
    uint32_t packet_reg = (packets_num << 2) | ((uint32_t)cfg.packets_mode & 0x3U);
    reg_write32(SPI_PACKET_OFFSET, packet_reg);

    for (size_t i = 0; i < tx_count; ++i) {
        reg_write32(SPI_TX_FIFO_OFFSET, tx_words[i]);
    }

    uint32_t csr = (1U << 1) | ((cfg.read_en ? 1U : 0U) << 2);
    reg_write32(SPI_CSR_REG_OFFSET, csr);

    bool done = spi_wait_done(timeout_ticks ? timeout_ticks : pdMS_TO_TICKS(100));

    if (done && rx_words && rx_capacity > 0U && cfg.read_en) {
        size_t n = (tx_count < rx_capacity) ? tx_count : rx_capacity;
        for (size_t i = 0; i < n; ++i) {
            rx_words[i] = Xil_In32(SPI_BRAM_BASEADDR + (uint32_t)(i * 4U));
        }
    }

    spi_irq_clear();
    xSemaphoreGive(spi_bus_mutex);
    return done;
}
