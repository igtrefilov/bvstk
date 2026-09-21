#ifndef BVSTK_SD_MASTER_H
#define BVSTK_SD_MASTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware/pl/sd/bvstk_sd_master_regs.h"
#include "shared/base/bvstk_status.h"
#include "shared/interfaces/bvstk_clock.h"
#include "shared/interfaces/bvstk_mmio.h"

/* The port masks/acknowledges stale IRQs in prepare, arms the fabric IRQ,
 * and counts actual handler invocations. The ISR may mask the level input
 * until the next prepare. No command logic runs in interrupt context. */
typedef struct {
    void *context;
    bvstk_status_t (*prepare)(void *context);
    uint32_t (*count)(void *context);
    void (*disable)(void *context);
    void (*barrier)(void *context);
} bvstk_sd_master_events_t;

typedef struct {
    uintptr_t core_base, dma_base, bram_base;
    size_t bram_size;
    uint16_t init_divider, data_divider;
    bvstk_sd_master_events_t events;
    /* Opt-in for the measured AX7020 bitstream: CMD17 mailbox changes during
     * the payload and ends at 0xff (not a latched R1). Only accept this value
     * after fresh IRQ, both DMAs, exact length, guards and CRC16 all pass. */
    bool read_mailbox_is_stream;
    uint8_t read_descriptor_bytes; /* 8: diagram; 16: duplicated Tcl example. */
} bvstk_sd_master_config_t;

typedef struct {
    uint32_t csr, irq, irq_count, tx_status, rx_status, rx_length;
    uint32_t reply, first_reply, init_reply[SDM_INIT_WORDS], elapsed_ms;
    uint16_t crc_calculated, crc_received;
    uint8_t command;
    bool guards_ok, read_idle_reply;
    const char *stage;
} bvstk_sd_master_report_t;

typedef struct {
    bvstk_mmio_region_t core, dma, bram;
    bvstk_sd_master_config_t config;
    bvstk_clock_t clock;
    bool open, io_error;
} bvstk_sd_master_t;

/* Caller serializes the complete command/verification sequence. Config and
 * callbacks must remain valid until close. open maps memory only. */
bvstk_status_t bvstk_sd_master_open(bvstk_sd_master_t *driver,
    const bvstk_sd_master_config_t *config, const bvstk_clock_t *clock);
void bvstk_sd_master_close(bvstk_sd_master_t *driver);
bvstk_status_t bvstk_sd_master_initialize(bvstk_sd_master_t *driver,
    uint32_t timeout_ms, bvstk_sd_master_report_t *report);
bvstk_status_t bvstk_sd_master_command(bvstk_sd_master_t *driver,
    uint8_t command, uint32_t argument, const uint8_t *write_block,
    uint8_t *read_block, uint32_t timeout_ms, bvstk_sd_master_report_t *report);

uint8_t bvstk_sd_master_crc7(const uint8_t *data, size_t size);
uint16_t bvstk_sd_master_crc16(const uint8_t *data, size_t size);
void bvstk_sd_master_encode(uint8_t command, uint32_t argument, uint32_t words[2]);

#endif
