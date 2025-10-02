#ifndef BVSTK_SMI_H
#define BVSTK_SMI_H

#include <stdint.h>
#include "xscugic.h"
#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xil_assert.h"
#include "xil_exception.h"

#define MASTER_BASEADDR     XPAR_SMI_MASTER_0_BASEADDR
#define SLAVE_BASEADDR      XPAR_SMI_SLAVE_0_BASEADDR
#define BRAM_BASEADDR       XPAR_BRAM_1_BASEADDR
#define BRAM_HIGHADDR       XPAR_BRAM_1_HIGHADDR

#define MASTER_WR_OFFSET    0x0000
#define SLAVE_WR_OFFSET     0x1000
#define SLAVE_RD_OFFSET     0x2000

#define IRQ_MASTER          XPAR_FABRIC_SMI_MASTER_0_IRQ_INTR
#define IRQ_SLAVE           XPAR_FABRIC_SMI_SLAVE_0_IRQ_INTR
#define INTC_DEVICE_ID      XPAR_PS7_SCUGIC_0_DEVICE_ID

// Сдвиги регистров
#define CSR_m       0x00
#define TIMEOUT_m   0x04
#define IRQ_m       0x08
#define TX_FIFO_m   0x0c
#define MEM_AADR_m  0x10

#define CSR_s       0x00
#define MEM_ADDR_s  0x04
#define IRQ_s       0x08
#define S2H         0x0c


// ====== Публичные переменные (если они нужны в main) ======
extern uint8_t  phy_addr;

extern volatile uint32_t bram_master_data;
extern volatile uint32_t bram_master_addr;

extern volatile uint32_t bram_slave_data;
extern volatile uint32_t bram_slave_addr;

extern volatile uint32_t slave2host_data;

// ====== Публичные функции модуля ======
void start_smi(void);
int  SetupInterruptSystem(XScuGic *p_intc_inst);
void mdio_write(uint8_t phy, uint8_t reg, uint16_t data);
void mdio_read (uint8_t phy, uint8_t reg);
void timeout_write(uint16_t timeout);
uint16_t timeout_read();

#endif // BVSTK_SMI_H
