#ifndef BVSTK_SMI_H
#define BVSTK_SMI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xil_assert.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "../pl_common/bvstk_hw_config.h"
#include "../pl_common/bvstk_smi_regs.h"

#define SMI_TASK_STACK_SIZE     (1024U)
#define SMI_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)

#define MASTER_BASEADDR     BVSTK_SMI_MASTER_BASE
#define SLAVE_BASEADDR      BVSTK_SMI_SLAVE_BASE
#define BRAM_BASEADDR       BVSTK_SMI_BRAM_BASE
#define BRAM_HIGHADDR       (BVSTK_SMI_BRAM_BASE + BVSTK_SMI_BRAM_SIZE - 1U)

#define MASTER_WR_OFFSET    BVSTK_SMI_MASTER_WR_OFFSET
#define SLAVE_WR_OFFSET     BVSTK_SMI_SLAVE_WR_OFFSET
#define SLAVE_RD_OFFSET     BVSTK_SMI_SLAVE_RD_OFFSET

#define IRQ_MASTER          BVSTK_IRQ_SMI_MASTER
#define IRQ_SLAVE           BVSTK_IRQ_SMI_SLAVE
#define INTC_DEVICE_ID      XPAR_PS7_SCUGIC_0_DEVICE_ID

#define CSR_m       BVSTK_SMI_MSTR_CSR_OFFSET
#define TIMEOUT_m   BVSTK_SMI_MSTR_TIMEOUT_OFFSET
#define IRQ_m       BVSTK_SMI_MSTR_IRQ_OFFSET
#define TX_FIFO_m   BVSTK_SMI_MSTR_TX_FIFO_OFFSET
#define MEM_AADR_m  BVSTK_SMI_MSTR_MEM_ADDR_OFFSET

#define CSR_s       BVSTK_SMI_SLV_CSR_OFFSET
#define MEM_ADDR_s  BVSTK_SMI_SLV_MEM_ADDR_OFFSET
#define IRQ_s       BVSTK_SMI_SLV_IRQ_OFFSET
#define S2H         BVSTK_SMI_SLV_S2H_OFFSET

void start_smi(void);
void smi_task(void *pvParameters);
void mdio_write(uint8_t phy, uint8_t reg, uint16_t data);
void mdio_read (uint8_t phy, uint8_t reg);
bool smi_write_checked(uint8_t phy, uint8_t reg, uint16_t data);
bool smi_write_checked_source(uint8_t phy, uint8_t reg, uint16_t data, uint8_t source);
bool smi_read_blocking(uint8_t phy, uint8_t reg, uint16_t *out_value, TickType_t timeout_ticks);
void timeout_write(uint16_t timeout);
uint16_t timeout_read();
void smi_irq_install(void);

#endif
