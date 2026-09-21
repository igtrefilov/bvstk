#include "drivers/pl/sd/bvstk_sd_master.h"

#include <string.h>

#define SENTINEL UINT32_C(0xa5963cc3)
#define GUARD UINT32_C(0x69c35aa5)

static uint32_t rd(bvstk_sd_master_t *d, const bvstk_mmio_region_t *r, size_t off)
{
    uint32_t value = 0;
    if (bvstk_mmio_read32(r, off, &value) != 0) d->io_error = true;
    return value;
}

static void wr(bvstk_sd_master_t *d, const bvstk_mmio_region_t *r,
               size_t off, uint32_t value)
{
    if (!d->io_error && bvstk_mmio_write32(r, off, value) != 0) d->io_error = true;
    d->config.events.barrier(d->config.events.context);
}

static uint32_t elapsed(bvstk_sd_master_t *d, uint64_t start)
{
    return (uint32_t)(bvstk_clock_now_ms(&d->clock) - start);
}

static bvstk_status_t dma_reset(bvstk_sd_master_t *d)
{
    uint64_t start = bvstk_clock_now_ms(&d->clock);
    wr(d, &d->dma, SDM_TX_CONTROL, SDM_DMA_RESET);
    do {
        uint32_t controls = rd(d, &d->dma, SDM_TX_CONTROL) |
                            rd(d, &d->dma, SDM_RX_CONTROL);
        if (d->io_error) return BVSTK_ERR_IO;
        if ((controls & SDM_DMA_RESET) == 0) {
            wr(d, &d->dma, SDM_TX_STATUS, SDM_DMA_IRQS);
            wr(d, &d->dma, SDM_RX_STATUS, SDM_DMA_IRQS);
            return d->io_error ? BVSTK_ERR_IO : BVSTK_OK;
        }
        bvstk_clock_sleep_ms(&d->clock, 1);
    } while (elapsed(d, start) < 100);
    return BVSTK_ERR_TIMEOUT;
}

static void abort_transfer(bvstk_sd_master_t *d)
{
    d->config.events.disable(d->config.events.context);
    /* Preserve the report before recovery. Never retry a media write. */
    d->io_error = false;
    (void)dma_reset(d);
    wr(d, &d->core, SDM_CSR, SDM_RESET);
}

static void snapshot(bvstk_sd_master_t *d, bvstk_sd_master_report_t *r)
{
    r->csr = rd(d, &d->core, SDM_CSR);
    r->irq |= rd(d, &d->core, SDM_IRQ);
    r->irq_count = d->config.events.count(d->config.events.context);
    r->tx_status = rd(d, &d->dma, SDM_TX_STATUS);
    r->rx_status = rd(d, &d->dma, SDM_RX_STATUS);
    r->rx_length = rd(d, &d->dma, SDM_RX_LENGTH);
}

static bool dma_done(uint32_t status)
{
    return (status & (SDM_DMA_IOC | SDM_DMA_IDLE | SDM_DMA_HALTED |
                      SDM_DMA_ERRORS)) == (SDM_DMA_IOC | SDM_DMA_IDLE);
}

uint8_t bvstk_sd_master_crc7(const uint8_t *data, size_t size)
{
    uint8_t crc = 0;
    size_t i;
    unsigned bit;
    for (i = 0; i < size; ++i) {
        uint8_t byte = data[i];
        for (bit = 0; bit < 8; ++bit) {
            crc <<= 1;
            if ((byte ^ crc) & 0x80U) crc ^= 0x09U;
            byte <<= 1;
        }
    }
    return (uint8_t)((crc << 1) | 1U);
}

uint16_t bvstk_sd_master_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0;
    size_t i;
    unsigned bit;
    for (i = 0; i < size; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0));
    }
    return crc;
}

void bvstk_sd_master_encode(uint8_t command, uint32_t argument, uint32_t words[2])
{
    uint8_t frame[5] = {(uint8_t)(0x40U | command), (uint8_t)(argument >> 24),
        (uint8_t)(argument >> 16), (uint8_t)(argument >> 8), (uint8_t)argument};
    /* CMD16 Tcl example: argument 512 occupies the word 00020000. */
    words[0] = ((argument & 0xffU) << 24) | ((argument & 0xff00U) << 8) |
               ((argument >> 8) & 0xff00U) | (argument >> 24);
    words[1] = frame[0] | ((uint32_t)bvstk_sd_master_crc7(frame, 5) << 8);
}

bvstk_status_t bvstk_sd_master_open(bvstk_sd_master_t *d,
    const bvstk_sd_master_config_t *cfg, const bvstk_clock_t *clock)
{
    if (!d || !cfg || !cfg->events.prepare || !cfg->events.count ||
        !cfg->events.disable || !cfg->events.barrier ||
        cfg->bram_size < SDM_BRAM_MIN || cfg->bram_base > UINT32_MAX - SDM_BRAM_MIN ||
        ((cfg->core_base | cfg->dma_base | cfg->bram_base) & 3U) ||
        cfg->init_divider < 250 || (cfg->init_divider & 1U) ||
        (cfg->read_descriptor_bytes != 8 && cfg->read_descriptor_bytes != 16) ||
        cfg->data_divider < 2 || (cfg->data_divider & 1U)) return BVSTK_ERR_RANGE;
    memset(d, 0, sizeof(*d));
    d->config = *cfg;
    if (clock) d->clock = *clock;
    if (bvstk_mmio_region_open(&d->core, cfg->core_base, 0x10000) ||
        bvstk_mmio_region_open(&d->dma, cfg->dma_base, 0x10000) ||
        bvstk_mmio_region_open(&d->bram, cfg->bram_base, cfg->bram_size)) {
        bvstk_mmio_region_close(&d->bram);
        bvstk_mmio_region_close(&d->dma);
        bvstk_mmio_region_close(&d->core);
        return BVSTK_ERR_IO;
    }
    d->open = true;
    return BVSTK_OK;
}

void bvstk_sd_master_close(bvstk_sd_master_t *d)
{
    if (!d || !d->open) return;
    abort_transfer(d);
    bvstk_mmio_region_close(&d->bram);
    bvstk_mmio_region_close(&d->dma);
    bvstk_mmio_region_close(&d->core);
    d->open = false;
}

static bool init_valid(const uint32_t *m)
{
    size_t i;
    for (i = 0; i < SDM_INIT_WORDS; ++i) if (m[i] == SENTINEL) return false;
    return m[0] == 1 && m[1] == 0x1aa && m[2] == 1 && m[3] == 0 &&
           m[4] <= 1 && m[6] == 0 && (m[5] & UINT32_C(0x80000000)) &&
           (m[5] & UINT32_C(0x00ff8000));
}

bvstk_status_t bvstk_sd_master_initialize(bvstk_sd_master_t *d,
    uint32_t timeout_ms, bvstk_sd_master_report_t *r)
{
    bvstk_status_t result;
    uint64_t start;
    size_t i;
    if (!d || !d->open || !r) return BVSTK_ERR_NOT_READY;
    memset(r, 0, sizeof(*r));
    d->io_error = false;
    r->stage = "DMA reset";
    d->config.events.disable(d->config.events.context);
    result = dma_reset(d);
    if (result != BVSTK_OK) goto failed;
    wr(d, &d->core, SDM_CSR, SDM_RESET);
    bvstk_clock_sleep_ms(&d->clock, 2);
    for (i = 0; i < SDM_INIT_WORDS; ++i) wr(d, &d->bram, i * 4, SENTINEL);
    wr(d, &d->core, SDM_PACKET, SDM_FALL_THROUGH);
    wr(d, &d->core, SDM_DELAY, 1);
    wr(d, &d->core, SDM_DIVIDER, d->config.init_divider);
    wr(d, &d->core, SDM_CSR, SDM_START_READ);
    r->stage = "initialization";
    result = d->config.events.prepare(d->config.events.context);
    if (result != BVSTK_OK) goto failed;
    start = bvstk_clock_now_ms(&d->clock);
    wr(d, &d->core, SDM_INIT, SDM_INIT_VALUE);
    result = BVSTK_ERR_TIMEOUT;
    do {
        snapshot(d, r);
        for (i = 0; i < SDM_INIT_WORDS; ++i) r->init_reply[i] = rd(d, &d->bram, i * 4);
        if (d->io_error) { result = BVSTK_ERR_IO; break; }
        /* The final OCR reply, not an early hardware-clock IRQ, establishes
         * card readiness. Every word must have replaced its sentinel. */
        if (r->irq_count && init_valid(r->init_reply)) { result = BVSTK_OK; break; }
        bvstk_clock_sleep_ms(&d->clock, 1);
    } while (elapsed(d, start) < (timeout_ms ? timeout_ms : 20000));
    r->elapsed_ms = elapsed(d, start);
    if (result == BVSTK_OK) { r->stage = "initialized"; return result; }
failed:
    snapshot(d, r);
    abort_transfer(d);
    return result;
}

static bool guards_valid(bvstk_sd_master_t *d, uint32_t tx_size, bool receive)
{
    bool ok = rd(d, &d->bram, SDM_TX_AREA - 4) == GUARD &&
        rd(d, &d->bram, SDM_TX_AREA + ((tx_size + 3) & ~3U)) == GUARD;
    if (receive) {
        ok = ok && rd(d, &d->bram, SDM_RX_AREA - 4) == GUARD &&
            rd(d, &d->bram, SDM_RX_AREA + 516) == GUARD &&
            (rd(d, &d->bram, SDM_RX_AREA + 512) & UINT32_C(0xffff0000)) ==
                (SENTINEL & UINT32_C(0xffff0000));
    }
    return ok;
}

bvstk_status_t bvstk_sd_master_command(bvstk_sd_master_t *d,
    uint8_t command, uint32_t argument, const uint8_t *write_block,
    uint8_t *read_block, uint32_t timeout_ms, bvstk_sd_master_report_t *r)
{
    uint32_t words[2], mailbox, off, size = command == 24 ? SDM_TX_BYTES : 8;
    uint64_t start;
    bvstk_status_t result;
    uint8_t received[SDM_RX_BYTES];
    if (!d || !d->open || !r) return BVSTK_ERR_NOT_READY;
    memset(r, 0, sizeof(*r));
    r->command = command;
    r->first_reply = SENTINEL;
    r->stage = "arguments";
    switch (command) {
    case 13: mailbox = SDM_MB_CMD13; break;
    case 16: mailbox = SDM_MB_CMD16; break;
    case 17: mailbox = SDM_MB_CMD17; break;
    case 24: mailbox = SDM_MB_CMD24; break;
    default: return BVSTK_ERR_UNSUPPORTED;
    }
    if ((command == 24) != (write_block != NULL) ||
        (command == 17) != (read_block != NULL)) return BVSTK_ERR_MALFORMED;
    if (command == 17) size = d->config.read_descriptor_bytes;
    d->io_error = false;
    r->stage = "DMA reset";
    result = dma_reset(d);
    if (result != BVSTK_OK) goto failed;
    wr(d, &d->core, SDM_PACKET, SDM_FALL_THROUGH);
    wr(d, &d->core, SDM_DELAY, 1);
    wr(d, &d->core, SDM_DIVIDER, d->config.data_divider);
    /* The documented repeated-CMD17 flow only writes CSR=0x0e for the next
     * request. CSR=0x06 belongs to initialization, not between commands. */
    result = d->config.events.prepare(d->config.events.context);
    if (result != BVSTK_OK) goto failed;
    wr(d, &d->bram, mailbox, SENTINEL);
    bvstk_sd_master_encode(command, argument, words);
    wr(d, &d->bram, SDM_TX_AREA - 4, GUARD);
    wr(d, &d->bram, SDM_TX_AREA, words[0]);
    wr(d, &d->bram, SDM_TX_AREA + 4, words[1]);
    if (command == 17 && size == 16) {
        wr(d, &d->bram, SDM_TX_AREA + 8, words[0]);
        wr(d, &d->bram, SDM_TX_AREA + 12, (words[1] & 0xffU) | 0xff00U);
    }
    if (write_block) {
        for (off = 0; off < SDM_BLOCK_SIZE; off += 4) {
            uint32_t w = ((uint32_t)write_block[off] << 24) |
                ((uint32_t)write_block[off + 1] << 16) |
                ((uint32_t)write_block[off + 2] << 8) | write_block[off + 3];
            wr(d, &d->bram, SDM_TX_AREA + 8 + off, w);
        }
        wr(d, &d->bram, SDM_TX_AREA + 520,
           UINT32_C(0xffff0000) | bvstk_sd_master_crc16(write_block, SDM_BLOCK_SIZE));
    }
    wr(d, &d->bram, SDM_TX_AREA + ((size + 3) & ~3U), GUARD);
    if (read_block) {
        wr(d, &d->bram, SDM_RX_AREA - 4, GUARD);
        for (off = 0; off < 516; off += 4) wr(d, &d->bram, SDM_RX_AREA + off, SENTINEL);
        wr(d, &d->bram, SDM_RX_AREA + 516, GUARD);
        /* Arm receive before submitting the command. */
        wr(d, &d->dma, SDM_RX_CONTROL, SDM_DMA_RUN);
        wr(d, &d->dma, SDM_RX_DESTINATION, (uint32_t)d->config.bram_base + SDM_RX_AREA);
        wr(d, &d->dma, SDM_RX_LENGTH, SDM_RX_BYTES);
    }
    wr(d, &d->dma, SDM_TX_CONTROL, SDM_DMA_RUN);
    wr(d, &d->dma, SDM_TX_SOURCE, (uint32_t)d->config.bram_base + SDM_TX_AREA);
    start = bvstk_clock_now_ms(&d->clock);
    wr(d, &d->dma, SDM_TX_LENGTH, size);
    wr(d, &d->core, SDM_CSR, SDM_START_COMMAND);
    r->stage = "command completion";
    result = BVSTK_ERR_TIMEOUT;
    do {
        snapshot(d, r);
        r->reply = rd(d, &d->bram, mailbox);
        if (r->first_reply == SENTINEL && r->reply != SENTINEL) r->first_reply = r->reply;
        if (d->io_error || ((r->tx_status | r->rx_status) & SDM_DMA_ERRORS)) {
            result = BVSTK_ERR_IO;
            break;
        }
        if (r->irq_count && r->reply != SENTINEL && dma_done(r->tx_status) &&
            (!read_block || dma_done(r->rx_status))) { result = BVSTK_OK; break; }
        bvstk_clock_sleep_ms(&d->clock, 1);
    } while (elapsed(d, start) < (timeout_ms ? timeout_ms : 5000));
    r->elapsed_ms = elapsed(d, start);
    if (result != BVSTK_OK) goto failed;
    r->stage = "DMA bounds";
    r->guards_ok = guards_valid(d, size, read_block != NULL);
    if (!r->guards_ok || (read_block && r->rx_length != SDM_RX_BYTES) || d->io_error) {
        result = BVSTK_ERR_IO;
        goto failed;
    }
    if (read_block) {
        uint32_t word = 0;
        for (off = 0; off < SDM_RX_BYTES; ++off) {
            if ((off & 3U) == 0) word = rd(d, &d->bram, SDM_RX_AREA + off);
            received[off] = (uint8_t)(word >> ((off & 3U) * 8));
        }
        r->stage = "CRC16";
        r->crc_calculated = bvstk_sd_master_crc16(received, SDM_BLOCK_SIZE);
        r->crc_received = ((uint16_t)received[512] << 8) | received[513];
        if (d->io_error || r->crc_calculated != r->crc_received) {
            result = BVSTK_ERR_IO;
            goto failed;
        }
    }
    r->stage = "card response";
    if (r->reply != 0) {
        if (command == 17 && d->config.read_mailbox_is_stream && r->reply == 0xff)
            r->read_idle_reply = true;
        else { result = BVSTK_ERR_IO; goto failed; }
    }
    if (read_block) memcpy(read_block, received, SDM_BLOCK_SIZE);
    r->stage = "complete";
    return BVSTK_OK;
failed:
    snapshot(d, r);
    abort_transfer(d);
    return result;
}
