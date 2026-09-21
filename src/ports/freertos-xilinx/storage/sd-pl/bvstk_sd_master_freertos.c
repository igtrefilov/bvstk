#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "ports/freertos-xilinx/os/bvstk_sync_freertos.h"

#include "task.h"
#include "xaxidma_hw.h"
#include "xil_io.h"
#include "xil_mmu.h"
#include "xparameters.h"
#include "xpseudo_asm.h"
#include "xscugic.h"
#include "xtime_l.h"

#if !defined(XPAR_SPI_SPI_MASTER_0_BASEADDR) || \
    !defined(XPAR_SPI_AXI_DMA_0_BASEADDR) || \
    !defined(XPAR_SPI_AXI_BRAM_CTRL_2_S_AXI_BASEADDR) || \
    !defined(XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR)
#error "The XSA must export SPI master, its BRAM, DMA and fabric IRQ"
#endif
#if XPAR_SPI_AXI_DMA_0_INCLUDE_SG || XPAR_SPI_AXI_DMA_0_ADDR_WIDTH != 32 || \
    !XPAR_SPI_AXI_DMA_0_INCLUDE_MM2S || !XPAR_SPI_AXI_DMA_0_INCLUDE_S2MM || \
    !XPAR_SPI_AXI_DMA_0_INCLUDE_MM2S_DRE || !XPAR_SPI_AXI_DMA_0_INCLUDE_S2MM_DRE
#error "SD master requires 32-bit simple DMA with both channels and DRE"
#endif

_Static_assert(SDM_DMA_ERRORS == XAXIDMA_ERR_ALL_MASK, "DMA error mask");
_Static_assert(SDM_DMA_IOC == XAXIDMA_IRQ_IOC_MASK, "DMA completion mask");

/* Owned by the FreeRTOS tick setup. Never reinitialize the shared GIC or
 * replace the processor's exception handler from a peripheral driver. */
extern XScuGic xInterruptController;

static bvstk_sd_master_t s_driver;
static bvstk_sd_service_t s_service;
static bvstk_freertos_mutex_t s_mutex;
static volatile uint32_t s_irq_count;
static volatile int s_startup_done;
static bool s_started;

static void sd_master_start_task(void *argument)
{
    bvstk_status_t status;
    (void)argument;
    status = bvstk_sd_master_freertos_start();
    if (status != BVSTK_OK) {
        xil_printf("SD PL: service startup failed: %s\r\n",
                   bvstk_status_string(status));
    } else {
        xil_printf("SD PL: service ready; card initialization is explicit\r\n");
    }
    s_startup_done = 1;
    vTaskDelete(NULL);
}

static void barrier(void *context)
{
    (void)context;
    dsb();
}

static void disable(void *context)
{
    (void)context;
    XScuGic_Disable(&xInterruptController, XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR);
    dsb();
}

static void core_isr(void *context)
{
    disable(context); /* Bound a stuck level IRQ; work stays in task context. */
    ++s_irq_count;
    dsb();
}

static uint32_t count(void *context)
{
    (void)context;
    return s_irq_count;
}

static void sleep_ms(void *context, uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    (void)context;
    vTaskDelay(ticks ? ticks : 1);
}

static uint64_t now_ms(void *context)
{
    XTime ticks;
    (void)context;
    XTime_GetTime(&ticks);
    return ticks / (COUNTS_PER_SECOND / 1000U);
}

static bvstk_status_t prepare(void *context)
{
    const uint32_t irq = XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR;
    disable(context);
    Xil_Out32(XPAR_SPI_SPI_MASTER_0_BASEADDR + SDM_IRQ, SDM_ACK);
    dsb();
    sleep_ms(NULL, 1);
    (void)Xil_In32(XPAR_SPI_SPI_MASTER_0_BASEADDR + SDM_IRQ);
    s_irq_count = 0;
    XScuGic_DistWriteReg(&xInterruptController,
        XSCUGIC_PENDING_CLR_OFFSET + (irq / 32U) * 4U,
        UINT32_C(1) << (irq % 32U));
    dsb();
    XScuGic_Enable(&xInterruptController, irq);
    dsb();
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_master_freertos_start(void)
{
    bvstk_status_t status;
    const bvstk_sd_master_config_t config = {
        .core_base = XPAR_SPI_SPI_MASTER_0_BASEADDR,
        .dma_base = XPAR_SPI_AXI_DMA_0_BASEADDR,
        .bram_base = XPAR_SPI_AXI_BRAM_CTRL_2_S_AXI_BASEADDR,
        .bram_size = XPAR_SPI_AXI_BRAM_CTRL_2_S_AXI_HIGHADDR -
                     XPAR_SPI_AXI_BRAM_CTRL_2_S_AXI_BASEADDR + 1U,
        .init_divider = 512, .data_divider = 512,
        .events = {NULL, prepare, count, disable, barrier},
        .read_mailbox_is_stream = true,
        .read_descriptor_bytes = 8
    };
    const bvstk_clock_t clock = {NULL, now_ms, sleep_ms};
    if (s_started) {
        s_startup_done = 1;
        return BVSTK_OK;
    }
    status = bvstk_freertos_mutex_init(&s_mutex);
    if (status != BVSTK_OK) {
        s_startup_done = 1;
        return status;
    }
    /* Separate device-memory BRAM buffers eliminate DDR cache ownership and
     * cache-line overlap with mailboxes. The whole AXI master is exclusive. */
    Xil_SetTlbAttributes((INTPTR)config.core_base, DEVICE_MEMORY);
    Xil_SetTlbAttributes((INTPTR)config.dma_base, DEVICE_MEMORY);
    Xil_SetTlbAttributes((INTPTR)config.bram_base, DEVICE_MEMORY);
    disable(NULL);
    if (XScuGic_Connect(&xInterruptController,
            XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR, core_isr, NULL) != XST_SUCCESS) {
        bvstk_freertos_mutex_destroy(&s_mutex);
        s_startup_done = 1;
        return BVSTK_ERR_IO;
    }
    XScuGic_SetPriorityTriggerType(&xInterruptController,
        XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR, 0xa0, 1);
    status = bvstk_sd_master_open(&s_driver, &config, &clock);
    if (status == BVSTK_OK)
        status = bvstk_sd_service_attach(&s_service, &s_driver, &s_mutex.public_mutex, true);
    if (status != BVSTK_OK) {
        bvstk_sd_master_close(&s_driver);
        XScuGic_Disconnect(&xInterruptController, XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR);
        bvstk_freertos_mutex_destroy(&s_mutex);
        s_startup_done = 1;
        return status;
    }
    s_started = true;
    s_startup_done = 1;
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_master_freertos_schedule_start(void)
{
    if (s_started) return BVSTK_OK;
    s_startup_done = 0;
    if (xTaskCreate(sd_master_start_task, "sd-pl-start", 1024, NULL,
                    tskIDLE_PRIORITY + 4, NULL) == pdPASS) {
        return BVSTK_OK;
    }
    s_startup_done = 1;
    return BVSTK_ERR_INTERNAL;
}

int bvstk_sd_master_freertos_startup_done(void)
{
    return s_startup_done != 0;
}

bvstk_sd_service_t *bvstk_sd_master_freertos_service(void)
{
    return s_started ? &s_service : NULL;
}
