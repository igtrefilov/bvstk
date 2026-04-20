#include "i2c_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "lwip/sockets.h"

#include "../../bvstk_i2c/bvstk_i2c.h"
#include "../../config/config_store.h"
#include "../../dcp2/dcp2_notify.h"

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

static bool rule_contains(const i2c_rule_entry_t *rules, size_t len, uint8_t reg, uint8_t val)
{
    if (!rules) return false;
    for (size_t i = 0; i < len; ++i) {
        if (rules[i].reg == reg && rules[i].val == val) return true;
    }
    return false;
}

static bool parse_selector(const char *tok, size_t *out_idx, const i2c_device_config_t **out_cfg)
{
    if (out_idx) *out_idx = 0;
    if (out_cfg) *out_cfg = NULL;
    if (!tok || !tok[0]) return false;

    size_t idx = 0;
    const i2c_device_config_t *cfg = NULL;

    if (tok[0] == '@') {
        bool ok = false;
        unsigned long a = parse_num(tok + 1, &ok);
        if (!ok || a > 0x7F) return false;
        if (!i2cdev_find_device_index_by_addr((uint8_t)a, &idx)) return false;
        cfg = config_store_find_i2c_device_by_addr((uint8_t)a);
    } else {
        if (i2cdev_find_device_index_by_name(tok, &idx)) {
            cfg = config_store_find_i2c_device_by_name(tok);
        } else {
            bool ok = false;
            unsigned long a = parse_num(tok, &ok);
            if (ok && a <= 0x7F) {
                if (!i2cdev_find_device_index_by_addr((uint8_t)a, &idx)) return false;
                cfg = config_store_find_i2c_device_by_addr((uint8_t)a);
            } else {
                return false;
            }
        }
    }

    if (!cfg) {
        i2cdev_device_info_t info;
        if (!i2cdev_device_get_info(idx, &info)) return false;
        cfg = config_store_find_i2c_device_by_name(info.name);
        if (!cfg) return false;
    }

    if (out_idx) *out_idx = idx;
    if (out_cfg) *out_cfg = cfg;
    return true;
}

static void persist_cfg(int fd, const i2c_device_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    int saved = config_store_save_i2c_device(cfg);
    if (!saved) write_str(fd, "WARN: failed to save to flash:/config/i2c/<device>.json\r\n");
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

static void cmd_info(int fd, size_t idx, const i2c_device_config_t *cfg)
{
    i2cdev_device_info_t d;
    if (!i2cdev_device_get_info(idx, &d)) { write_str(fd, "ERR (no device)\r\n"); return; }
    const char *pol = (i2cdev_get_policy_dev(idx) == I2CDEV_POLICY_BLACKLIST) ? "blacklist" : "whitelist";
    i2c_writef(fd, "selected: %s addr=0x%02X regs=%u max_value=%u policy=%s\r\n",
               d.name ? d.name : "?", (unsigned)d.addr_7b, (unsigned)d.reg_count, (unsigned)d.max_value_code, pol);
    if (cfg) {
        i2c_writef(fd, "file=%s whitelist_len=%u blacklist_len=%u settings_len=%u autopoll=%s regs_len=%u\r\n",
                   cfg->file_name[0] ? cfg->file_name : "?",
                   (unsigned)cfg->whitelist_len,
                   (unsigned)cfg->blacklist_len,
                   (unsigned)cfg->settings_len,
                   cfg->autopoll_enabled ? "on" : "off",
                   (unsigned)cfg->autopoll_regs_len);
    }
}

static void cmd_r(int fd, size_t idx, const char *s_reg)
{
    if (!s_reg) { write_str(fd, "ERR\r\n"); return; }
    bool ok = false;
    unsigned long reg = parse_num(s_reg, &ok);
    if (!ok || reg > 0xFF) { write_str(fd, "ERR\r\n"); return; }
    uint8_t v = 0;
    if (!i2cdev_read_reg_cached_dev(idx, (uint8_t)reg, &v)) { write_str(fd, "ERR\r\n"); return; }
    i2c_writef(fd, "OK REG 0x%02lX = 0x%02X %u\r\n", reg, v, v);
}

static void cmd_w(int fd, size_t idx, const i2c_device_config_t *cfg, const char *s_reg, const char *s_val)
{
    if (!s_reg || !s_val) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false, okv = false;
    unsigned long reg = parse_num(s_reg, &okr);
    unsigned long val = parse_num(s_val, &okv);
    if (!okr || !okv || reg > 0xFF || val > 0xFF) { write_str(fd, "ERR\r\n"); return; }
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    if ((uint8_t)reg >= cfg->reg_count) {
        i2c_writef(fd, "ERR DENIED reg=0x%02lX outside range 0x00..0x%02X\r\n",
                   reg,
                   (unsigned)(cfg->reg_count ? (cfg->reg_count - 1u) : 0u));
        return;
    }
    if ((uint8_t)val > cfg->max_value_code) {
        i2c_writef(fd, "ERR DENIED val=0x%02lX exceeds max_value_code=0x%02X\r\n",
                   val,
                   (unsigned)cfg->max_value_code);
        return;
    }
    if (!i2cdev_write_reg_dev_source(idx, (uint8_t)reg, (uint8_t)val, (uint8_t)DCP2_NOTIFY_SOURCE_TELNET)) {
        const bool allowed = (cfg->policy == I2C_POLICY_WHITELIST)
            ? rule_contains(cfg->whitelist, cfg->whitelist_len, (uint8_t)reg, (uint8_t)val)
            : !rule_contains(cfg->blacklist, cfg->blacklist_len, (uint8_t)reg, (uint8_t)val);
        if (!allowed) {
            if (cfg->policy == I2C_POLICY_WHITELIST) {
                i2c_writef(fd,
                           "ERR DENIED policy=whitelist reg=0x%02lX val=0x%02lX whitelist_len=%u (see: i2c %s policy show rules)\r\n",
                           reg, val, (unsigned)cfg->whitelist_len, cfg->name);
            } else {
                i2c_writef(fd,
                           "ERR DENIED policy=blacklist reg=0x%02lX val=0x%02lX is blocked (see: i2c %s policy show rules)\r\n",
                           reg, val, cfg->name);
            }
        } else {
            i2c_writef(fd, "ERR WRITE_FAILED reg=0x%02lX val=0x%02lX\r\n", reg, val);
        }
        return;
    }
    write_str(fd, "OK\r\n");
}

static void cmd_policy(int fd, size_t idx, const i2c_device_config_t *cfg, const char *mode)
{
    if (!mode) { write_str(fd, "ERR\r\n"); return; }
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    if (strcasecmp(mode, "whitelist") == 0) { (void)i2cdev_set_policy_dev(idx, I2CDEV_POLICY_WHITELIST); persist_cfg(fd, cfg); write_str(fd, "OK\r\n"); return; }
    if (strcasecmp(mode, "blacklist") == 0) { (void)i2cdev_set_policy_dev(idx, I2CDEV_POLICY_BLACKLIST); persist_cfg(fd, cfg); write_str(fd, "OK\r\n"); return; }
    write_str(fd, "ERR\r\n");
}

static void cmd_addr(int fd, size_t idx, const i2c_device_config_t *cfg, const char *s_addr)
{
    (void)idx;
    if (!cfg || !s_addr) { write_str(fd, "ERR\r\n"); return; }
    bool ok = false;
    unsigned long a = parse_num(s_addr, &ok);
    if (!ok || a > 0x7Ful) { write_str(fd, "ERR\r\n"); return; }
    i2c_device_config_t *mut = (i2c_device_config_t *)cfg;
    taskENTER_CRITICAL();
    mut->addr_7b = (uint8_t)(a & 0x7Fu);
    taskEXIT_CRITICAL();
    persist_cfg(fd, cfg);
    write_str(fd, "OK\r\n");
}

static void cmd_rules(int fd, size_t idx, const i2c_device_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    const char *pol = (i2cdev_get_policy_dev(idx) == I2CDEV_POLICY_BLACKLIST) ? "BLACKLIST" : "WHITELIST";
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

static void cmd_rule_list_show(int fd, const char *title, const i2c_rule_entry_t *rules, size_t len)
{
    i2c_writef(fd, "%s (%u):\r\n", title, (unsigned)len);
    for (size_t i = 0; i < len; ++i) {
        i2c_writef(fd, "  { reg:0x%02X val:0x%02X }\r\n", rules[i].reg, rules[i].val);
    }
}

static void cmd_policy_show(int fd, size_t idx, const i2c_device_config_t *cfg, const char *what)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    if (!what) {
        const char *pol = (i2cdev_get_policy_dev(idx) == I2CDEV_POLICY_BLACKLIST) ? "BLACKLIST" : "WHITELIST";
        i2c_writef(fd, "POLICY=%s\r\n", pol);
        return;
    }
    if (strcasecmp(what, "rules") == 0) {
        cmd_rules(fd, idx, cfg);
        return;
    }
    if (strcasecmp(what, "whitelist") == 0) {
        cmd_rule_list_show(fd, "WHITELIST", cfg->whitelist, cfg->whitelist_len);
        return;
    }
    if (strcasecmp(what, "blacklist") == 0) {
        cmd_rule_list_show(fd, "BLACKLIST", cfg->blacklist, cfg->blacklist_len);
        return;
    }
    write_str(fd, "ERR\r\n");
}

static void cmd_policy_list_edit(int fd, size_t idx, const i2c_device_config_t *cfg,
                                 const char *list_name, const char *action,
                                 const char *s_reg, const char *s_val)
{
    if (!cfg || !list_name || !action) { write_str(fd, "ERR\r\n"); return; }

    if (strcasecmp(action, "clear") == 0) {
        i2c_device_config_t *mut = (i2c_device_config_t *)cfg;
        taskENTER_CRITICAL();
        if (strcasecmp(list_name, "whitelist") == 0) {
            mut->whitelist_len = 0;
        } else if (strcasecmp(list_name, "blacklist") == 0) {
            mut->blacklist_len = 0;
        } else {
            taskEXIT_CRITICAL();
            write_str(fd, "ERR\r\n");
            return;
        }
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }

    if (!s_reg || !s_val) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false, okv = false;
    unsigned long reg = parse_num(s_reg, &okr);
    unsigned long val = parse_num(s_val, &okv);
    if (!okr || !okv || reg > 0xFF || val > 0xFF) { write_str(fd, "ERR\r\n"); return; }

    bool ok = false;
    if (strcasecmp(list_name, "whitelist") == 0) {
        if (strcasecmp(action, "add") == 0) ok = i2cdev_rule_allow_dev(idx, (uint8_t)reg, (uint8_t)val);
        else if (strcasecmp(action, "del") == 0 || strcasecmp(action, "delete") == 0) ok = i2cdev_rule_clear_dev(idx, (uint8_t)reg, (uint8_t)val);
        else { write_str(fd, "ERR\r\n"); return; }
    } else if (strcasecmp(list_name, "blacklist") == 0) {
        if (strcasecmp(action, "add") == 0) ok = i2cdev_rule_deny_dev(idx, (uint8_t)reg, (uint8_t)val);
        else if (strcasecmp(action, "del") == 0 || strcasecmp(action, "delete") == 0) ok = i2cdev_rule_clear_dev(idx, (uint8_t)reg, (uint8_t)val);
        else { write_str(fd, "ERR\r\n"); return; }
    } else {
        write_str(fd, "ERR\r\n");
        return;
    }

    if (ok) persist_cfg(fd, cfg);
    write_str(fd, ok ? "OK\r\n" : "ERR\r\n");
}

static void cmd_policy_dispatch(int fd, size_t idx, const i2c_device_config_t *cfg, char **save)
{
    char *sub = strtok_r(NULL, " \t", save);
    if (!sub) {
        cmd_policy_show(fd, idx, cfg, NULL);
        return;
    }

    if (strcasecmp(sub, "show") == 0) {
        cmd_policy_show(fd, idx, cfg, strtok_r(NULL, " \t", save));
        return;
    }

    if (strcasecmp(sub, "set") == 0) {
        cmd_policy(fd, idx, cfg, strtok_r(NULL, " \t", save));
        return;
    }

    if (strcasecmp(sub, "whitelist") == 0 || strcasecmp(sub, "blacklist") == 0) {
        char *action = strtok_r(NULL, " \t", save);
        if (!action) { write_str(fd, "ERR\r\n"); return; }
        cmd_policy_list_edit(fd, idx, cfg, sub, action,
                             strtok_r(NULL, " \t", save),
                             strtok_r(NULL, " \t", save));
        return;
    }

    write_str(fd, "ERR\r\n");
}

static void cmd_autopoll_show(int fd, const i2c_device_config_t *cfg)
{
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

    size_t idx = 0;
    const i2c_device_config_t *cfg = NULL;
    if (!parse_selector(sub, &idx, &cfg)) { write_str(fd, "ERR (device not found)\r\n"); return true; }

    char *cmd = strtok_r(NULL, " \t", save);
    if (!cmd) { cmd_info(fd, idx, cfg); return true; }

    if (strcasecmp(cmd, "info") == 0) { cmd_info(fd, idx, cfg); return true; }
    if (strcasecmp(cmd, "r") == 0) { cmd_r(fd, idx, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(cmd, "w") == 0) { cmd_w(fd, idx, cfg, strtok_r(NULL, " \t", save), strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(cmd, "addr") == 0 || strcasecmp(cmd, "address") == 0) { cmd_addr(fd, idx, cfg, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(cmd, "policy") == 0) { cmd_policy_dispatch(fd, idx, cfg, save); return true; }
    if (strcasecmp(cmd, "autopoll") == 0) { cmd_autopoll_show(fd, cfg); return true; }

    write_str(fd, "ERR\r\n");
    return true;
}

void i2c_help(int fd)
{
    write_str(fd, "i2c usage:\r\n");
    write_str(fd, "  i2c list\r\n");
    write_str(fd, "  i2c <name> [info]\r\n");
    write_str(fd, "  i2c <name> r <reg>\r\n");
    write_str(fd, "  i2c <name> w <reg> <val>\r\n");
    write_str(fd, "  i2c <name> addr <addr_7b>   (set 7-bit address, persists)\r\n");
    write_str(fd, "  i2c <name> policy\r\n");
    write_str(fd, "  i2c <name> policy show [rules|whitelist|blacklist]\r\n");
    write_str(fd, "  i2c <name> policy set <whitelist|blacklist>\r\n");
    write_str(fd, "  i2c <name> policy whitelist add <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy whitelist del <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy whitelist clear\r\n");
    write_str(fd, "  i2c <name> policy blacklist add <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy blacklist del <reg> <val>\r\n");
    write_str(fd, "  i2c <name> policy blacklist clear\r\n");
    write_str(fd, "  i2c <name> autopoll\r\n");
    write_str(fd, "  (address selector: use @0x.. instead of <name>, e.g. `i2c @0x36 r 0x10`)\r\n");
}
