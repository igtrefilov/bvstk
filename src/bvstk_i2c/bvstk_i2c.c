#include "bvstk_i2c.h"

static QueueHandle_t q_master = NULL;
static QueueHandle_t q_slave  = NULL;
static SemaphoreHandle_t i2c_bus_mutex = NULL;
static volatile uint32_t g_master_wr_offset = 0;

typedef enum { MASTER_EVT_IRQ } master_evt_type_t;
typedef struct { master_evt_type_t type; uint32_t wr_offset; } master_evt_t;

typedef enum { SLAVE_EVT_FRAME } slave_evt_type_t;
typedef struct { slave_evt_type_t type; uint32_t size; } slave_evt_t;

uint8_t i2cdev_whitelist_bitmap[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1];
uint8_t i2cdev_blacklist_bitmap[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1];

static i2cdev_policy_t g_i2cdev_policy = I2CDEV_DEFAULT_POLICY;
static uint8_t i2cdev_reg_cache[I2CDEV_REG_COUNT];
static volatile uint8_t s_pending_reg = 0xFF;

static inline void reg_write32(uint32_t base, uint32_t ofs, uint32_t v) { Xil_Out32(base + ofs, v); }
static inline uint32_t reg_read32(uint32_t base, uint32_t ofs) { return Xil_In32(base + ofs); }

static void master_evt_task(void *arg);
static void slave_evt_task (void *arg);
static void master_ISR     (void *CallBackRef);
static void slave_ISR      (void *CallBackRef);
static inline void i2c_irq_enable(void) { vPortEnableInterrupt(IRQ_I2C_MASTER); vPortEnableInterrupt(IRQ_I2C_SLAVE); }

extern size_t xPortGetFreeHeapSize(void);
extern size_t xPortGetMinimumEverFreeHeapSize(void);

void i2cdev_policy_reset_defaults(void)
{
    for (uint32_t r = 0; r < I2CDEV_REG_COUNT; ++r) {
        for (uint32_t v = 0; v <= I2CDEV_MAX_VALUE_CODE; ++v) {
            i2cdev_whitelist_bitmap[r][v] = 0;
            i2cdev_blacklist_bitmap[r][v] = 0;
        }
    }
    i2cdev_whitelist_bitmap[0x13][16] = 1;
    i2cdev_whitelist_bitmap[0x13][17] = 1;
    i2cdev_whitelist_bitmap[0x13][18] = 1;
    i2cdev_whitelist_bitmap[0x13][19] = 1;
    g_i2cdev_policy = I2CDEV_DEFAULT_POLICY;
}

void i2cdev_set_policy(i2cdev_policy_t policy)
{
    g_i2cdev_policy = policy;
}

i2cdev_policy_t i2cdev_get_policy(void)
{
    return g_i2cdev_policy;
}

bool i2cdev_rule_allow(uint8_t reg, uint8_t val)
{
    if (reg >= I2CDEV_REG_COUNT) return false;
    if (val > I2CDEV_MAX_VALUE_CODE) return false;
    i2cdev_whitelist_bitmap[reg][val] = 1;
    return true;
}

bool i2cdev_rule_deny(uint8_t reg, uint8_t val)
{
    if (reg >= I2CDEV_REG_COUNT) return false;
    if (val > I2CDEV_MAX_VALUE_CODE) return false;
    i2cdev_blacklist_bitmap[reg][val] = 1;
    return true;
}

bool i2cdev_rule_clear(uint8_t reg, uint8_t val)
{
    if (reg >= I2CDEV_REG_COUNT) return false;
    if (val > I2CDEV_MAX_VALUE_CODE) return false;
    i2cdev_whitelist_bitmap[reg][val] = 0;
    i2cdev_blacklist_bitmap[reg][val] = 0;
    return true;
}

bool i2cdev_is_value_permitted_current(uint8_t reg, uint8_t val)
{
    if (reg >= I2CDEV_REG_COUNT) return false;
    if (val > I2CDEV_MAX_VALUE_CODE) return false;
    if (g_i2cdev_policy == I2CDEV_POLICY_WHITELIST) {
        return i2cdev_whitelist_bitmap[reg][val] != 0;
    } else {
        return i2cdev_blacklist_bitmap[reg][val] == 0;
    }
}

static inline void i2c_master_wait_ready(void)
{
    while ((reg_read32(I2C_MASTER_BASE, STATUS_OFFSET) & 0x1Fu) != 0u) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void start_i2c(void)
{
    xil_printf("Heap BEFORE I2C: %u, min ever: %u\r\n",
               (unsigned)xPortGetFreeHeapSize(),
               (unsigned)xPortGetMinimumEverFreeHeapSize());
    i2cdev_policy_reset_defaults();
    q_master = xQueueCreate(64, sizeof(master_evt_t));
    q_slave  = xQueueCreate(64, sizeof(slave_evt_t));
    i2c_bus_mutex = xSemaphoreCreateMutex();
    configASSERT(q_master);
    configASSERT(q_slave);
    configASSERT(i2c_bus_mutex);
    reg_write32(I2C_MASTER_BASE, IRQ_REG_OFFSET, 0x01);
    reg_write32(I2C_SLAVE_BASE,  IRQ_REG_OFFSET, 0x01);
    xPortInstallInterruptHandler(IRQ_I2C_MASTER, master_ISR, NULL);
    xPortInstallInterruptHandler(IRQ_I2C_SLAVE,  slave_ISR,  NULL);
    configASSERT(xTaskCreate(master_evt_task, "i2c_master_evt", I2C_TASK_STACK_SIZE, NULL, I2C_TASK_PRIORITY, NULL) == pdPASS);
    configASSERT(xTaskCreate(slave_evt_task,  "i2c_slave_evt",  I2C_TASK_STACK_SIZE, NULL, I2C_TASK_PRIORITY, NULL) == pdPASS);
    configASSERT(xTaskCreate(i2c_task,        "i2c_task",       I2C_TASK_STACK_SIZE, NULL, I2C_TASK_PRIORITY,       NULL) == pdPASS);
    xil_printf("Heap AFTER  I2C: %u, min ever: %u\r\n",
               (unsigned)xPortGetFreeHeapSize(),
               (unsigned)xPortGetMinimumEverFreeHeapSize());
}

void i2c_task(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (uint32_t i = 0; i < I2CDEV_REG_COUNT; ++i) i2cdev_reg_cache[i] = 0;
    i2cdev_init_full_scan();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void i2c_master_send(uint8_t addr_7b, uint8_t op_read, uint32_t num_bytes, const uint8_t *payload, uint32_t buf_size, uint8_t csr_bits)
{
    (void)buf_size;
    if (i2c_bus_mutex) xSemaphoreTake(i2c_bus_mutex, portMAX_DELAY);
    uint32_t nb = (num_bytes > 0xFFFFFFu) ? 0xFFFFFFu : (uint32_t)num_bytes;
    uint32_t header = I2C_MAKE_HEADER((uint8_t)(addr_7b & 0x7Fu), op_read ? 1u : 0u, nb);
    reg_write32(I2C_MASTER_BASE, TX_DATA_OFFSET, header);
    uint32_t full_words = nb / 4u;
    uint32_t tail_bytes = nb % 4u;
    const uint8_t *p = payload;
    for (uint32_t w = 0; w < full_words; ++w) {
        uint32_t v = ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        reg_write32(I2C_MASTER_BASE, TX_DATA_OFFSET, v);
        p += 4;
    }
    if (tail_bytes) {
        uint8_t b0 = (tail_bytes > 0) ? p[0] : 0;
        uint8_t b1 = (tail_bytes > 1) ? p[1] : 0;
        uint8_t b2 = (tail_bytes > 2) ? p[2] : 0;
        uint32_t v = ((uint32_t)b0) | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16);
        reg_write32(I2C_MASTER_BASE, TX_DATA_OFFSET, v);
    }
    i2c_master_wait_ready();
    reg_write32(I2C_MASTER_BASE, CSR_REG_OFFSET, (uint32_t)csr_bits);
    if (i2c_bus_mutex) xSemaphoreGive(i2c_bus_mutex);
}

static inline void i2cdev_write_byte(uint8_t reg, uint8_t val)
{
    uint8_t payload[2] = { reg, val };
    i2c_master_send(I2CDEV_I2C_ADDR_7B, 0, 2, payload, 2, CSR_START_BIT);
    i2cdev_reg_cache[reg] = val;
}

static void i2c_master_read_reg_to_bram(uint8_t addr7, uint8_t reg, uint32_t rd_len)
{
    uint32_t h1 = I2C_MAKE_HEADER((uint8_t)(addr7 & 0x7Fu), 0, 1u);
    reg_write32(I2C_MASTER_BASE, TX_DATA_OFFSET, h1);
    reg_write32(I2C_MASTER_BASE, TX_DATA_OFFSET, (uint32_t)reg);
    uint32_t h2 = I2C_MAKE_HEADER((uint8_t)(addr7 & 0x7Fu), 1, rd_len);
    reg_write32(I2C_MASTER_BASE, TX_DATA_OFFSET, h2);
    i2c_master_wait_ready();
    reg_write32(I2C_MASTER_BASE, CSR_REG_OFFSET, CSR_START_BIT | CSR_RP_START_BIT);
    s_pending_reg = reg;
}

static inline void i2cdev_read_byte_to_master_bram(uint8_t reg)
{
    i2c_master_read_reg_to_bram(I2CDEV_I2C_ADDR_7B, reg, 1u);
}

void i2cdev_init_full_scan(void)
{
    for (uint32_t i = 0; i < I2CDEV_REG_COUNT; ++i) {
        i2cdev_read_byte_to_master_bram((uint8_t)i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void slave_check_and_exec(const uint8_t *frame, uint32_t size)
{
    if (size == 0) return;
    uint8_t reg = frame[0];
    if (size >= 2) {
        if (reg < I2CDEV_REG_COUNT) {
            uint8_t val = frame[1];
            if (i2cdev_is_value_permitted_current(reg, val)) {
                i2cdev_write_byte(reg, val);
                xil_printf("[I2C][SLAVE] REG 0x%02x <- 0x%02x\n\r", reg, val);
            } else {
                xil_printf("[I2C][SLAVE] Reject value 0x%02x for REG 0x%02x\n\r", val, reg);
            }
        } else {
            xil_printf("[I2C][SLAVE] Reject REG 0x%02x\n\r", reg);
        }
    } else {
        if (reg < I2CDEV_REG_COUNT) {
            uint32_t out = (uint32_t)i2cdev_reg_cache[reg];
            reg_write32(BRAM_BASE_ADDR, I2C_BRAM_SLAVE_RD + 0x00, out);
            xil_printf("[I2C][SLAVE] REG 0x%02x -> 0x%02x\n\r", reg, (unsigned)out & 0xFF);
        } else {
            xil_printf("[I2C][SLAVE] Reject REG 0x%02x\n\r", reg);
        }
    }
}

static void master_ISR(void *CallBackRef)
{
    (void)CallBackRef;
    if (q_master == NULL) {
        reg_write32(I2C_MASTER_BASE, IRQ_REG_OFFSET, 0x01);
        return;
    }
    reg_write32(I2C_MASTER_BASE, IRQ_REG_OFFSET, 0x01);
    BaseType_t hpw = pdFALSE;
    master_evt_t evt = (master_evt_t){ .type = MASTER_EVT_IRQ, .wr_offset = 0 };
    (void)xQueueSendFromISR(q_master, &evt, &hpw);
    portYIELD_FROM_ISR(hpw);
}

static void slave_ISR(void *CallBackRef)
{
    (void)CallBackRef;
    if (q_slave == NULL) { reg_write32(I2C_SLAVE_BASE, IRQ_REG_OFFSET, 0x01); return; }
    uint32_t word0 = reg_read32(BRAM_BASE_ADDR, I2C_BRAM_SLAVE_WR);
    uint32_t size  = (uint32_t)I2C_HDR_NUM_BYTES(word0);
    reg_write32(I2C_SLAVE_BASE, IRQ_REG_OFFSET, 0x01);
    BaseType_t hpw = pdFALSE;
    slave_evt_t evt = (slave_evt_t){ .type = SLAVE_EVT_FRAME, .size = size };
    (void)xQueueSendFromISR(q_slave, &evt, &hpw);
    portYIELD_FROM_ISR(hpw);
}

static void master_evt_task(void *arg)
{
    (void)arg;
    static uint8_t once = 0;
    if (!once) { once = 1; i2c_irq_enable(); }
    master_evt_t evt;
    for (;;) {
        if (xQueueReceive(q_master, &evt, portMAX_DELAY) == pdTRUE) {
            uint32_t hdr = reg_read32(BRAM_BASE_ADDR, I2C_BRAM_MASTER + 0x00);
            uint32_t nb  = I2C_HDR_NUM_BYTES(hdr);
            uint8_t  op  = I2C_HDR_OP(hdr);
            if (op == 0x01 && nb > 0) {
                uint32_t src_ofs = I2C_BRAM_MASTER + 0x04;
                uint32_t dst_ofs = I2C_BRAM_SLAVE_RD + 0x00;
                uint32_t full_words = nb / 4u;
                uint32_t tail       = nb % 4u;
                for (uint32_t w = 0; w < full_words; ++w) {
                    uint32_t v = reg_read32(BRAM_BASE_ADDR, src_ofs);
                    src_ofs += 4;
                    reg_write32(BRAM_BASE_ADDR, dst_ofs, v);
                    dst_ofs += 4;
                }
                if (tail) {
                    uint32_t v = reg_read32(BRAM_BASE_ADDR, src_ofs);
                    uint8_t val = (uint8_t)(v & 0xFF);
                    if (s_pending_reg < I2CDEV_REG_COUNT) i2cdev_reg_cache[s_pending_reg] = val;
                }
            }
        }
    }
}

static void slave_evt_task(void *arg)
{
    (void)arg;
    slave_evt_t evt;
    for (;;) {
        if (xQueueReceive(q_slave, &evt, portMAX_DELAY) == pdTRUE) {
            if (evt.type == SLAVE_EVT_FRAME) {
                uint32_t size = evt.size;
                static uint8_t s_in[1024];
                if (size > sizeof(s_in)) size = sizeof(s_in);
                uint32_t byte_idx = 0;
                uint32_t bram_ofs = I2C_BRAM_SLAVE_WR + 0x04;
                while (byte_idx < size) {
                    uint32_t w = reg_read32(BRAM_BASE_ADDR, bram_ofs);
                    bram_ofs += 4;
                    for (int k = 0; k < 4 && byte_idx < size; ++k, ++byte_idx) {
                        s_in[byte_idx] = (uint8_t)((w >> (8*k)) & 0xFF);
                    }
                }
                if (size >= 1) slave_check_and_exec(&s_in[0], size);
            }
        }
    }
}

bool i2cdev_read_reg_cached(uint8_t reg, uint8_t *out_val)
{
    if (!out_val) return false;
    if (reg >= I2CDEV_REG_COUNT) return false;
    *out_val = i2cdev_reg_cache[reg];
    return true;
}

bool i2cdev_read_reg(uint8_t reg, uint8_t *out_val)
{
    if (!out_val) return false;
    if (reg >= I2CDEV_REG_COUNT) return false;
    i2cdev_read_byte_to_master_bram(reg);
    vTaskDelay(pdMS_TO_TICKS(2));
    *out_val = i2cdev_reg_cache[reg];
    return true;
}

bool i2cdev_write_reg(uint8_t reg, uint8_t val)
{
    if (reg >= I2CDEV_REG_COUNT) return false;
    if (!i2cdev_is_value_permitted_current(reg, val)) return false;
    i2cdev_write_byte(reg, val);
    return true;
}
