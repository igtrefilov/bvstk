#include "i2c_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "lwip/sockets.h"

#include "../../bvstk_i2c/bvstk_i2c.h"
#include "../../config/config_store.h"

static void i2c_writef(int fd, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    (void)lwip_write(fd, buf, n);
}

static const i2c_device_config_t *current_cfg(void)
{
    i2cdev_device_info_t info;
    if (!i2cdev_get_selected_info(&info) || !info.name || !info.name[0]) return NULL;
    return config_store_find_i2c_device_by_name(info.name);
}

static void persist_current(int fd)
{
    const i2c_device_config_t *cfg = current_cfg();
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    int saved = config_store_save_i2c_device(cfg);
    if (!saved) write_str(fd, "WARN: failed to save to flash:/configs/i2c/<device>.json\r\n");
}

static void cmd_list(int fd)
{
    i2cdev_device_info_t sel;
    bool has_sel = i2cdev_get_selected_info(&sel);
    size_t n = i2cdev_device_count();
    i2c_writef(fd, "I2C devices: %u\r\n", (unsigned)n);
    for (size_t i = 0; i < n; ++i) {
        i2cdev_device_info_t d;
        if (!i2cdev_device_get_info(i, &d)) continue;
        const char *mark = (has_sel && d.name && sel.name && strcmp(d.name, sel.name) == 0) ? "*" : " ";
        i2c_writef(fd, "%s %u: %s addr=0x%02X regs=%u max_value=%u\r\n",
                   mark, (unsigned)i,
                   d.name ? d.name : "?",
                   (unsigned)d.addr_7b,
                   (unsigned)d.reg_count,
                   (unsigned)d.max_value_code);
    }
}

static void cmd_info(int fd)
{
    i2cdev_device_info_t d;
    if (!i2cdev_get_selected_info(&d)) { write_str(fd, "ERR (no device)\r\n"); return; }
    const char *pol = (i2cdev_get_policy() == I2CDEV_POLICY_BLACKLIST) ? "blacklist" : "whitelist";
    i2c_writef(fd, "selected: %s addr=0x%02X regs=%u max_value=%u policy=%s\r\n",
               d.name ? d.name : "?", (unsigned)d.addr_7b, (unsigned)d.reg_count, (unsigned)d.max_value_code, pol);
}

static void cmd_use(int fd, const char *name)
{
    if (!name) { write_str(fd, "ERR\r\n"); return; }
    if (!i2cdev_select_device(name)) { write_str(fd, "ERR (not found)\r\n"); return; }
    cmd_info(fd);
}

static void cmd_addr(int fd, const char *s_addr)
{
    if (!s_addr) { write_str(fd, "ERR\r\n"); return; }
    bool ok = false;
    unsigned long a = parse_num(s_addr, &ok);
    if (!ok || a > 0x7F) { write_str(fd, "ERR\r\n"); return; }
    if (!i2cdev_select_device_addr((uint8_t)a)) { write_str(fd, "ERR (not found)\r\n"); return; }
    cmd_info(fd);
}

static void cmd_r(int fd, const char *s_reg)
{
    if (!s_reg) { write_str(fd, "ERR\r\n"); return; }
    bool ok = false;
    unsigned long reg = parse_num(s_reg, &ok);
    if (!ok || reg > 0xFF) { write_str(fd, "ERR\r\n"); return; }
    uint8_t v = 0;
    if (!i2cdev_read_reg((uint8_t)reg, &v)) { write_str(fd, "ERR\r\n"); return; }
    i2c_writef(fd, "OK REG 0x%02lX = 0x%02X %u\r\n", reg, v, v);
}

static void cmd_w(int fd, const char *s_reg, const char *s_val)
{
    if (!s_reg || !s_val) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false, okv = false;
    unsigned long reg = parse_num(s_reg, &okr);
    unsigned long val = parse_num(s_val, &okv);
    if (!okr || !okv || reg > 0xFF || val > 0xFF) { write_str(fd, "ERR\r\n"); return; }
    if (!i2cdev_write_reg((uint8_t)reg, (uint8_t)val)) { write_str(fd, "ERR DENIED\r\n"); return; }
    write_str(fd, "OK\r\n");
}

static void cmd_policy(int fd, const char *mode)
{
    if (!mode) { write_str(fd, "ERR\r\n"); return; }
    if (strcasecmp(mode, "whitelist") == 0) { i2cdev_set_policy(I2CDEV_POLICY_WHITELIST); persist_current(fd); write_str(fd, "OK\r\n"); return; }
    if (strcasecmp(mode, "blacklist") == 0) { i2cdev_set_policy(I2CDEV_POLICY_BLACKLIST); persist_current(fd); write_str(fd, "OK\r\n"); return; }
    write_str(fd, "ERR\r\n");
}

static void cmd_rule_edit(int fd, const char *op, const char *s_reg, const char *s_val)
{
    if (!op || !s_reg || !s_val) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false, okv = false;
    unsigned long reg = parse_num(s_reg, &okr);
    unsigned long val = parse_num(s_val, &okv);
    if (!okr || !okv || reg > 0xFF || val > 0xFF) { write_str(fd, "ERR\r\n"); return; }
    bool ok = false;
    if (strcasecmp(op, "allow") == 0) ok = i2cdev_rule_allow((uint8_t)reg, (uint8_t)val);
    else if (strcasecmp(op, "deny") == 0) ok = i2cdev_rule_deny((uint8_t)reg, (uint8_t)val);
    else if (strcasecmp(op, "clear") == 0) ok = i2cdev_rule_clear((uint8_t)reg, (uint8_t)val);
    else { write_str(fd, "ERR\r\n"); return; }
    if (ok) persist_current(fd);
    write_str(fd, ok ? "OK\r\n" : "ERR\r\n");
}

static void cmd_rules(int fd)
{
    const i2c_device_config_t *cfg = current_cfg();
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    const char *pol = (i2cdev_get_policy() == I2CDEV_POLICY_BLACKLIST) ? "BLACKLIST" : "WHITELIST";
    i2c_writef(fd, "POLICY=%s\r\n", pol);
    i2c_writef(fd, "WHITELIST (%u):\r\n", (unsigned)cfg->whitelist_len);
    for (size_t i = 0; i < cfg->whitelist_len; ++i) {
        i2c_writef(fd, "  { reg:0x%02X val:0x%02X }\r\n", cfg->whitelist[i].reg, cfg->whitelist[i].val);
    }
    i2c_writef(fd, "BLACKLIST (%u):\r\n", (unsigned)cfg->blacklist_len);
    for (size_t i = 0; i < cfg->blacklist_len; ++i) {
        i2c_writef(fd, "  { reg:0x%02X val:0x%02X }\r\n", cfg->blacklist[i].reg, cfg->blacklist[i].val);
    }
}

static void cmd_autopoll_show(int fd)
{
    const i2c_device_config_t *cfg = current_cfg();
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    i2c_writef(fd, "autopoll: %s reg_delay_ms=%u cycle_delay_ms=%u regs_len=%u\r\n",
               cfg->autopoll_enabled ? "on" : "off",
               (unsigned)cfg->autopoll_reg_delay_ms,
               (unsigned)cfg->autopoll_cycle_delay_ms,
               (unsigned)cfg->autopoll_regs_len);
    if (cfg->autopoll_regs_len) {
        write_str(fd, "regs:");
        for (size_t i = 0; i < cfg->autopoll_regs_len; ++i) {
            i2c_writef(fd, " 0x%02X", cfg->autopoll_regs[i]);
        }
        write_str(fd, "\r\n");
    }
}

bool i2c_handle(char *tok, char **save, int fd)
{
    if (!tok || strcasecmp(tok, "i2c") != 0) return false;
    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0) { i2c_help(fd); return true; }

    if (strcasecmp(sub, "list") == 0) { cmd_list(fd); return true; }
    if (strcasecmp(sub, "info") == 0) { cmd_info(fd); return true; }
    if (strcasecmp(sub, "use") == 0 || strcasecmp(sub, "select") == 0) { cmd_use(fd, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(sub, "addr") == 0) { cmd_addr(fd, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(sub, "r") == 0) { cmd_r(fd, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(sub, "w") == 0) { cmd_w(fd, strtok_r(NULL, " \t", save), strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(sub, "policy") == 0) { cmd_policy(fd, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(sub, "rules") == 0) { cmd_rules(fd); return true; }
    if (strcasecmp(sub, "allow") == 0 || strcasecmp(sub, "deny") == 0 || strcasecmp(sub, "clear") == 0) {
        cmd_rule_edit(fd, sub, strtok_r(NULL, " \t", save), strtok_r(NULL, " \t", save));
        return true;
    }
    if (strcasecmp(sub, "save") == 0) { persist_current(fd); write_str(fd, "OK\r\n"); return true; }
    if (strcasecmp(sub, "autopoll") == 0) { cmd_autopoll_show(fd); return true; }

    write_str(fd, "ERR\r\n");
    return true;
}

void i2c_help(int fd)
{
    write_str(fd, "i2c usage:\r\n");
    write_str(fd, "  i2c list\r\n");
    write_str(fd, "  i2c info\r\n");
    write_str(fd, "  i2c use <name>\r\n");
    write_str(fd, "  i2c addr <0x..>\r\n");
    write_str(fd, "  i2c r <reg>\r\n");
    write_str(fd, "  i2c w <reg> <val>\r\n");
    write_str(fd, "  i2c rules\r\n");
    write_str(fd, "  i2c policy <whitelist|blacklist>\r\n");
    write_str(fd, "  i2c allow <reg> <val>\r\n");
    write_str(fd, "  i2c deny <reg> <val>\r\n");
    write_str(fd, "  i2c clear <reg> <val>\r\n");
    write_str(fd, "  i2c autopoll   (show current autopoll config)\r\n");
    write_str(fd, "  i2c save       (persist to flash:/configs/i2c/<device>.json)\r\n");
}
