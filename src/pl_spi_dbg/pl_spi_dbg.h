#ifndef PL_SPI_DBG_H
#define PL_SPI_DBG_H

#include <stdbool.h>
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

#define PL_SPI_DBG_IRQ_WR_DISPEL_BIT     0U

#define PL_SPI_DBG_MODE_MULTI            0x2U

typedef struct {
    uint32_t base_addr;
    uint32_t bram_base_addr;
    uint8_t mode;
    bool read_en;
} pl_spi_dbg_ctx_t;

/* Debug module public entrypoints. */
void start_pl_spi_dbg(void);
bool pl_spi_dbg_transfer(void);

#endif
