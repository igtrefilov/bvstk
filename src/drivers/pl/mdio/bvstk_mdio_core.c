#include "drivers/pl/mdio/bvstk_mdio_core.h"
#include "hardware/pl/mdio/bvstk_mdio_regs.h"
#include <string.h>

enum { DATA_VALID = 1, BUSY = 2, TX_EMPTY = 4, TX_FULL = 8 };

static bvstk_status_t rd(const bvstk_mmio_region_t *r, size_t off, uint32_t *v)
{
    return bvstk_mmio_read32(r, off, v) == 0 ? BVSTK_OK : BVSTK_ERR_IO;
}
static bvstk_status_t wr(const bvstk_mmio_region_t *r, size_t off, uint32_t v)
{
    return bvstk_mmio_write32(r, off, v) == 0 ? BVSTK_OK : BVSTK_ERR_IO;
}
static size_t index_of(uint8_t phy, uint8_t reg)
{
    return (((size_t)phy << 5) | reg) * 4U;
}
/* Reading MEM_ADDR arms the acknowledgement in this version of the core. */
static bvstk_status_t ack(bvstk_mdio_core_t *c)
{
    uint32_t unused;
    if (rd(&c->master, BVSTK_MDIO_MSTR_MEM_ADDR_OFFSET, &unused) != BVSTK_OK)
        return BVSTK_ERR_IO;
    return wr(&c->master, BVSTK_MDIO_MSTR_IRQ_OFFSET, 1U);
}
static bvstk_status_t reset(bvstk_mdio_core_t *c)
{
    bvstk_status_t st = wr(&c->master, BVSTK_MDIO_MSTR_CSR_OFFSET, 1U);
    bvstk_clock_sleep_ms(&c->clock, 1U);
    return st == BVSTK_OK ? ack(c) : st;
}
bvstk_status_t bvstk_mdio_core_init(bvstk_mdio_core_t *c,
    const bvstk_clock_t *clock, const bvstk_mutex_t *mutex)
{
    if (!c) return BVSTK_ERR_MALFORMED;
    memset(c, 0, sizeof(*c));
    if (clock) c->clock = *clock;
    if (mutex) c->mutex = *mutex;
    if (bvstk_mmio_region_open(&c->master, BVSTK_MDIO_MASTER_BASE, BVSTK_MDIO_MASTER_SIZE))
        return BVSTK_ERR_IO;
    if (bvstk_mmio_region_open(&c->slave, BVSTK_MDIO_SLAVE_BASE, BVSTK_MDIO_SLAVE_SIZE)) {
        bvstk_mmio_region_close(&c->master);
        return BVSTK_ERR_IO;
    }
    if (bvstk_mmio_region_open(&c->bram, BVSTK_MDIO_BRAM_BASE, BVSTK_MDIO_BRAM_SIZE)) {
        bvstk_mmio_region_close(&c->slave);
        bvstk_mmio_region_close(&c->master);
        return BVSTK_ERR_IO;
    }
    c->initialized = 1U;
    if (reset(c) != BVSTK_OK) {
        bvstk_mdio_core_shutdown(c);
        return BVSTK_ERR_IO;
    }
    return BVSTK_OK;
}
void bvstk_mdio_core_shutdown(bvstk_mdio_core_t *c)
{
    if (!c) return;
    if (c->initialized) {
        bvstk_mmio_region_close(&c->bram);
        bvstk_mmio_region_close(&c->slave);
        bvstk_mmio_region_close(&c->master);
    }
    memset(c, 0, sizeof(*c));
}

static bvstk_status_t transfer(bvstk_mdio_core_t *c, uint8_t phy,
    uint8_t reg, uint16_t *value, int write, uint32_t timeout)
{
    uint32_t csr, addr, result;
    uint64_t start;
    bvstk_status_t st;
    size_t off;
    if (!c || !c->initialized) return BVSTK_ERR_NOT_READY;
    if (!value) return BVSTK_ERR_MALFORMED;
    if (phy > 31U || reg > 31U) return BVSTK_ERR_RANGE;
    if (!timeout) timeout = 100U;
    st = bvstk_mutex_lock(&c->mutex, timeout);
    if (st != BVSTK_OK) return st;
    start = bvstk_clock_now_ms(&c->clock);
    off = index_of(phy, reg);
    st = ack(c);
    if (st != BVSTK_OK) goto done;
    for (;;) {
        st = rd(&c->master, BVSTK_MDIO_MSTR_CSR_OFFSET, &csr);
        if (st != BVSTK_OK) goto done;
        if (!(csr & (BUSY | TX_FULL)) && (csr & TX_EMPTY)) break;
        if (bvstk_clock_now_ms(&c->clock) - start >= timeout) {
            st = BVSTK_ERR_TIMEOUT;
            goto done;
        }
        bvstk_clock_sleep_ms(&c->clock, 1U);
    }
    if (!write && wr(&c->bram, off, BVSTK_MDIO_MAILBOX_EMPTY) != BVSTK_OK) {
        st = BVSTK_ERR_IO;
        goto done;
    }
    st = wr(&c->master, BVSTK_MDIO_MSTR_TX_FIFO_OFFSET,
        ((uint32_t)write << 26) | ((uint32_t)phy << 21) |
        ((uint32_t)reg << 16) | (write ? *value : 0U));
    if (st != BVSTK_OK) goto done;
    /* Avoid observing idle before FIFO acceptance. At the supplied 500 kHz
     * a complete 64-bit Clause 22 frame takes 128 us. No PHY write ACK exists. */
    bvstk_clock_sleep_ms(&c->clock, 1U);
    for (;;) {
        st = rd(&c->master, BVSTK_MDIO_MSTR_CSR_OFFSET, &csr);
        if (st != BVSTK_OK) break;
        if (write && !(csr & BUSY) && (csr & TX_EMPTY)) break;
        if (!write && (csr & DATA_VALID)) {
            st = rd(&c->master, BVSTK_MDIO_MSTR_MEM_ADDR_OFFSET, &addr);
            if (st == BVSTK_OK) st = rd(&c->bram, off, &result);
            if (st == BVSTK_OK && (addr != BVSTK_MDIO_BRAM_BASE + off || result > 0xFFFFU))
                st = BVSTK_ERR_IO;
            if (st == BVSTK_OK) *value = (uint16_t)result;
            if (ack(c) != BVSTK_OK) st = BVSTK_ERR_IO;
            break;
        }
        if (bvstk_clock_now_ms(&c->clock) - start >= timeout) {
            st = BVSTK_ERR_TIMEOUT;
            break;
        }
        bvstk_clock_sleep_ms(&c->clock, 1U);
    }
done:
    /* Abort stale FIFO/results on error. Never retry a possibly destructive write. */
    if (st != BVSTK_OK) (void)reset(c);
    bvstk_mutex_unlock(&c->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_core_read(bvstk_mdio_core_t *c, uint8_t phy,
    uint8_t reg, uint16_t *value, uint32_t timeout)
{
    return transfer(c, phy, reg, value, 0, timeout);
}
bvstk_status_t bvstk_mdio_core_write(bvstk_mdio_core_t *c, uint8_t phy,
    uint8_t reg, uint16_t value, uint32_t timeout)
{
    return transfer(c, phy, reg, &value, 1, timeout);
}
bvstk_status_t bvstk_mdio_core_prepare_slave(bvstk_mdio_core_t *c)
{
    size_t i;
    if (!c || !c->initialized) return BVSTK_ERR_NOT_READY;
    for (i = 0U; i < 4096U; i += 4U) {
        if (wr(&c->bram, BVSTK_MDIO_SLAVE_WR_OFFSET + i, BVSTK_MDIO_MAILBOX_EMPTY) != BVSTK_OK ||
            wr(&c->bram, BVSTK_MDIO_SLAVE_RD_OFFSET + i, 0xFFFFU) != BVSTK_OK)
            return BVSTK_ERR_IO;
    }
    return bvstk_mdio_core_ack_slave(c);
}
bvstk_status_t bvstk_mdio_core_take_write(bvstk_mdio_core_t *c,
    uint8_t phy, uint8_t reg, uint16_t *value)
{
    uint32_t word;
    size_t off;
    if (!c || !c->initialized) return BVSTK_ERR_NOT_READY;
    if (!value || phy > 31U || reg > 31U) return BVSTK_ERR_RANGE;
    off = BVSTK_MDIO_SLAVE_WR_OFFSET + index_of(phy, reg);
    if (rd(&c->bram, off, &word) != BVSTK_OK) return BVSTK_ERR_IO;
    if (word & 0xFFFF0000U) return BVSTK_ERR_NOT_FOUND;
    /* A racing PL write may be coalesced/lost: the black-box has no FIFO. */
    if (wr(&c->bram, off, BVSTK_MDIO_MAILBOX_EMPTY) != BVSTK_OK) return BVSTK_ERR_IO;
    *value = (uint16_t)word;
    return BVSTK_OK;
}
bvstk_status_t bvstk_mdio_core_publish(bvstk_mdio_core_t *c,
    uint8_t phy, uint8_t reg, uint16_t value)
{
    if (!c || !c->initialized) return BVSTK_ERR_NOT_READY;
    if (phy > 31U || reg > 31U) return BVSTK_ERR_RANGE;
    return wr(&c->bram, BVSTK_MDIO_SLAVE_RD_OFFSET + index_of(phy, reg), value);
}
bvstk_status_t bvstk_mdio_core_ack_slave(bvstk_mdio_core_t *c)
{
    uint32_t unused;
    if (!c || !c->initialized) return BVSTK_ERR_NOT_READY;
    if (rd(&c->slave, BVSTK_MDIO_SLV_MEM_ADDR_OFFSET, &unused) != BVSTK_OK)
        return BVSTK_ERR_IO;
    return wr(&c->slave, BVSTK_MDIO_SLV_IRQ_OFFSET, 1U);
}
