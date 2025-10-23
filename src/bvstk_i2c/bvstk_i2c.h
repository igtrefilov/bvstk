#ifndef BVSTK_I2C_H
#define BVSTK_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "xparameters.h"
#include "xil_io.h"
#include "xil_types.h"
#include "xil_assert.h"
#include "xil_printf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "axp15060_pmic.h"

#define I2C_TASK_STACK_SIZE     (1024U)
#define I2C_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)

#define CSR_REG_OFFSET        0x00
#define IRQ_REG_OFFSET        0x04
#define STATUS_OFFSET         0x08
#define TX_DATA_OFFSET        0x0C

#define BRAM_BASE_ADDR        XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR
#define I2C_BRAM_MASTER       0x0500
#define I2C_BRAM_SLAVE_WR     0x0000
#define I2C_BRAM_SLAVE_RD     0x1000

#define I2C_MASTER_BASE       XPAR_I2C_MASTER_0_BASEADDR
#define I2C_SLAVE_BASE        XPAR_I2C_SLAVE_0_BASEADDR

#define IRQ_I2C_MASTER        XPAR_FABRIC_I2C_MASTER_0_IRQ_INTR
#define IRQ_I2C_SLAVE         XPAR_FABRIC_I2C_SLAVE_0_IRQ_INTR

#define CSR_START_BIT    (1u << 0)
#define CSR_RP_START_BIT (1u << 1)

static inline uint32_t I2C_MAKE_HEADER(uint8_t addr7, uint8_t op_read, uint32_t num_bytes)
{
    return (((uint32_t)(num_bytes & 0xFFFFFFu)) << 8) | (((uint32_t)(op_read ? 1u : 0u)) << 7) | ((uint32_t)(addr7 & 0x7Fu));
}
static inline uint32_t I2C_HDR_NUM_BYTES(uint32_t hdr) { return (uint32_t)((hdr >> 8) & 0xFFFFFFu); }
static inline uint8_t  I2C_HDR_OP(uint32_t hdr)        { return (uint8_t)((hdr >> 7) & 0x01u); }
static inline uint8_t  I2C_HDR_ADDR(uint32_t hdr)      { return (uint8_t)(hdr & 0x7Fu); }

void start_i2c(void);
void i2c_task(void *pvParameters);
void i2c_master_send(uint8_t addr_7b, uint8_t op_read, uint32_t num_bytes, const uint8_t *payload, uint32_t buf_size, uint8_t csr_bits);
void pmic_init_full_scan(void);
void slave_check_and_exec(const uint8_t *frame, uint32_t size);

#endif
