#include "apps/freertos/diagnostics/pl_spi_test/driver.h"
#include "apps/freertos/diagnostics/pl_spi_test/config.h"

#include <string.h>

#include "FreeRTOS.h"
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
#error "The selected XSA must export SPI master, its BRAM, DMA and fabric IRQ"
#endif
#if XPAR_SPI_AXI_DMA_0_INCLUDE_SG || XPAR_SPI_AXI_DMA_0_ADDR_WIDTH != 32 || \
    !XPAR_SPI_AXI_DMA_0_INCLUDE_MM2S || !XPAR_SPI_AXI_DMA_0_INCLUDE_S2MM || \
    !XPAR_SPI_AXI_DMA_0_INCLUDE_MM2S_DRE || !XPAR_SPI_AXI_DMA_0_INCLUDE_S2MM_DRE
#error "Diagnostic requires 32-bit simple DMA with both channels and DRE"
#endif

_Static_assert(DMA_ERRORS == XAXIDMA_ERR_ALL_MASK, "DMA error mask");
_Static_assert(DMA_IOC == XAXIDMA_IRQ_IOC_MASK, "DMA completion mask");
_Static_assert(DMA_RX_LENGTH == XAXIDMA_RX_OFFSET + XAXIDMA_BUFFLEN_OFFSET,
               "DMA RX length register");

/* Owned and initialized by the Xilinx FreeRTOS tick setup. Do not initialize
 * another GIC instance or reinstall the processor's exception handler here. */
extern XScuGic xInterruptController;

uint32_t pl_spi_read(const pl_spi_t *dev, uint32_t offset)
{
    return Xil_In32((UINTPTR)(dev->core + offset));
}

void pl_spi_write(const pl_spi_t *dev, uint32_t offset, uint32_t value)
{
    Xil_Out32((UINTPTR)(dev->core + offset), value);
    dsb();
}

uint32_t pl_spi_dma_read(const pl_spi_t *dev, uint32_t offset)
{
    return Xil_In32((UINTPTR)(dev->dma + offset));
}

static void dma_write(const pl_spi_t *dev, uint32_t offset, uint32_t value)
{
    Xil_Out32((UINTPTR)(dev->dma + offset), value);
    dsb();
}

uint32_t pl_spi_bram_read(const pl_spi_t *dev, uint32_t offset)
{
    configASSERT((offset & 3U) == 0U && offset <= dev->bram_size - 4U);
    return Xil_In32((UINTPTR)(dev->bram + offset));
}

void pl_spi_bram_write(const pl_spi_t *dev, uint32_t offset, uint32_t value)
{
    configASSERT((offset & 3U) == 0U && offset <= dev->bram_size - 4U);
    Xil_Out32((UINTPTR)(dev->bram + offset), value);
    dsb();
}

uint32_t pl_spi_now_ms(void)
{
    XTime ticks;
    XTime_GetTime(&ticks);
    return (uint32_t)(ticks / (COUNTS_PER_SECOND / 1000U));
}

void pl_spi_delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks != 0U ? ticks : 1U);
}

static void core_isr(void *argument)
{
    pl_spi_t *dev = argument;
    /* Preserve the cause for the task. Mask the GIC input to bound a stuck
     * level interrupt; do not acknowledge the core from this handler. */
    XScuGic_Disable(&xInterruptController, dev->irq_id);
    dev->irq_latched |= pl_spi_read(dev, SPI_IRQ);
    ++dev->irq_count;
    dsb();
}

bool pl_spi_open(pl_spi_t *dev)
{
    memset(dev, 0, sizeof(*dev));
    dev->core = XPAR_SPI_SPI_MASTER_0_BASEADDR;
    dev->dma = XPAR_SPI_AXI_DMA_0_BASEADDR;
    dev->bram = XPAR_SPI_AXI_BRAM_CTRL_2_S_AXI_BASEADDR;
    dev->bram_size = XPAR_SPI_AXI_BRAM_CTRL_2_S_AXI_HIGHADDR - dev->bram + 1U;
    dev->irq_id = XPAR_FABRIC_SPI_SPI_MASTER_0_IRQ_INTR;
    if (dev->bram_size < SPI_RX_AREA + SPI_RX_CAPACITY + 4U ||
        xInterruptController.IsReady != XIL_COMPONENT_IS_READY) {
        return false;
    }
    /* All DMA data is in BRAM. No cached DDR buffer is shared with DMA. */
    Xil_SetTlbAttributes((INTPTR)dev->core, DEVICE_MEMORY);
    Xil_SetTlbAttributes((INTPTR)dev->dma, DEVICE_MEMORY);
    Xil_SetTlbAttributes((INTPTR)dev->bram, DEVICE_MEMORY);
    XScuGic_Disable(&xInterruptController, dev->irq_id);
    if (XScuGic_Connect(&xInterruptController, dev->irq_id, core_isr, dev) !=
        XST_SUCCESS) {
        return false;
    }
    XScuGic_SetPriorityTriggerType(&xInterruptController, dev->irq_id,
                                   0xA0U, 1U); /* Level high. */
    dev->connected = true;
    return true;
}

void pl_spi_core_reset(pl_spi_t *dev)
{
    XScuGic_Disable(&xInterruptController, dev->irq_id);
    pl_spi_write(dev, SPI_CSR, SPI_RESET);
    pl_spi_delay_ms(2U);
    dev->irq_latched = 0U;
    dev->irq_count = 0U;
}

pl_spi_result_t pl_spi_dma_reset(pl_spi_t *dev)
{
    uint32_t start = pl_spi_now_ms();
    /* A reset of either AXI DMA channel resets the complete DMA engine. */
    dma_write(dev, DMA_TX_CONTROL, DMA_RESET);
    do {
        if (((pl_spi_dma_read(dev, DMA_TX_CONTROL) |
              pl_spi_dma_read(dev, DMA_RX_CONTROL)) & DMA_RESET) == 0U) {
            dma_write(dev, DMA_TX_STATUS, DMA_IRQS);
            dma_write(dev, DMA_RX_STATUS, DMA_IRQS);
            dev->rx_capacity = 0U;
            dev->tx_size = 0U;
            return PL_SPI_OK;
        }
        pl_spi_delay_ms(1U);
    } while ((uint32_t)(pl_spi_now_ms() - start) < PL_SPI_TEST_RESET_MS);
    return PL_SPI_TIMEOUT;
}

void pl_spi_irq_prepare(pl_spi_t *dev)
{
    XScuGic_Disable(&xInterruptController, dev->irq_id);
    pl_spi_write(dev, SPI_IRQ, SPI_ACK);
    pl_spi_delay_ms(1U);
    (void)pl_spi_read(dev, SPI_IRQ);
    dev->irq_count = 0U;
    dev->irq_latched = 0U;
    XScuGic_DistWriteReg(&xInterruptController,
                         XSCUGIC_PENDING_CLR_OFFSET + (dev->irq_id / 32U) * 4U,
                         UINT32_C(1) << (dev->irq_id % 32U));
    dsb();
    XScuGic_Enable(&xInterruptController, dev->irq_id);
}

static uint32_t rounded(uint32_t size)
{
    return (size + 3U) & ~UINT32_C(3);
}

static bool guards_ok(const pl_spi_t *dev)
{
    if (dev->tx_size != 0U &&
        (pl_spi_bram_read(dev, SPI_TX_AREA - 4U) != SPI_GUARD ||
         pl_spi_bram_read(dev, SPI_TX_AREA + rounded(dev->tx_size)) != SPI_GUARD)) {
        return false;
    }
    if (dev->rx_capacity != 0U) {
        uint32_t tail = dev->rx_capacity & 3U;
        if (pl_spi_bram_read(dev, SPI_RX_AREA - 4U) != SPI_GUARD ||
            pl_spi_bram_read(dev, SPI_RX_AREA + rounded(dev->rx_capacity)) !=
                SPI_GUARD) {
            return false;
        }
        if (tail != 0U) {
            uint32_t mask = UINT32_MAX << (tail * 8U);
            uint32_t word = pl_spi_bram_read(dev, SPI_RX_AREA +
                                             (dev->rx_capacity & ~3U));
            if ((word & mask) != (SPI_SENTINEL & mask)) return false;
        }
    }
    return true;
}

void pl_spi_observe(pl_spi_t *dev, pl_spi_observation_t *out)
{
    out->csr = pl_spi_read(dev, SPI_CSR);
    out->irq = pl_spi_read(dev, SPI_IRQ);
    taskENTER_CRITICAL();
    dev->irq_latched |= out->irq;
    out->irq_seen = dev->irq_latched;
    out->irq_count = dev->irq_count;
    taskEXIT_CRITICAL();
    out->tx_control = pl_spi_dma_read(dev, DMA_TX_CONTROL);
    out->tx_status = pl_spi_dma_read(dev, DMA_TX_STATUS);
    out->rx_control = pl_spi_dma_read(dev, DMA_RX_CONTROL);
    out->rx_status = pl_spi_dma_read(dev, DMA_RX_STATUS);
    out->rx_length = pl_spi_dma_read(dev, DMA_RX_LENGTH);
    out->guards_ok = guards_ok(dev);
}

static bool dma_done(uint32_t status)
{
    return (status & (DMA_IOC | DMA_IDLE | DMA_HALTED | DMA_ERRORS)) ==
           (DMA_IOC | DMA_IDLE);
}

static void prepare_rx(pl_spi_t *dev, uint32_t capacity)
{
    uint32_t offset;
    configASSERT(capacity != 0U && capacity <= SPI_RX_CAPACITY);
    dev->rx_capacity = capacity;
    pl_spi_bram_write(dev, SPI_RX_AREA - 4U, SPI_GUARD);
    for (offset = 0U; offset < rounded(capacity); offset += 4U) {
        pl_spi_bram_write(dev, SPI_RX_AREA + offset, SPI_SENTINEL);
    }
    pl_spi_bram_write(dev, SPI_RX_AREA + rounded(capacity), SPI_GUARD);
    dma_write(dev, DMA_RX_STATUS, DMA_IRQS);
    dma_write(dev, DMA_RX_CONTROL, DMA_RUN);
    dma_write(dev, DMA_RX_DESTINATION, (uint32_t)dev->bram + SPI_RX_AREA);
    dma_write(dev, DMA_RX_LENGTH, capacity);
}

static void copy_rx(const pl_spi_t *dev, uint8_t *bytes, uint32_t size)
{
    uint32_t offset, word = 0U;
    for (offset = 0U; offset < size; ++offset) {
        if ((offset & 3U) == 0U) word = pl_spi_bram_read(dev, SPI_RX_AREA + offset);
        bytes[offset] = (uint8_t)(word >> ((offset & 3U) * 8U));
    }
}

uint8_t pl_spi_crc7(const uint8_t *bytes, size_t size)
{
    uint8_t crc = 0U;
    size_t i;
    unsigned bit;
    for (i = 0U; i < size; ++i) {
        for (bit = 0U; bit < 8U; ++bit) {
            uint8_t feedback = ((crc >> 6) ^ (bytes[i] >> (7U - bit))) & 1U;
            crc = (uint8_t)((crc << 1) & 0x7FU);
            if (feedback != 0U) crc ^= 0x09U;
        }
    }
    return (uint8_t)((crc << 1) | 1U);
}

uint16_t pl_spi_crc16(const uint8_t *bytes, size_t size)
{
    uint16_t crc = 0U;
    size_t i;
    unsigned bit;
    for (i = 0U; i < size; ++i) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
        }
    }
    return crc;
}

uint32_t pl_spi_crc32(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t i;
    unsigned bit;
    for (i = 0U; i < size; ++i) {
        crc ^= bytes[i];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? UINT32_C(0xEDB88320) : 0U);
        }
    }
    return ~crc;
}

void pl_spi_encode_command(uint8_t command, uint32_t argument, uint32_t words[2])
{
    uint8_t frame[5] = { (uint8_t)(0x40U | command), (uint8_t)(argument >> 24),
        (uint8_t)(argument >> 16), (uint8_t)(argument >> 8), (uint8_t)argument };
    /* The CMD16 vector in the supplied Tcl stores argument 512 as 00020000.
     * Opcode and CRC occupy the low two bytes of the second DMA word. */
    words[0] = ((argument & 0xFFU) << 24) | ((argument & 0xFF00U) << 8) |
               ((argument >> 8) & 0xFF00U) | ((argument >> 24) & 0xFFU);
    words[1] = frame[0] | ((uint32_t)pl_spi_crc7(frame, sizeof(frame)) << 8);
}

static uint32_t response_offset(uint8_t command)
{
    switch (command) {
    case 13U: return MB_CMD13;
    case 16U: return MB_CMD16;
    case 17U: return MB_CMD17;
    case 24U: return MB_CMD24;
    case 32U: return MB_CMD32;
    case 33U: return MB_CMD33;
    case 38U: return MB_CMD38;
    default: return UINT32_MAX;
    }
}

pl_spi_result_t pl_spi_initialize_card(pl_spi_t *dev, pl_spi_observation_t *out)
{
    uint32_t start, i;
    pl_spi_result_t result;
    memset(out, 0, sizeof(*out));
    pl_spi_core_reset(dev);
    result = pl_spi_dma_reset(dev);
    if (result != PL_SPI_OK) {
        pl_spi_observe(dev, out);
        return result;
    }
    for (i = 0U; i < SPI_INIT_WORDS; ++i) {
        pl_spi_bram_write(dev, i * 4U, SPI_SENTINEL);
    }
    pl_spi_write(dev, SPI_PACKET, SPI_PACKET_VALUE(SPI_STREAM, 0U));
    pl_spi_write(dev, SPI_DELAY, 1U);
    pl_spi_write(dev, SPI_DIVIDER, PL_SPI_TEST_INIT_DIV);
    pl_spi_write(dev, SPI_CSR, SPI_START | SPI_READ);
    pl_spi_irq_prepare(dev);
    start = pl_spi_now_ms();
    pl_spi_write(dev, SPI_SD_INIT, (PL_SPI_TEST_INIT_CLOCKS << 2) |
                  SPI_SD_CLOCK_ENABLE | SPI_SD_SEQUENCE_ENABLE);
    result = PL_SPI_TIMEOUT;
    do {
        pl_spi_observe(dev, out);
        for (i = 0U; i < SPI_INIT_WORDS; ++i) {
            out->mailbox[i] = pl_spi_bram_read(dev, i * 4U);
        }
        if ((out->irq_seen & (SPI_SD_IRQ_HARD_INIT | SPI_SD_IRQ_SOFT_INIT)) ==
            (SPI_SD_IRQ_HARD_INIT | SPI_SD_IRQ_SOFT_INIT) &&
            out->mailbox[MB_CMD58_R1 / 4U] != SPI_SENTINEL &&
            out->mailbox[MB_CMD58_OCR / 4U] != SPI_SENTINEL) {
            result = PL_SPI_OK;
            break;
        }
        pl_spi_delay_ms(1U);
    } while ((uint32_t)(pl_spi_now_ms() - start) < PL_SPI_TEST_INIT_MS);
    if (result == PL_SPI_OK) {
        /* Give a concurrently asserted fabric IRQ time to reach the handler
         * before reporting its count. This does not substitute for a cause. */
        pl_spi_delay_ms(1U);
        pl_spi_observe(dev, out);
    }
    out->elapsed_ms = pl_spi_now_ms() - start;
    return result;
}

pl_spi_result_t pl_spi_command(pl_spi_t *dev, uint8_t command,
                               uint32_t argument, const uint8_t *write_block,
                               uint8_t *read_block_with_crc,
                               pl_spi_observation_t *out)
{
    uint32_t words[2], offset, start;
    uint32_t mailbox = response_offset(command);
    uint32_t tx_size = command == 24U ? SPI_BLOCK_TX : 8U;
    uint32_t timeout = command == 38U ? PL_SPI_TEST_ERASE_MS : PL_SPI_TEST_COMMAND_MS;
    pl_spi_result_t result;
    memset(out, 0, sizeof(*out));
    out->response = SPI_SENTINEL;
    if (mailbox == UINT32_MAX ||
        (command == 24U) != (write_block != NULL) ||
        (command == 17U) != (read_block_with_crc != NULL)) {
        return PL_SPI_BAD_ARGUMENT;
    }
    if ((command == 24U && !PL_SPI_TEST_ENABLE_WRITE) ||
        ((command == 32U || command == 33U || command == 38U) &&
         !PL_SPI_TEST_ENABLE_ERASE)) {
        return PL_SPI_DISABLED;
    }
    result = pl_spi_dma_reset(dev);
    if (result != PL_SPI_OK) {
        pl_spi_observe(dev, out);
        return result;
    }
    pl_spi_write(dev, SPI_PACKET, SPI_PACKET_VALUE(SPI_STREAM, 0U));
    pl_spi_write(dev, SPI_DELAY, 1U);
    pl_spi_write(dev, SPI_DIVIDER, PL_SPI_TEST_DATA_DIV);
    pl_spi_write(dev, SPI_CSR, SPI_START | SPI_READ);
    pl_spi_irq_prepare(dev);
    pl_spi_bram_write(dev, mailbox, SPI_SENTINEL);
    pl_spi_encode_command(command, argument, words);
    dev->tx_size = tx_size;
    pl_spi_bram_write(dev, SPI_TX_AREA - 4U, SPI_GUARD);
    pl_spi_bram_write(dev, SPI_TX_AREA, words[0]);
    pl_spi_bram_write(dev, SPI_TX_AREA + 4U, words[1]);
    if (write_block != NULL) {
        /* Payload words use the page's MSB-first transmit convention. The
         * CMD24 figure places CRC16 in the low half of the final DMA word.
         * Readback tests check this interpretation; no automatic byteswap
         * fallback is permitted if it disagrees with the selected hardware. */
        for (offset = 0U; offset < SPI_BLOCK_BYTES; offset += 4U) {
            uint32_t word = ((uint32_t)write_block[offset] << 24) |
                ((uint32_t)write_block[offset + 1U] << 16) |
                ((uint32_t)write_block[offset + 2U] << 8) | write_block[offset + 3U];
            pl_spi_bram_write(dev, SPI_TX_AREA + 8U + offset, word);
        }
        pl_spi_bram_write(dev, SPI_TX_AREA + 8U + SPI_BLOCK_BYTES,
                          UINT32_C(0xFFFF0000) | pl_spi_crc16(write_block, SPI_BLOCK_BYTES));
    }
    pl_spi_bram_write(dev, SPI_TX_AREA + rounded(tx_size), SPI_GUARD);
    if (read_block_with_crc != NULL) prepare_rx(dev, SPI_BLOCK_RX);
    dma_write(dev, DMA_TX_STATUS, DMA_IRQS);
    dma_write(dev, DMA_TX_CONTROL, DMA_RUN);
    dma_write(dev, DMA_TX_SOURCE, (uint32_t)dev->bram + SPI_TX_AREA);
    start = pl_spi_now_ms();
    dma_write(dev, DMA_TX_LENGTH, tx_size);
    pl_spi_write(dev, SPI_CSR, SPI_START | SPI_READ | SPI_SD_COMMAND_START);
    result = PL_SPI_TIMEOUT;
    do {
        pl_spi_observe(dev, out);
        out->response = pl_spi_bram_read(dev, mailbox);
        if (((out->tx_status | out->rx_status) & DMA_ERRORS) != 0U) {
            result = PL_SPI_DMA_ERROR;
            break;
        }
        if (dma_done(out->tx_status) &&
            (out->irq_seen & SPI_SD_IRQ_COMMAND) != 0U &&
            out->response != SPI_SENTINEL &&
            (read_block_with_crc == NULL || dma_done(out->rx_status))) {
            result = PL_SPI_OK;
            break;
        }
        pl_spi_delay_ms(1U);
    } while ((uint32_t)(pl_spi_now_ms() - start) < timeout);
    if (result == PL_SPI_OK) {
        pl_spi_delay_ms(1U);
        pl_spi_observe(dev, out);
    }
    out->elapsed_ms = pl_spi_now_ms() - start;
    if (read_block_with_crc != NULL) copy_rx(dev, read_block_with_crc, SPI_BLOCK_RX);
    return result;
}

pl_spi_result_t pl_spi_raw(pl_spi_t *dev, uint32_t mode, uint32_t count,
                           const uint32_t *words, size_t word_count,
                           bool read_enable, bool start_transfer,
                           pl_spi_observation_t *out)
{
    uint32_t start;
    size_t i;
    pl_spi_result_t result;
    memset(out, 0, sizeof(*out));
    if (words == NULL || word_count == 0U || word_count > 4U ||
        mode < SPI_SINGLE || mode > SPI_STREAM || count > 16U) {
        return PL_SPI_BAD_ARGUMENT;
    }
    pl_spi_core_reset(dev);
    result = pl_spi_dma_reset(dev);
    if (result != PL_SPI_OK) {
        pl_spi_observe(dev, out);
        return result;
    }
    pl_spi_write(dev, SPI_CSR, read_enable ? SPI_READ : 0U);
    pl_spi_write(dev, SPI_PACKET, SPI_PACKET_VALUE(mode, count));
    pl_spi_write(dev, SPI_DELAY, 1U);
    pl_spi_write(dev, SPI_DIVIDER, PL_SPI_TEST_INIT_DIV);
    /* Also arm S2MM in write-only tests to detect any unwanted receive data. */
    prepare_rx(dev, 64U);
    for (i = 0U; i < word_count; ++i) pl_spi_write(dev, SPI_TX_FIFO, words[i]);
    pl_spi_irq_prepare(dev);
    start = pl_spi_now_ms();
    if (start_transfer) {
        pl_spi_write(dev, SPI_CSR, SPI_START | (read_enable ? SPI_READ : 0U));
    }
    result = PL_SPI_TIMEOUT;
    do {
        pl_spi_observe(dev, out);
        if ((out->rx_status & DMA_ERRORS) != 0U) {
            result = PL_SPI_DMA_ERROR;
            break;
        }
        if (start_transfer &&
            (mode == SPI_STREAM || (out->irq_seen & SPI_DOC_IRQ_END) != 0U) &&
            (!read_enable || dma_done(out->rx_status))) {
            result = PL_SPI_OK;
            /* A write-only completion does not end the negative RX check:
             * observe the full interval to catch delayed/unwanted data. */
            if (read_enable) break;
        }
        pl_spi_delay_ms(1U);
    } while ((uint32_t)(pl_spi_now_ms() - start) < PL_SPI_TEST_RAW_MS);
    out->elapsed_ms = pl_spi_now_ms() - start;
    /* Inhibit is an observation interval, not a transfer with a completion. */
    if (!start_transfer && result == PL_SPI_TIMEOUT) result = PL_SPI_OK;
    return result;
}

const char *pl_spi_result_name(pl_spi_result_t result)
{
    switch (result) {
    case PL_SPI_OK: return "complete";
    case PL_SPI_TIMEOUT: return "timeout";
    case PL_SPI_DMA_ERROR: return "DMA error";
    case PL_SPI_BAD_ARGUMENT: return "bad argument";
    case PL_SPI_DISABLED: return "disabled";
    default: return "unknown";
    }
}

void pl_spi_close(pl_spi_t *dev)
{
    if (dev->connected) {
        XScuGic_Disable(&xInterruptController, dev->irq_id);
        (void)pl_spi_dma_reset(dev);
        pl_spi_core_reset(dev);
        XScuGic_Disconnect(&xInterruptController, dev->irq_id);
        dev->connected = false;
    }
}
