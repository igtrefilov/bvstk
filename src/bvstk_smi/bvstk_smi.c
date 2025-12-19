#include "bvstk_smi.h"

#include "../config/config_store.h"

uint32_t bram_master_data = 0;
uint32_t bram_master_addr = 0;
uint32_t bram_slave_data  = 0;
uint32_t bram_slave_addr  = 0;
uint32_t slave2host_data  = 0;

typedef enum { MASTER_EVT_OVERFLOW, MASTER_EVT_DATA } master_evt_type_t;
typedef struct { master_evt_type_t type; uint32_t csr; uint32_t mem_addr; } master_evt_t;
typedef enum { SLAVE_EVT_HOST_WRITE, SLAVE_EVT_HOST_READ } slave_evt_type_t;
typedef struct { slave_evt_type_t type; uint32_t csr; uint32_t bram_addr; uint32_t s2h; } slave_evt_t;

static QueueHandle_t q_master = NULL;
static QueueHandle_t q_slave  = NULL;
static QueueHandle_t q_s2h_evt = NULL;
static SemaphoreHandle_t smi_bus_mutex = NULL;

static volatile uint8_t s2h_filter_en = 0;
static volatile uint8_t s2h_filter_phy = 0;
static volatile uint8_t s2h_filter_reg = 0;

static int u8_list_contains(const uint8_t *a, size_t len, uint8_t v)
{
    if (!a) return 0;
    for (size_t i = 0; i < len; ++i) {
        if (a[i] == v) return 1;
    }
    return 0;
}

static bool smi_is_write_allowed(uint8_t phy, uint8_t reg)
{
    const smi_phy_config_t *cfg = config_store_find_smi_device_by_phy(phy);
    if (!cfg) return true; /* No config => keep legacy behaviour. */
    if (reg >= cfg->reg_count) return false;
    if (cfg->policy == SMI_POLICY_WHITELIST) {
        return u8_list_contains(cfg->write_allow_regs, cfg->write_allow_regs_len, reg) != 0;
    }
    return u8_list_contains(cfg->write_deny_regs, cfg->write_deny_regs_len, reg) == 0;
}

static void master_evt_task(void *arg);
static void slave_evt_task (void *arg);
static void master_ISR     (void *CallBackRef);
static void slave_ISR      (void *CallBackRef);
static inline void smi_irq_enable(void) { vPortEnableInterrupt(IRQ_MASTER); vPortEnableInterrupt(IRQ_SLAVE); }

void start_smi(void)
{
    q_master = xQueueCreate(128, sizeof(master_evt_t));
    q_slave  = xQueueCreate(128, sizeof(slave_evt_t));
    q_s2h_evt = xQueueCreate(1, sizeof(uint32_t));
    smi_bus_mutex = xSemaphoreCreateMutex();

    configASSERT(q_master);
    configASSERT(q_slave);
    configASSERT(q_s2h_evt);
    configASSERT(smi_bus_mutex);

    Xil_Out32(MASTER_BASEADDR + IRQ_m, 1U);
    Xil_Out32(SLAVE_BASEADDR  + IRQ_s, 1U);

    smi_irq_install();

    configASSERT(xTaskCreate(master_evt_task, "master_evt_task", SMI_TASK_STACK_SIZE, NULL, SMI_TASK_PRIORITY + 1U, NULL) == pdPASS);
    configASSERT(xTaskCreate(slave_evt_task,  "slave_evt_task",  SMI_TASK_STACK_SIZE, NULL, SMI_TASK_PRIORITY + 1U, NULL) == pdPASS);

    if (xTaskCreate(smi_task, "smi_task", SMI_TASK_STACK_SIZE, NULL, SMI_TASK_PRIORITY, NULL) != pdPASS) {
        xil_printf("Error creating SMI task\n\r");
    }
}

void smi_task(void *pvParameters)
{
    (void) pvParameters;

    vTaskDelay(pdMS_TO_TICKS(200));
    timeout_write(4321);
    (void)timeout_read();

    bool settings_applied = false;

    for (;;) {
        if (!settings_applied && config_store_is_ready()) {
            const smi_phy_config_t *cfgs = config_store_get_smi_devices();
            size_t n = config_store_get_smi_device_count();
            for (size_t di = 0; di < n; ++di) {
                const smi_phy_config_t *cfg = &cfgs[di];
                for (size_t si = 0; si < cfg->settings_len; ++si) {
                    (void)smi_write_checked(cfg->phy_addr, cfg->settings[si].reg, cfg->settings[si].val);
                    vTaskDelay(pdMS_TO_TICKS(2));
                }
            }
            settings_applied = true;
        }

        const smi_phy_config_t *cfgs = config_store_is_ready() ? config_store_get_smi_devices() : NULL;
        size_t cfg_n = config_store_is_ready() ? config_store_get_smi_device_count() : 0u;

        if (!cfgs || cfg_n == 0u) {
            /* Legacy fallback: scan PHY=1 regs 0..31. */
            for (uint8_t i = 0; i < 32; i++) {
                if (smi_bus_mutex) xSemaphoreTake(smi_bus_mutex, portMAX_DELAY);
                mdio_read(0x01, i);
                if (s2h_filter_en == 0) {
                    uint32_t dummy;
                    xQueueReceive(q_s2h_evt, &dummy, 0);
                }
                if (smi_bus_mutex) xSemaphoreGive(smi_bus_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        for (size_t di = 0; di < cfg_n; ++di) {
            const smi_phy_config_t *cfg = &cfgs[di];
            uint8_t phy = (uint8_t)(cfg->phy_addr & 0x1Fu);

            if (cfg->autopoll_enabled && cfg->autopoll_regs_len > 0u) {
                for (size_t ri = 0; ri < cfg->autopoll_regs_len; ++ri) {
                    uint8_t reg = cfg->autopoll_regs[ri];
                    if (smi_bus_mutex) xSemaphoreTake(smi_bus_mutex, portMAX_DELAY);
                    mdio_read(phy, reg);
                    if (s2h_filter_en == 0) {
                        uint32_t dummy;
                        xQueueReceive(q_s2h_evt, &dummy, 0);
                    }
                    if (smi_bus_mutex) xSemaphoreGive(smi_bus_mutex);
                    if (cfg->autopoll_reg_delay_ms) vTaskDelay(pdMS_TO_TICKS(cfg->autopoll_reg_delay_ms));
                }
            } else {
                for (uint8_t reg = 0; reg < cfg->reg_count; ++reg) {
                    if (smi_bus_mutex) xSemaphoreTake(smi_bus_mutex, portMAX_DELAY);
                    mdio_read(phy, reg);
                    if (s2h_filter_en == 0) {
                        uint32_t dummy;
                        xQueueReceive(q_s2h_evt, &dummy, 0);
                    }
                    if (smi_bus_mutex) xSemaphoreGive(smi_bus_mutex);
                    if (cfg->autopoll_reg_delay_ms) vTaskDelay(pdMS_TO_TICKS(cfg->autopoll_reg_delay_ms));
                }
            }
            uint32_t cd = cfg->autopoll_cycle_delay_ms ? cfg->autopoll_cycle_delay_ms : 1u;
            vTaskDelay(pdMS_TO_TICKS(cd));
        }
    }
}

void mdio_write(uint8_t phy, uint8_t reg, uint16_t data)
{
    if (smi_bus_mutex) xSemaphoreTake(smi_bus_mutex, portMAX_DELAY);
    uint32_t x = (uint32_t)data
               | ((uint32_t)(reg & 0x1F) << 16)
               | ((uint32_t)(phy & 0x1F) << 21)
               | ((uint32_t)1u << 26);
    Xil_Out32(MASTER_BASEADDR + TX_FIFO_m, x);
    if (smi_bus_mutex) xSemaphoreGive(smi_bus_mutex);
}

bool smi_write_checked(uint8_t phy, uint8_t reg, uint16_t data)
{
    if (!smi_is_write_allowed((uint8_t)(phy & 0x1Fu), (uint8_t)(reg & 0x1Fu))) return false;
    uint8_t phy5 = (uint8_t)(phy & 0x1Fu);
    uint8_t reg5 = (uint8_t)(reg & 0x1Fu);
    mdio_write(phy5, reg5, data);

    /* Track persisted settings in config (like i2c does). */
    const smi_phy_config_t *cfg = config_store_find_smi_device_by_phy(phy5);
    if (cfg && reg5 < cfg->reg_count) {
        smi_phy_config_t *mut = (smi_phy_config_t *)cfg;
        taskENTER_CRITICAL();
        bool updated = false;
        for (size_t i = 0; i < mut->settings_len; ++i) {
            if (mut->settings[i].reg == reg5) {
                mut->settings[i].val = data;
                updated = true;
                break;
            }
        }
        if (!updated && mut->settings_len < SMI_CFG_SETTINGS_MAX) {
            mut->settings[mut->settings_len].reg = reg5;
            mut->settings[mut->settings_len].val = data;
            mut->settings_len++;
        }
        taskEXIT_CRITICAL();
    }
    return true;
}

void mdio_read(uint8_t phy, uint8_t reg)
{
    uint32_t x = 0
               | ((uint32_t)(reg & 0x1F) << 16)
               | ((uint32_t)(phy & 0x1F) << 21)
               | ((uint32_t)0u << 26);
    Xil_Out32(MASTER_BASEADDR + TX_FIFO_m, x);
}

bool mdio_read_blocking(uint8_t phy, uint8_t reg, uint16_t *out_value, TickType_t timeout_ticks)
{
    if (!out_value || !q_s2h_evt) return false;

    if (smi_bus_mutex) xSemaphoreTake(smi_bus_mutex, portMAX_DELAY);

    s2h_filter_phy = (uint8_t)(phy & 0x1F);
    s2h_filter_reg = (uint8_t)(reg & 0x1F);
    s2h_filter_en  = 1;

    xQueueReset(q_s2h_evt);

    mdio_read(phy, reg);

    uint32_t word = 0;
    BaseType_t ok = xQueueReceive(q_s2h_evt, &word, timeout_ticks);

    s2h_filter_en = 0;

    if (smi_bus_mutex) xSemaphoreGive(smi_bus_mutex);

    if (ok != pdTRUE) return false;

    *out_value = (uint16_t)(word & 0xFFFFU);
    return true;
}

void timeout_write(uint16_t timeout)
{
    uint32_t x = 0
               | ((uint32_t)(timeout & 0x7FFF) << 15)
               | ((uint32_t)1u << 31);
    Xil_Out32(MASTER_BASEADDR + TIMEOUT_m, x);
}

uint16_t timeout_read(void)
{
    uint16_t timeout = (Xil_In32(MASTER_BASEADDR + TIMEOUT_m) & 0x3FFF8000U) >> 15;
    xil_printf("\n\rTimeout = %lu us \n\r\n\r", (uint32_t)(timeout/100U));
    return timeout;
}

static void master_ISR(void *CallBackRef)
{
    (void)CallBackRef;

    if (q_master == NULL) { Xil_Out32(MASTER_BASEADDR + IRQ_m, 1U); return; }

    uint32_t csr = Xil_In32(MASTER_BASEADDR + CSR_m);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    master_evt_t evt = (master_evt_t){ .csr = csr, .mem_addr = 0, .type = MASTER_EVT_OVERFLOW };

    if ((csr & 0x28U) != 0U) {
        evt.type = MASTER_EVT_OVERFLOW;
        (void)xQueueSendFromISR(q_master, &evt, &xHigherPriorityTaskWoken);
        Xil_Out32(MASTER_BASEADDR + CSR_m, 0x01U);
    } else if ((csr & 0x01U) != 0U) {
        evt.type = MASTER_EVT_DATA;
        evt.mem_addr = Xil_In32(MASTER_BASEADDR + MEM_AADR_m);
        (void)xQueueSendFromISR(q_master, &evt, &xHigherPriorityTaskWoken);
        Xil_Out32(MASTER_BASEADDR + IRQ_m, 1U);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void slave_ISR(void *CallBackRef)
{
    (void)CallBackRef;

    if (q_slave == NULL) { Xil_Out32(SLAVE_BASEADDR + IRQ_s, 1U); return; }

    uint32_t csr = Xil_In32(SLAVE_BASEADDR + CSR_s);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    slave_evt_t evt = (slave_evt_t){ .csr = csr, .bram_addr = 0, .s2h = 0, .type = SLAVE_EVT_HOST_WRITE };

    if ((csr & 0x01U) != 0U) {
        evt.type = SLAVE_EVT_HOST_WRITE;
        evt.bram_addr = Xil_In32(SLAVE_BASEADDR + MEM_ADDR_s);
        (void)xQueueSendFromISR(q_slave, &evt, &xHigherPriorityTaskWoken);
    } else if ((csr & 0x04U) != 0U) {
        evt.type = SLAVE_EVT_HOST_READ;
        evt.s2h = Xil_In32(SLAVE_BASEADDR + S2H);
        (void)xQueueSendFromISR(q_slave, &evt, &xHigherPriorityTaskWoken);
    }

    Xil_Out32(SLAVE_BASEADDR + IRQ_s, 1U);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void smi_irq_install(void)
{
    xPortInstallInterruptHandler(IRQ_MASTER, master_ISR, NULL);
    xPortInstallInterruptHandler(IRQ_SLAVE,  slave_ISR,  NULL);
}

static void master_evt_task(void *arg)
{
    (void)arg;

    static uint8_t once = 0;
    if (!once) { once = 1; smi_irq_enable(); }

    master_evt_t evt;
    for (;;) {
        if (xQueueReceive(q_master, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.type) {
            case MASTER_EVT_OVERFLOW:
                xil_printf("One of the buffers is full, you idiot\n\r");
                break;
            case MASTER_EVT_DATA:
                bram_master_addr = evt.mem_addr;
                bram_master_data = Xil_In32(bram_master_addr);
                Xil_Out32(bram_master_addr + SLAVE_RD_OFFSET, bram_master_data);
                break;
            default:
                break;
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
            switch (evt.type) {
            case SLAVE_EVT_HOST_WRITE: {
                Xil_Out32(MASTER_BASEADDR + CSR_m, 0x01U);
                bram_slave_addr = evt.bram_addr;
                uint32_t phy_reg_field = (bram_slave_addr - BRAM_BASEADDR - SLAVE_WR_OFFSET) / 4U;
                uint8_t reg_addr = (uint8_t)(phy_reg_field & 0x1FU);
                uint8_t phy_addr = (uint8_t)(phy_reg_field >> 5);
                bram_slave_data = Xil_In32(bram_slave_addr);
                uint16_t data = (uint16_t)(bram_slave_data & 0xFFFFU);
                xil_printf("[IRQ slave->task]: Host write data = 0x%04X reg_addr=%u phy_addr=%u \n\r", data, reg_addr, phy_addr);
                if (!smi_write_checked(phy_addr, reg_addr, data)) {
                    xil_printf("[SMI] DENIED: phy=%u reg=%u val=0x%04X\r\n", phy_addr, reg_addr, data);
                }
                break;
            }
            case SLAVE_EVT_HOST_READ: {
                slave2host_data = evt.s2h;
                uint16_t data      = (uint16_t)( slave2host_data        & 0xFFFFU);
                uint8_t  reg_addr  = (uint8_t) ((slave2host_data >> 16) & 0x1FU);
                uint8_t  phy_addr  = (uint8_t) ((slave2host_data >> 21) & 0x1FU);
                uint8_t  rw        = (uint8_t) ((slave2host_data >> 26) & 0x01U);
                xil_printf("[IRQ slave->task]: Host read data=0x%04X reg_addr=%u phy_addr=%u %s\r\n", data, reg_addr, phy_addr, rw ? "write" : "read");
                if (s2h_filter_en && phy_addr == s2h_filter_phy && reg_addr == s2h_filter_reg) {
                    xQueueOverwrite(q_s2h_evt, &evt.s2h);
                }
                break;
            }
            default:
                break;
            }
        }
    }
}
