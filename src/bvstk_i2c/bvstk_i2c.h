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

typedef enum {
    I2CDEV_POLICY_WHITELIST = 0,
    I2CDEV_POLICY_BLACKLIST = 1
} i2cdev_policy_t;

typedef struct {
    uint8_t reg;
    uint8_t val;
} i2cdev_rule_entry_t;

typedef struct {
    uint8_t regs[64];
    size_t regs_len;
    uint32_t reg_delay_ms;
    uint32_t cycle_delay_ms;
    bool enabled;
} i2cdev_autopoll_profile_t;

#define I2CDEV_MAX_DEVICES     32u
#define I2CDEV_MAX_REG_COUNT   256u
#define I2CDEV_MAX_VALUE_CODE  64u

typedef struct {
    const char *name;
    uint8_t addr_7b;
    uint16_t reg_count;
    uint8_t max_value_code;
} i2cdev_device_info_t;

size_t i2cdev_device_count(void);
bool i2cdev_device_get_info(size_t idx, i2cdev_device_info_t *out);
bool i2cdev_select_device(const char *name);
bool i2cdev_select_device_addr(uint8_t addr_7b);
bool i2cdev_get_selected_info(i2cdev_device_info_t *out);

bool i2cdev_find_device_index_by_name(const char *name, size_t *out_idx);
bool i2cdev_find_device_index_by_addr(uint8_t addr_7b, size_t *out_idx);

void i2cdev_autopoll_get(i2cdev_autopoll_profile_t *out);
void i2cdev_autopoll_set(const i2cdev_autopoll_profile_t *p);

#define I2C_TASK_STACK_SIZE     (512U)
#define I2C_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)

#define CSR_REG_OFFSET        0x00
#define IRQ_REG_OFFSET        0x04
#define STATUS_OFFSET         0x08
#define TX_DATA_OFFSET        0x0C

/* BRAM window for I2C mailbox/buffer exchange. */
#define BRAM_BASE_ADDR        XPAR_AXI_BRAM_CTRL_1_S_AXI_BASEADDR

#define I2C_BRAM_MASTER       0x0500
#define I2C_BRAM_SLAVE_WR     0x0000
#define I2C_BRAM_SLAVE_RD     0x1000

#define I2C_MASTER_BASE       XPAR_SMI_MASTER_0_BASEADDR
#define I2C_SLAVE_BASE        XPAR_SMI_SLAVE_0_BASEADDR
#define IRQ_I2C_MASTER        XPAR_FABRIC_SMI_MASTER_0_IRQ_INTR
#define IRQ_I2C_SLAVE         XPAR_FABRIC_SMI_SLAVE_0_IRQ_INTR

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
void i2cdev_init_full_scan(void);
void slave_check_and_exec(const uint8_t *frame, uint32_t size, uint8_t op_read);
bool i2cdev_read_reg(uint8_t reg, uint8_t *out_val);
bool i2cdev_write_reg(uint8_t reg, uint8_t val);
bool i2cdev_read_reg_cached(uint8_t reg, uint8_t *out_val);

bool i2cdev_read_reg_dev(size_t dev_idx, uint8_t reg, uint8_t *out_val);
bool i2cdev_write_reg_dev(size_t dev_idx, uint8_t reg, uint8_t val);

void i2cdev_policy_reset_defaults(void);
void i2cdev_policy_clear_all(void);
void i2cdev_set_policy(i2cdev_policy_t policy);
i2cdev_policy_t i2cdev_get_policy(void);
bool i2cdev_is_value_permitted_current(uint8_t reg, uint8_t val);
bool i2cdev_rule_allow(uint8_t reg, uint8_t val);
bool i2cdev_rule_deny(uint8_t reg, uint8_t val);
bool i2cdev_rule_clear(uint8_t reg, uint8_t val);

bool i2cdev_set_policy_dev(size_t dev_idx, i2cdev_policy_t policy);
i2cdev_policy_t i2cdev_get_policy_dev(size_t dev_idx);
bool i2cdev_rule_allow_dev(size_t dev_idx, uint8_t reg, uint8_t val);
bool i2cdev_rule_deny_dev(size_t dev_idx, uint8_t reg, uint8_t val);
bool i2cdev_rule_clear_dev(size_t dev_idx, uint8_t reg, uint8_t val);

#endif
