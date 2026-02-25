#ifndef BVSTK_SPI_H
#define BVSTK_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xparameters.h"
#include "xil_io.h"
#include "xil_types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SPI_TASK_STACK_SIZE     (768U)
#define SPI_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)

#define SPI_BASEADDR            XPAR_SPI_MASTER_0_BASEADDR
#define SPI_BRAM_BASEADDR       XPAR_AXI_BRAM_CTRL_2_S_AXI_BASEADDR
#define SPI_IRQ_INTR            XPAR_FABRIC_SPI_MASTER_0_IRQ_INTR
#define SPI_HAS_IRQ             1

/* SPI register map. */
#define SPI_CSR_REG_OFFSET      0x00U
#define SPI_IRQ_REG_OFFSET      0x04U
#define SPI_PACKET_OFFSET       0x08U
#define SPI_TIMEOUT_OFFSET      0x0CU
#define SPI_TX_FIFO_OFFSET      0x10U
#define SPI_SD_INI_OFFSET       0x14U
#define SPI_SIG_REG_OFFSET      0x18U

#define SPI_MODE_SINGLE         0x1U
#define SPI_MODE_MULTI          0x2U
#define SPI_MODE_FALLTHROUGH    0x3U

typedef struct {
    uint8_t packets_mode;     /* 01 single, 10 multi, 11 fall-through */
    uint32_t timeout_ticks;   /* >= 1 */
    uint16_t p_clk_div;       /* >= 2 and even */
    bool read_en;             /* true: full duplex */
} spi_runtime_cfg_t;

void start_spi(void);

void spi_set_cfg(const spi_runtime_cfg_t *cfg);
void spi_get_cfg(spi_runtime_cfg_t *out);

bool spi_transfer_words(const uint32_t *tx_words,
                        size_t tx_count,
                        uint32_t *rx_words,
                        size_t rx_capacity,
                        TickType_t timeout_ticks);

#endif
