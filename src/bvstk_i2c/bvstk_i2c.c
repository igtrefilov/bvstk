#include "bvstk_i2c.h"

#include <string.h>
#include <strings.h>

#include "../config/config_store.h"

#ifndef I2C_BOOT_SCAN_ENABLE
#define I2C_BOOT_SCAN_ENABLE 0
#endif
#ifndef I2C_AUTOPOLL_ENABLE
#define I2C_AUTOPOLL_ENABLE 0
#endif

static QueueHandle_t q_master = NULL;
static QueueHandle_t q_slave  = NULL;
static SemaphoreHandle_t i2c_bus_mutex = NULL;
static volatile uint32_t g_master_wr_offset = 0;

typedef enum { MASTER_EVT_IRQ } master_evt_type_t;
typedef struct { master_evt_type_t type; uint32_t wr_offset; } master_evt_t;

typedef enum { SLAVE_EVT_FRAME } slave_evt_type_t;
typedef struct { slave_evt_type_t type; uint32_t size; uint8_t op_read; } slave_evt_t;

#define I2C_STS_TX_FIFO_CNT(_sts)   (uint32_t)(((_sts) >> 5) & 0x7Fu)

static i2c_device_config_t *s_cfgs[I2CDEV_MAX_DEVICES];
static size_t s_cfg_count = 0;
static size_t s_selected = 0;

static uint8_t s_reg_cache[I2CDEV_MAX_DEVICES][I2CDEV_MAX_REG_COUNT];
static uint8_t s_slave_reg_ptr[I2CDEV_MAX_DEVICES];
static volatile uint8_t s_pending_dev = 0xFF;
static volatile uint8_t s_pending_reg = 0xFF;

static size_t i2cdev_selected_idx(void)
{
    size_t idx = 0;
    taskENTER_CRITICAL();
    idx = s_selected;
    taskEXIT_CRITICAL();
    if (idx >= s_cfg_count) idx = 0;
    return idx;
}

static bool i2cdev_set_selected_idx(size_t idx)
{
    if (idx >= s_cfg_count) return false;
    taskENTER_CRITICAL();
    s_selected = idx;
    taskEXIT_CRITICAL();
    return true;
}

static i2c_device_config_t *i2cdev_selected_cfg(void)
{
    if (s_cfg_count == 0) return NULL;
    return s_cfgs[i2cdev_selected_idx()];
}

static uint8_t *i2cdev_selected_cache(void)
{
    if (s_cfg_count == 0) return NULL;
    return s_reg_cache[i2cdev_selected_idx()];
}

static i2c_device_config_t *i2cdev_cfg_by_idx(size_t idx)
{
    if (idx >= s_cfg_count) return NULL;
    return s_cfgs[idx];
}

static uint8_t *i2cdev_cache_by_idx(size_t idx)
{
    if (idx >= s_cfg_count) return NULL;
    return s_reg_cache[idx];
}

size_t i2cdev_device_count(void)
{
    return s_cfg_count;
}

bool i2cdev_device_get_info(size_t idx, i2cdev_device_info_t *out)
{
    if (!out || idx >= s_cfg_count) return false;
    i2c_device_config_t *c = s_cfgs[idx];
    out->name = c ? c->name : "";
    out->addr_7b = c ? c->addr_7b : 0;
    out->reg_count = c ? c->reg_count : 0;
    out->max_value_code = c ? c->max_value_code : 0;
    return (c != NULL);
}

bool i2cdev_select_device(const char *name)
{
    if (!name || !name[0]) return false;
    for (size_t i = 0; i < s_cfg_count; ++i) {
        if (s_cfgs[i] && strcasecmp(s_cfgs[i]->name, name) == 0) {
            return i2cdev_set_selected_idx(i);
        }
    }
    return false;
}

bool i2cdev_select_device_addr(uint8_t addr_7b)
{
    addr_7b &= 0x7Fu;
    for (size_t i = 0; i < s_cfg_count; ++i) {
        if (s_cfgs[i] && s_cfgs[i]->addr_7b == addr_7b) {
            return i2cdev_set_selected_idx(i);
        }
    }
    return false;
}

bool i2cdev_get_selected_info(i2cdev_device_info_t *out)
{
    if (!out) return false;
    i2c_device_config_t *c = i2cdev_selected_cfg();
    if (!c) return false;
    out->name = c->name;
    out->addr_7b = c->addr_7b;
    out->reg_count = c->reg_count;
    out->max_value_code = c->max_value_code;
    return true;
}

bool i2cdev_find_device_index_by_name(const char *name, size_t *out_idx)
{
    if (out_idx) *out_idx = 0;
    if (!name || !name[0]) return false;
    for (size_t i = 0; i < s_cfg_count; ++i) {
        if (s_cfgs[i] && strcasecmp(s_cfgs[i]->name, name) == 0) {
            if (out_idx) *out_idx = i;
            return true;
        }
    }
    return false;
}

bool i2cdev_find_device_index_by_addr(uint8_t addr_7b, size_t *out_idx)
{
    if (out_idx) *out_idx = 0;
    addr_7b &= 0x7Fu;
    for (size_t i = 0; i < s_cfg_count; ++i) {
        if (s_cfgs[i] && s_cfgs[i]->addr_7b == addr_7b) {
            if (out_idx) *out_idx = i;
            return true;
        }
    }
    return false;
}

static int rule_list_contains(const i2c_rule_entry_t *rules, size_t len, uint8_t reg, uint8_t val)
{
    for (size_t i = 0; i < len; ++i) {
        if (rules[i].reg == reg && rules[i].val == val) return 1;
    }
    return 0;
}

static void rule_list_add(i2c_rule_entry_t *rules, size_t *len, size_t max, uint8_t reg, uint8_t val)
{
    if (!rules || !len) return;
    if (rule_list_contains(rules, *len, reg, val)) return;
    if (*len < max) {
        rules[*len].reg = reg;
        rules[*len].val = val;
        (*len)++;
    }
}

static void rule_list_remove(i2c_rule_entry_t *rules, size_t *len, uint8_t reg, uint8_t val)
{
    if (!rules || !len) return;
    size_t w = 0;
    for (size_t i = 0; i < *len; ++i) {
        if (rules[i].reg == reg && rules[i].val == val) continue;
        rules[w++] = rules[i];
    }
    *len = w;
}

static bool i2cdev_is_value_permitted_cfg(const i2c_device_config_t *cfg, uint8_t reg, uint8_t val)
{
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (val > cfg->max_value_code) return false;
    if (cfg->policy == I2C_POLICY_WHITELIST) {
        return rule_list_contains(cfg->whitelist, cfg->whitelist_len, reg, val) != 0;
    }
    return rule_list_contains(cfg->blacklist, cfg->blacklist_len, reg, val) == 0;
}

static inline void reg_write32(uint32_t base, uint32_t ofs, uint32_t v) { Xil_Out32(base + ofs, v); }
static inline uint32_t reg_read32(uint32_t base, uint32_t ofs) { return Xil_In32(base + ofs); }

static bool i2cdev_write_byte(size_t dev_idx, uint8_t addr7, uint8_t reg, uint8_t val);
static void i2c_slave_fill_read_window(const uint8_t *src, uint32_t nbytes);
static bool i2c_master_read_reg_to_bram(size_t dev_idx, uint8_t addr7, uint8_t reg, uint32_t rd_len);

static void master_evt_task(void *arg);
static void slave_evt_task (void *arg);
static void master_ISR     (void *CallBackRef);
static void slave_ISR      (void *CallBackRef);
static inline void i2c_irq_enable(void)
{
    if (I2C_HAS_IRQ != 0U) {
        vPortEnableInterrupt(IRQ_I2C_MASTER);
        vPortEnableInterrupt(IRQ_I2C_SLAVE);
    }
}
static bool i2cdev_read_reg_idx(size_t dev_idx, uint8_t reg, uint8_t *out_val);

void i2cdev_policy_reset_defaults(void)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return;
    cfg->policy = I2C_POLICY_WHITELIST;
    cfg->whitelist_len = 0;
    cfg->blacklist_len = 0;
    cfg->autopoll_enabled = false;
    cfg->autopoll_regs_len = 0;
    cfg->autopoll_reg_delay_ms = 0;
    cfg->autopoll_cycle_delay_ms = 1000u;
}

void i2cdev_policy_clear_all(void)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return;
    cfg->whitelist_len = 0;
    cfg->blacklist_len = 0;
}

void i2cdev_set_policy(i2cdev_policy_t policy)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return;
    taskENTER_CRITICAL();
    cfg->policy = (policy == I2CDEV_POLICY_BLACKLIST) ? I2C_POLICY_BLACKLIST : I2C_POLICY_WHITELIST;
    taskEXIT_CRITICAL();
}

i2cdev_policy_t i2cdev_get_policy(void)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return I2CDEV_POLICY_WHITELIST;
    return (cfg->policy == I2C_POLICY_BLACKLIST) ? I2CDEV_POLICY_BLACKLIST : I2CDEV_POLICY_WHITELIST;
}

bool i2cdev_set_policy_dev(size_t dev_idx, i2cdev_policy_t policy)
{
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (!cfg) return false;
    taskENTER_CRITICAL();
    cfg->policy = (policy == I2CDEV_POLICY_BLACKLIST) ? I2C_POLICY_BLACKLIST : I2C_POLICY_WHITELIST;
    taskEXIT_CRITICAL();
    return true;
}

i2cdev_policy_t i2cdev_get_policy_dev(size_t dev_idx)
{
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (!cfg) return I2CDEV_POLICY_WHITELIST;
    return (cfg->policy == I2C_POLICY_BLACKLIST) ? I2CDEV_POLICY_BLACKLIST : I2CDEV_POLICY_WHITELIST;
}

bool i2cdev_rule_allow(uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (val > cfg->max_value_code) return false;
    taskENTER_CRITICAL();
    rule_list_add(cfg->whitelist, &cfg->whitelist_len, I2C_CFG_RULES_MAX, reg, val);
    taskEXIT_CRITICAL();
    return true;
}

bool i2cdev_rule_deny(uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (val > cfg->max_value_code) return false;
    taskENTER_CRITICAL();
    rule_list_add(cfg->blacklist, &cfg->blacklist_len, I2C_CFG_RULES_MAX, reg, val);
    taskEXIT_CRITICAL();
    return true;
}

bool i2cdev_rule_clear(uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return false;
    taskENTER_CRITICAL();
    rule_list_remove(cfg->whitelist, &cfg->whitelist_len, reg, val);
    rule_list_remove(cfg->blacklist, &cfg->blacklist_len, reg, val);
    taskEXIT_CRITICAL();
    return true;
}

bool i2cdev_rule_allow_dev(size_t dev_idx, uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (val > cfg->max_value_code) return false;
    taskENTER_CRITICAL();
    rule_list_add(cfg->whitelist, &cfg->whitelist_len, I2C_CFG_RULES_MAX, reg, val);
    taskEXIT_CRITICAL();
    return true;
}

bool i2cdev_rule_deny_dev(size_t dev_idx, uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (val > cfg->max_value_code) return false;
    taskENTER_CRITICAL();
    rule_list_add(cfg->blacklist, &cfg->blacklist_len, I2C_CFG_RULES_MAX, reg, val);
    taskEXIT_CRITICAL();
    return true;
}

bool i2cdev_rule_clear_dev(size_t dev_idx, uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (!cfg) return false;
    taskENTER_CRITICAL();
    rule_list_remove(cfg->whitelist, &cfg->whitelist_len, reg, val);
    rule_list_remove(cfg->blacklist, &cfg->blacklist_len, reg, val);
    taskEXIT_CRITICAL();
    return true;
}

bool i2cdev_is_value_permitted_current(uint8_t reg, uint8_t val)
{
    return i2cdev_is_value_permitted_cfg(i2cdev_selected_cfg(), reg, val);
}

static bool i2c_master_wait_ready(uint32_t timeout_ms)
{
    /* MSTR_CSR readback is status_reg_t: bit0=core_state. */
    uint32_t waited = 0u;
    while ((reg_read32(I2C_MASTER_BASE, MSTR_CSR_REG_OFFSET) & 0x00000001u) != 0u) {
        if (waited >= timeout_ms) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        waited++;
    }
    return true;
}

static bool i2c_master_wait_bus_idle(uint32_t timeout_ms)
{
    uint32_t waited = 0u;
    while (waited < timeout_ms) {
        uint32_t dbg2 = reg_read32(I2C_MASTER_BASE, MSTR_DBG2_OFFSET);
        bool scl_high = ((dbg2 >> MSTR_DBG2_SCL_I_BIT) & 0x1u) != 0u;
        bool sda_high = ((dbg2 >> MSTR_DBG2_SDA_I_BIT) & 0x1u) != 0u;
        bool core_idle = (reg_read32(I2C_MASTER_BASE, MSTR_CSR_REG_OFFSET) & 0x00000001u) == 0u;
        if (core_idle && scl_high && sda_high) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        waited++;
    }
    return false;
}

static void i2c_master_print_transaction(uint8_t addr_7b, uint8_t op_read, uint32_t num_bytes, const uint8_t *payload)
{
    uint8_t b0 = (payload && num_bytes > 0u) ? payload[0] : 0u;
    uint8_t b1 = (payload && num_bytes > 1u) ? payload[1] : 0u;
    uint8_t b2 = (payload && num_bytes > 2u) ? payload[2] : 0u;
    uint8_t b3 = (payload && num_bytes > 3u) ? payload[3] : 0u;
    xil_printf("[I2C][MSTR][tx] a=0x%02X rw=%u n=%lu b=%02X %02X %02X %02X\r\n",
               (unsigned)(addr_7b & 0x7Fu),
               (unsigned)(op_read ? 1u : 0u),
               (unsigned long)num_bytes,
               (unsigned)b0, (unsigned)b1, (unsigned)b2, (unsigned)b3);
}

static void i2c_master_print_lowlevel(const char *tag)
{
    uint32_t dbg0 = reg_read32(I2C_MASTER_BASE, MSTR_DBG0_OFFSET);
    uint32_t dbg1 = reg_read32(I2C_MASTER_BASE, MSTR_DBG1_OFFSET);
    uint32_t dbg2 = reg_read32(I2C_MASTER_BASE, MSTR_DBG2_OFFSET);
    uint32_t dbg3 = reg_read32(I2C_MASTER_BASE, MSTR_DBG3_OFFSET);
    xil_printf("[I2C][MSTR][ll] %s d0=0x%08lX d1=0x%08lX d2=0x%08lX d3=0x%08lX\r\n",
               tag ? tag : "?",
               (unsigned long)dbg0,
               (unsigned long)dbg1,
               (unsigned long)dbg2,
               (unsigned long)dbg3);
}

static void i2c_master_recover_after_timeout(void)
{
    reg_write32(I2C_MASTER_BASE, MSTR_CSR_REG_OFFSET, MSTR_CSR_SOFT_RESET_BIT);
    vTaskDelay(pdMS_TO_TICKS(I2C_MASTER_RECOVER_DELAY_MS));
    (void)i2c_master_wait_ready(I2C_MASTER_RECOVER_DELAY_MS * 2u);
    (void)i2c_master_wait_bus_idle(I2C_MASTER_RECOVER_DELAY_MS * 2u);
}

void start_i2c(void)
{
    q_master = xQueueCreate(64, sizeof(master_evt_t));
    q_slave  = xQueueCreate(64, sizeof(slave_evt_t));
    i2c_bus_mutex = xSemaphoreCreateMutex();
    configASSERT(q_master);
    configASSERT(q_slave);
    configASSERT(i2c_bus_mutex);
    if (I2C_HAS_IRQ != 0U) {
        reg_write32(I2C_MASTER_BASE, MSTR_IRQ_REG_OFFSET, 0x01);
        reg_write32(I2C_SLAVE_BASE,  SLV_IRQ_REG_OFFSET, 0x01);
        xPortInstallInterruptHandler(IRQ_I2C_MASTER, master_ISR, NULL);
        xPortInstallInterruptHandler(IRQ_I2C_SLAVE,  slave_ISR,  NULL);
    }
    configASSERT(xTaskCreate(master_evt_task, "i2c_master_evt", I2C_TASK_STACK_SIZE, NULL, I2C_TASK_PRIORITY, NULL) == pdPASS);
    configASSERT(xTaskCreate(slave_evt_task,  "i2c_slave_evt",  I2C_TASK_STACK_SIZE, NULL, I2C_TASK_PRIORITY, NULL) == pdPASS);
    configASSERT(xTaskCreate(i2c_task,        "i2c_task",       I2C_TASK_STACK_SIZE, NULL, I2C_TASK_PRIORITY,       NULL) == pdPASS);
}

void i2cdev_autopoll_get(i2cdev_autopoll_profile_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return;
    out->enabled = cfg->autopoll_enabled;
    out->reg_delay_ms = cfg->autopoll_reg_delay_ms;
    out->cycle_delay_ms = cfg->autopoll_cycle_delay_ms;
    size_t n = cfg->autopoll_regs_len;
    if (n > sizeof(out->regs)) n = sizeof(out->regs);
    memcpy(out->regs, cfg->autopoll_regs, n);
    out->regs_len = n;
}

void i2cdev_autopoll_set(const i2cdev_autopoll_profile_t *p)
{
    if (!p) return;
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return;
    taskENTER_CRITICAL();
    size_t n = p->regs_len;
    if (n > I2C_CFG_AUTOPOLL_REGS_MAX) n = I2C_CFG_AUTOPOLL_REGS_MAX;
    size_t w = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t r = p->regs[i];
        if (r < cfg->reg_count) cfg->autopoll_regs[w++] = r;
    }
    cfg->autopoll_regs_len = w;
    cfg->autopoll_enabled = p->enabled;
    cfg->autopoll_reg_delay_ms = p->reg_delay_ms;
    cfg->autopoll_cycle_delay_ms = p->cycle_delay_ms ? p->cycle_delay_ms : 1u;
    taskEXIT_CRITICAL();
}

static bool i2cdev_autopoll_any_enabled(void)
{
    for (size_t i = 0; i < s_cfg_count; ++i) {
        i2c_device_config_t *cfg = s_cfgs[i];
        if (cfg && cfg->autopoll_enabled && cfg->autopoll_regs_len > 0u) return true;
    }
    return false;
}

void i2c_task(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (!config_store_wait_ready(5000)) {
        xil_printf("I2C: config store not ready; no devices\r\n");
        vTaskDelete(NULL);
        return;
    }

    const i2c_device_config_t *cfgs = config_store_get_i2c_devices();
    size_t n = config_store_get_i2c_device_count();
    if (!cfgs || n == 0) {
        xil_printf("I2C: no devices configured\r\n");
        vTaskDelete(NULL);
        return;
    }
    if (n > I2CDEV_MAX_DEVICES) n = I2CDEV_MAX_DEVICES;
    s_cfg_count = n;
    for (size_t i = 0; i < s_cfg_count; ++i) {
        s_cfgs[i] = (i2c_device_config_t *)&cfgs[i];
        memset(s_reg_cache[i], 0, sizeof(s_reg_cache[i]));
#if !I2C_AUTOPOLL_ENABLE
        s_cfgs[i]->autopoll_enabled = false;
        s_cfgs[i]->autopoll_regs_len = 0u;
        s_cfgs[i]->autopoll_reg_delay_ms = 0u;
        s_cfgs[i]->autopoll_cycle_delay_ms = 1000u;
#endif
    }
    s_selected = 0;
    for (size_t i = 0; i < s_cfg_count; ++i) {
        if (s_cfgs[i] && strcasecmp(s_cfgs[i]->name, "axp15060") == 0) { s_selected = i; break; }
    }
    xil_printf("I2C: loaded %u device(s), selected=%s (0x%02X)\r\n",
               (unsigned)s_cfg_count,
               s_cfgs[s_selected] ? s_cfgs[s_selected]->name : "?",
               s_cfgs[s_selected] ? s_cfgs[s_selected]->addr_7b : 0);

#if I2C_BOOT_SCAN_ENABLE
    i2cdev_init_full_scan();
    /* Restore persisted device settings (register writes) from config. */
    for (size_t di = 0; di < s_cfg_count; ++di) {
        i2c_device_config_t *cfg = s_cfgs[di];
        if (!cfg || cfg->settings_len == 0u) continue;
        for (size_t si = 0; si < cfg->settings_len; ++si) {
            uint8_t reg = cfg->settings[si].reg;
            uint8_t val = cfg->settings[si].val;
            if (reg >= cfg->reg_count) continue;
            i2cdev_write_byte(di, cfg->addr_7b, reg, val);
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
#endif
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        TickType_t next_wakeup = now + pdMS_TO_TICKS(1000);

#if I2C_AUTOPOLL_ENABLE
        static TickType_t next_due[I2CDEV_MAX_DEVICES] = { 0 };

        for (size_t di = 0; di < s_cfg_count; ++di) {
            i2c_device_config_t *cfg = s_cfgs[di];
            if (!cfg || !cfg->autopoll_enabled || cfg->autopoll_regs_len == 0u) continue;
            TickType_t due = next_due[di];
            if (due == 0 || (int32_t)(now - due) >= 0) {
                for (size_t ri = 0; ri < cfg->autopoll_regs_len; ++ri) {
                    uint8_t reg = cfg->autopoll_regs[ri];
                    if (reg >= cfg->reg_count) continue;
                    uint8_t v;
                    (void)i2cdev_read_reg_idx(di, reg, &v);
                    if (cfg->autopoll_reg_delay_ms) vTaskDelay(pdMS_TO_TICKS(cfg->autopoll_reg_delay_ms));
                }
                uint32_t cd = cfg->autopoll_cycle_delay_ms ? cfg->autopoll_cycle_delay_ms : 1u;
                next_due[di] = now + pdMS_TO_TICKS(cd);
                due = next_due[di];
            }
            if (due < next_wakeup) next_wakeup = due;
        }

        if (!i2cdev_autopoll_any_enabled()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            TickType_t sleep_ticks = (next_wakeup > now) ? (next_wakeup - now) : pdMS_TO_TICKS(1);
            if (sleep_ticks < pdMS_TO_TICKS(1)) sleep_ticks = pdMS_TO_TICKS(1);
            vTaskDelay(sleep_ticks);
        }
#else
        (void)next_wakeup;
        vTaskDelay(pdMS_TO_TICKS(1000));
#endif
    }
}

bool i2c_master_send(uint8_t addr_7b, uint8_t op_read, uint32_t num_bytes, const uint8_t *payload, uint32_t buf_size, uint8_t csr_bits)
{
    (void)buf_size;
    if (i2c_bus_mutex && xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        xil_printf("[I2C][MSTR] mutex timeout before send\r\n");
        return false;
    }
    i2c_master_print_transaction(addr_7b, op_read, num_bytes, payload);
    bool ok = false;
    for (uint32_t attempt = 0; attempt < I2C_MASTER_RECOVER_RETRIES; ++attempt) {
        if (!i2c_master_wait_ready(100u)) {
            i2c_master_recover_after_timeout();
            continue;
        }

        uint32_t nb = (num_bytes > 0x1FFFFFu) ? 0x1FFFFFu : (uint32_t)num_bytes;
        uint32_t header = I2C_MAKE_MASTER_HEADER((uint8_t)(addr_7b & 0x7Fu), op_read ? 1u : 0u, 0u, 0u, nb);
        reg_write32(I2C_MASTER_BASE, MSTR_TX_FIFO_OFFSET, header);
        uint32_t full_words = nb / 4u;
        uint32_t tail_bytes = nb % 4u;
        const uint8_t *p = payload;
        for (uint32_t w = 0; w < full_words; ++w) {
            uint32_t v = ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
            reg_write32(I2C_MASTER_BASE, MSTR_TX_FIFO_OFFSET, v);
            p += 4;
        }
        if (tail_bytes) {
            uint8_t b0 = (tail_bytes > 0) ? p[0] : 0;
            uint8_t b1 = (tail_bytes > 1) ? p[1] : 0;
            uint8_t b2 = (tail_bytes > 2) ? p[2] : 0;
            uint32_t v = ((uint32_t)b0) | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16);
            reg_write32(I2C_MASTER_BASE, MSTR_TX_FIFO_OFFSET, v);
        }
        reg_write32(I2C_MASTER_BASE, MSTR_CSR_REG_OFFSET, (uint32_t)csr_bits);
        if (i2c_master_wait_ready(100u)) {
            ok = true;
            i2c_master_print_lowlevel("ok");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(I2C_MASTER_SETTLE_DELAY_MS));
        if (i2c_master_wait_ready(I2C_MASTER_SETTLE_DELAY_MS)) {
            ok = true;
            i2c_master_print_lowlevel("late_ok");
            break;
        }
        i2c_master_print_lowlevel("fail");
        i2c_master_recover_after_timeout();
    }
    if (i2c_bus_mutex) xSemaphoreGive(i2c_bus_mutex);
    return ok;
}

static bool i2cdev_write_byte(size_t dev_idx, uint8_t addr7, uint8_t reg, uint8_t val)
{
    uint8_t payload[2] = { reg, val };
    if (!i2c_master_send(addr7, 0, 2, payload, 2, MSTR_CSR_START_BIT)) {
        xil_printf("[I2C][MSTR][wr] send failed a=0x%02X r=0x%02X v=0x%02X\r\n",
                   (unsigned)(addr7 & 0x7Fu),
                   (unsigned)reg,
                   (unsigned)val);
        return false;
    }
    if (dev_idx < I2CDEV_MAX_DEVICES && reg < I2CDEV_MAX_REG_COUNT) {
        s_reg_cache[dev_idx][reg] = val;
    }
    /* Track persisted device settings (register writes) in the loaded config. */
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (cfg && reg < cfg->reg_count) {
        taskENTER_CRITICAL();
        bool updated = false;
        for (size_t i = 0; i < cfg->settings_len; ++i) {
            if (cfg->settings[i].reg == reg) {
                cfg->settings[i].val = val;
                updated = true;
                break;
            }
        }
        if (!updated && cfg->settings_len < I2C_CFG_SETTINGS_MAX) {
            cfg->settings[cfg->settings_len].reg = reg;
            cfg->settings[cfg->settings_len].val = val;
            cfg->settings_len++;
        }
        taskEXIT_CRITICAL();
    }
    return true;
}

static void i2c_slave_fill_read_window(const uint8_t *src, uint32_t nbytes)
{
    if (!src || nbytes == 0u) return;
    uint32_t dst_ofs = I2C_BRAM_SLAVE_RD;
    uint32_t idx = 0;
    while (idx < nbytes) {
        uint32_t w = 0;
        for (uint32_t b = 0; b < 4 && idx < nbytes; ++b, ++idx) {
            w |= ((uint32_t)src[idx]) << (8u * b);
        }
        reg_write32(BRAM_BASE_ADDR, dst_ofs, w);
        dst_ofs += 4;
    }
}

static bool i2c_master_read_reg_to_bram(size_t dev_idx, uint8_t addr7, uint8_t reg, uint32_t rd_len)
{
    if (i2c_bus_mutex && xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        xil_printf("[I2C][MSTR][rd] mutex timeout\r\n");
        return false;
    }
    uint8_t trace_buf[4] = { reg, 0u, 0u, 0u };
    i2c_master_print_transaction(addr7, 1u, rd_len, trace_buf);
    bool ok = false;
    for (uint32_t attempt = 0; attempt < I2C_MASTER_RECOVER_RETRIES; ++attempt) {
        if (!i2c_master_wait_ready(100u)) {
            i2c_master_recover_after_timeout();
            continue;
        }
        /* First phase: write register pointer with RESTART=1; second phase: read bytes. */
        uint32_t h1 = I2C_MAKE_MASTER_HEADER((uint8_t)(addr7 & 0x7Fu), 0u, 1u, 0u, 1u);
        reg_write32(I2C_MASTER_BASE, MSTR_TX_FIFO_OFFSET, h1);
        reg_write32(I2C_MASTER_BASE, MSTR_TX_FIFO_OFFSET, (uint32_t)reg);
        uint32_t h2 = I2C_MAKE_MASTER_HEADER((uint8_t)(addr7 & 0x7Fu), 1u, 0u, 0u, rd_len);
        reg_write32(I2C_MASTER_BASE, MSTR_TX_FIFO_OFFSET, h2);
        reg_write32(I2C_MASTER_BASE, MSTR_CSR_REG_OFFSET, MSTR_CSR_START_BIT);
        if (i2c_master_wait_ready(100u)) {
            ok = true;
            i2c_master_print_lowlevel("rd_ok");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(I2C_MASTER_SETTLE_DELAY_MS));
        if (i2c_master_wait_ready(I2C_MASTER_SETTLE_DELAY_MS)) {
            ok = true;
            i2c_master_print_lowlevel("rd_late_ok");
            break;
        }
        i2c_master_print_lowlevel("rd_fail");
        i2c_master_recover_after_timeout();
    }
    if (!ok) {
        if (i2c_bus_mutex) xSemaphoreGive(i2c_bus_mutex);
        xil_printf("[I2C][MSTR][rd] start failed a=0x%02X r=0x%02X n=%lu\r\n",
                   (unsigned)(addr7 & 0x7Fu),
                   (unsigned)reg,
                   (unsigned long)rd_len);
        return false;
    }
    s_pending_dev = (dev_idx < 0xFFu) ? (uint8_t)dev_idx : 0xFFu;
    s_pending_reg = reg;
    if (i2c_bus_mutex) xSemaphoreGive(i2c_bus_mutex);
    return true;
}

static inline void i2cdev_read_byte_to_master_bram(size_t dev_idx, uint8_t addr7, uint8_t reg)
{
    (void)i2c_master_read_reg_to_bram(dev_idx, addr7, reg, 1u);
}

void i2cdev_init_full_scan(void)
{
    for (size_t di = 0; di < s_cfg_count; ++di) {
        i2c_device_config_t *cfg = s_cfgs[di];
        if (!cfg) continue;
        for (uint32_t r = 0; r < cfg->reg_count && r < I2CDEV_MAX_REG_COUNT; ++r) {
            i2cdev_read_byte_to_master_bram(di, cfg->addr_7b, (uint8_t)r);
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
}

void slave_check_and_exec(const uint8_t *frame, uint32_t size, uint8_t op_read)
{
    size_t sel = i2cdev_selected_idx();
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    uint8_t *cache = i2cdev_selected_cache();
    if (!cfg || !cache) return;
    uint8_t reg_ptr = s_slave_reg_ptr[sel];
    if (reg_ptr >= cfg->reg_count) reg_ptr = 0;

    if (op_read) {
        uint8_t refresh_reg = reg_ptr;
        uint32_t n = (size == 0u) ? 1u : size;
        if ((uint32_t)reg_ptr + n > cfg->reg_count) n = (uint32_t)cfg->reg_count - reg_ptr;
        if (n == 0u) n = 1u;
        i2c_slave_fill_read_window(&cache[reg_ptr], n);
        reg_ptr = (uint8_t)((uint32_t)reg_ptr + n);
        if (reg_ptr >= cfg->reg_count) reg_ptr = 0;
        s_slave_reg_ptr[sel] = reg_ptr;
        /* Serve host from cache immediately, then refresh the first requested register from PHY. */
        (void)i2c_master_read_reg_to_bram(sel, cfg->addr_7b, refresh_reg, 1u);
        return;
    }

    if (size == 0u) return;
    uint8_t reg = frame[0];
    if (reg >= cfg->reg_count) {
        return;
    }
    s_slave_reg_ptr[sel] = reg;

    if (size >= 2u) {
        uint32_t n = size - 1;
        if (reg + n > cfg->reg_count) n = (uint32_t)cfg->reg_count - reg;
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t val = frame[1 + i];
            uint8_t r   = (uint8_t)(reg + i);
            if (i2cdev_is_value_permitted_cfg(cfg, r, val)) {
                i2cdev_write_byte(sel, cfg->addr_7b, r, val);
            }
        }
        /* Keep pointer at the requested register for SMBus read-after-write behavior. */
        s_slave_reg_ptr[sel] = reg;
    }

    /* For a plain pointer write (size==1) or after writes, preload BRAM read window. */
    reg_ptr = s_slave_reg_ptr[sel];
    if (reg_ptr >= cfg->reg_count) reg_ptr = 0;
    uint32_t preload_n = (uint32_t)cfg->reg_count - reg_ptr;
    if (preload_n > 64u) preload_n = 64u;
    if (preload_n == 0u) preload_n = 1u;
    i2c_slave_fill_read_window(&cache[reg_ptr], preload_n);
}

static void master_ISR(void *CallBackRef)
{
    (void)CallBackRef;
    if (q_master == NULL) {
        reg_write32(I2C_MASTER_BASE, MSTR_IRQ_REG_OFFSET, 0x01);
        return;
    }
    reg_write32(I2C_MASTER_BASE, MSTR_IRQ_REG_OFFSET, 0x01);
    BaseType_t hpw = pdFALSE;
    master_evt_t evt = (master_evt_t){ .type = MASTER_EVT_IRQ, .wr_offset = 0 };
    (void)xQueueSendFromISR(q_master, &evt, &hpw);
    portYIELD_FROM_ISR(hpw);
}

static void slave_ISR(void *CallBackRef)
{
    (void)CallBackRef;
    if (q_slave == NULL) { reg_write32(I2C_SLAVE_BASE, SLV_IRQ_REG_OFFSET, 0x01); return; }
    uint32_t word0 = reg_read32(BRAM_BASE_ADDR, I2C_BRAM_SLAVE_WR);
    uint32_t sts   = reg_read32(I2C_SLAVE_BASE, SLV_STATUS_OFFSET);
    uint32_t tx_words = I2C_STS_TX_FIFO_CNT(sts);
    uint32_t size  = (tx_words > 0u) ? (tx_words - 1u) : 0u; /* words after address marker */
    if (size > 1024u) size = 1024u;
    uint8_t  op    = (size == 0u) ? 1u : 0u; /* read phase has no payload bytes */
    reg_write32(I2C_SLAVE_BASE, SLV_IRQ_REG_OFFSET, 0x01);
    BaseType_t hpw = pdFALSE;
    slave_evt_t evt = (slave_evt_t){ .type = SLAVE_EVT_FRAME, .size = size, .op_read = op };
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
            uint32_t nb  = I2C_MSTR_HDR_NUM_BYTES(hdr);
            uint8_t  op  = I2C_MSTR_HDR_RW(hdr);
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
                    if (s_pending_dev < I2CDEV_MAX_DEVICES && s_pending_reg < I2CDEV_MAX_REG_COUNT) {
                        s_reg_cache[s_pending_dev][s_pending_reg] = val;
                    }
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
                i2c_device_config_t *cfg = i2cdev_selected_cfg();
                if (!cfg) continue;
                uint8_t reg = (uint8_t)(reg_read32(BRAM_BASE_ADDR, I2C_BRAM_SLAVE_WR + 0x04) & 0xFFu);
                uint8_t val = (uint8_t)(reg_read32(BRAM_BASE_ADDR, I2C_BRAM_SLAVE_WR + 0x08) & 0xFFu);
                uint8_t frame2[2] = { reg, val };
                uint8_t frame1[1] = { reg };
                bool reg_ok = (reg < cfg->reg_count);
                bool val_ok = reg_ok && i2cdev_is_value_permitted_cfg(cfg, reg, val);
                bool has_payload = (evt.size > 0u) || (reg != 0u) || (val != 0u);
                bool do_ptr_or_write = reg_ok && has_payload;
                bool do_write = do_ptr_or_write && val_ok;
                bool do_read_only = evt.op_read && !do_ptr_or_write;

                /* Update pointer only when transaction has meaningful payload. */
                if (do_ptr_or_write) {
                    slave_check_and_exec(frame1, 1u, 0u);
                }
                /* Apply write only for policy-allowed value to avoid stale-garbage writes. */
                if (do_write) {
                    slave_check_and_exec(frame2, 2u, 0u);
                }
                /* For pure read phase (no payload), serve current pointer value. */
                if (do_read_only) {
                    slave_check_and_exec(NULL, 0u, 1u);
                }

                /* Prevent stale payload from affecting next short transaction. */
                for (uint32_t ofs = I2C_BRAM_SLAVE_WR + 0x04; ofs <= I2C_BRAM_SLAVE_WR + 0x20; ofs += 4) {
                    reg_write32(BRAM_BASE_ADDR, ofs, 0u);
                }
            }
        }
    }
}

static bool i2cdev_read_reg_idx(size_t dev_idx, uint8_t reg, uint8_t *out_val)
{
    if (!out_val) return false;
    if (dev_idx >= s_cfg_count) return false;
    i2c_device_config_t *cfg = s_cfgs[dev_idx];
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (!i2c_master_read_reg_to_bram(dev_idx, cfg->addr_7b, reg, 1u)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    *out_val = s_reg_cache[dev_idx][reg];
    return true;
}

bool i2cdev_read_reg_dev(size_t dev_idx, uint8_t reg, uint8_t *out_val)
{
    return i2cdev_read_reg_idx(dev_idx, reg, out_val);
}

bool i2cdev_read_reg_cached_dev(size_t dev_idx, uint8_t reg, uint8_t *out_val)
{
    if (!out_val) return false;
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    uint8_t *cache = i2cdev_cache_by_idx(dev_idx);
    if (!cfg || !cache) return false;
    if (reg >= cfg->reg_count) return false;
    *out_val = cache[reg];
    return true;
}

bool i2cdev_read_reg_cached(uint8_t reg, uint8_t *out_val)
{
    if (!out_val) return false;
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    uint8_t *cache = i2cdev_selected_cache();
    if (!cfg || !cache) return false;
    if (reg >= cfg->reg_count) return false;
    *out_val = cache[reg];
    return true;
}

bool i2cdev_read_reg(uint8_t reg, uint8_t *out_val)
{
    return i2cdev_read_reg_idx(i2cdev_selected_idx(), reg, out_val);
}

bool i2cdev_write_reg(uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_selected_cfg();
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (!i2cdev_is_value_permitted_cfg(cfg, reg, val)) return false;
    return i2cdev_write_byte(i2cdev_selected_idx(), cfg->addr_7b, reg, val);
}

bool i2cdev_write_reg_dev(size_t dev_idx, uint8_t reg, uint8_t val)
{
    i2c_device_config_t *cfg = i2cdev_cfg_by_idx(dev_idx);
    if (!cfg) return false;
    if (reg >= cfg->reg_count) return false;
    if (!i2cdev_is_value_permitted_cfg(cfg, reg, val)) return false;
    return i2cdev_write_byte(dev_idx, cfg->addr_7b, reg, val);
}
