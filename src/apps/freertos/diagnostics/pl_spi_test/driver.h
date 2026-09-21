#ifndef PL_SPI_TEST_DRIVER_H
#define PL_SPI_TEST_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "apps/freertos/diagnostics/pl_spi_test/regs.h"

typedef enum {
    PL_SPI_OK, PL_SPI_TIMEOUT, PL_SPI_DMA_ERROR,
    PL_SPI_BAD_ARGUMENT, PL_SPI_DISABLED
} pl_spi_result_t;

typedef struct {
    uintptr_t core, dma, bram;
    uint32_t bram_size, irq_id;
    volatile uint32_t irq_count, irq_latched;
    uint32_t rx_capacity, tx_size;
    bool connected;
} pl_spi_t;

typedef struct {
    uint32_t csr, irq, irq_seen, irq_count;
    uint32_t tx_control, tx_status, rx_control, rx_status, rx_length;
    uint32_t response, mailbox[SPI_INIT_WORDS], elapsed_ms;
    bool guards_ok;
} pl_spi_observation_t;

/* Call open from a running FreeRTOS task: the BSP has already set up GIC. */
bool pl_spi_open(pl_spi_t *dev);
void pl_spi_close(pl_spi_t *dev);
uint32_t pl_spi_read(const pl_spi_t *dev, uint32_t offset);
void pl_spi_write(const pl_spi_t *dev, uint32_t offset, uint32_t value);
uint32_t pl_spi_dma_read(const pl_spi_t *dev, uint32_t offset);
uint32_t pl_spi_bram_read(const pl_spi_t *dev, uint32_t offset);
void pl_spi_bram_write(const pl_spi_t *dev, uint32_t offset, uint32_t value);
uint32_t pl_spi_now_ms(void);
void pl_spi_delay_ms(uint32_t ms);
void pl_spi_core_reset(pl_spi_t *dev);
pl_spi_result_t pl_spi_dma_reset(pl_spi_t *dev);
void pl_spi_irq_prepare(pl_spi_t *dev);
void pl_spi_observe(pl_spi_t *dev, pl_spi_observation_t *out);
const char *pl_spi_result_name(pl_spi_result_t result);

pl_spi_result_t pl_spi_initialize_card(pl_spi_t *dev,
                                      pl_spi_observation_t *out);
/* This function never resets/reinitializes SPI between SD commands. It waits
 * separately for fresh DMA IOC, a fresh mailbox reply and SD command done. */
pl_spi_result_t pl_spi_command(pl_spi_t *dev, uint8_t command,
                               uint32_t argument, const uint8_t *write_block,
                               uint8_t *read_block_with_crc,
                               pl_spi_observation_t *out);

/* Legacy documented FIFO vectors, observed entirely through PS interfaces.
 * RX capacity exceeds the requested count to expose unexpected extra bytes. */
pl_spi_result_t pl_spi_raw(pl_spi_t *dev, uint32_t mode, uint32_t count,
                           const uint32_t *words, size_t word_count,
                           bool read_enable, bool start,
                           pl_spi_observation_t *out);

uint8_t pl_spi_crc7(const uint8_t *bytes, size_t size);
uint16_t pl_spi_crc16(const uint8_t *bytes, size_t size);
uint32_t pl_spi_crc32(const uint8_t *bytes, size_t size);
void pl_spi_encode_command(uint8_t command, uint32_t argument,
                            uint32_t words[2]);

#endif
