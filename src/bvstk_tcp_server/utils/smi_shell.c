#include "smi_shell.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdint.h>

#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"

#include "../../bvstk_smi/bvstk_smi.h"
#include "../../config/config_store.h"

static void smi_writef(int fd, const char *fmt, ...)
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

static bool parse_selector(const char *tok, size_t *out_idx, const smi_phy_config_t **out_cfg)
{
    if (out_idx) *out_idx = 0;
    if (out_cfg) *out_cfg = NULL;
    if (!tok || !tok[0]) return false;

    const smi_phy_config_t *cfg = NULL;

    if (tok[0] == '@') {
        bool ok = false;
        unsigned long p = parse_num(tok + 1, &ok);
        if (!ok || p > 31ul) return false;
        cfg = config_store_find_smi_device_by_phy((uint8_t)p);
    } else {
        cfg = config_store_find_smi_device_by_name(tok);
        if (!cfg) {
            bool ok = false;
            unsigned long p = parse_num(tok, &ok);
            if (!ok || p > 31ul) return false;
            cfg = config_store_find_smi_device_by_phy((uint8_t)p);
        }
    }

    if (!cfg) return false;

    if (out_idx) {
        const smi_phy_config_t *cfgs = config_store_get_smi_devices();
        size_t n = config_store_get_smi_device_count();
        size_t idx = 0;
        for (idx = 0; idx < n; ++idx) {
            if (&cfgs[idx] == cfg) break;
        }
        if (idx >= n) idx = 0;
        *out_idx = idx;
    }
    if (out_cfg) *out_cfg = cfg;
    return true;
}

static void persist_cfg(int fd, const smi_phy_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    int saved = config_store_save_smi_device(cfg);
    if (!saved) write_str(fd, "WARN: failed to save to flash:/config/smi/<device>.json\r\n");
}

static void cmd_list(int fd)
{
    const smi_phy_config_t *cfgs = config_store_get_smi_devices();
    size_t n = config_store_get_smi_device_count();
    smi_writef(fd, "SMI devices: %u\r\n", (unsigned)n);
    for (size_t i = 0; i < n; ++i) {
        const smi_phy_config_t *c = &cfgs[i];
        const char *pol = (c->policy == SMI_POLICY_BLACKLIST) ? "blacklist" : "whitelist";
        smi_writef(fd, " %u: %s phy=%u regs=%u policy=%s\r\n",
                   (unsigned)i,
                   c->name[0] ? c->name : "?",
                   (unsigned)(c->phy_addr & 0x1Fu),
                   (unsigned)c->reg_count,
                   pol);
    }
}

static void cmd_info(int fd, const smi_phy_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    const char *pol = (cfg->policy == SMI_POLICY_BLACKLIST) ? "blacklist" : "whitelist";
    smi_writef(fd, "selected: %s phy=%u regs=%u policy=%s\r\n",
               cfg->name[0] ? cfg->name : "?",
               (unsigned)(cfg->phy_addr & 0x1Fu),
               (unsigned)cfg->reg_count,
               pol);
    smi_writef(fd, "autopoll: %s reg_delay_ms=%u cycle_delay_ms=%u regs_len=%u\r\n",
               cfg->autopoll_enabled ? "on" : "off",
               (unsigned)cfg->autopoll_reg_delay_ms,
               (unsigned)cfg->autopoll_cycle_delay_ms,
               (unsigned)cfg->autopoll_regs_len);
    smi_writef(fd, "write_allow_regs_len=%u write_deny_regs_len=%u settings_len=%u\r\n",
               (unsigned)cfg->write_allow_regs_len,
               (unsigned)cfg->write_deny_regs_len,
               (unsigned)cfg->settings_len);
}

static void cmd_r(int fd, uint8_t phy, const char *s_reg)
{
    if (!s_reg) { write_str(fd, "ERR\r\n"); return; }
    bool ok = false;
    unsigned long reg = parse_num(s_reg, &ok);
    if (!ok || reg > 31ul) { write_str(fd, "ERR\r\n"); return; }
    uint16_t val = 0;
    if (!mdio_read_blocking((uint8_t)(phy & 0x1Fu), (uint8_t)reg, &val, pdMS_TO_TICKS(100))) {
        write_str(fd, "ERR\r\n");
        return;
    }
    smi_writef(fd, "OK REG 0x%02lX = 0x%04" PRIX16 " %" PRIu16 "\r\n", reg, val, val);
}

static void cmd_w(int fd, uint8_t phy, const char *s_reg, const char *s_val)
{
    if (!s_reg || !s_val) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false, okv = false;
    unsigned long reg = parse_num(s_reg, &okr);
    unsigned long val = parse_num(s_val, &okv);
    if (!okr || !okv || reg > 31ul || val > 0xFFFFul) { write_str(fd, "ERR\r\n"); return; }
    if (!smi_write_checked((uint8_t)(phy & 0x1Fu), (uint8_t)reg, (uint16_t)val)) {
        write_str(fd, "ERR DENIED\r\n");
        return;
    }
    write_str(fd, "OK\r\n");
}

static void cmd_policy(int fd, smi_phy_config_t *cfg, const char *mode)
{
    if (!cfg || !mode) { write_str(fd, "ERR\r\n"); return; }
    if (strcasecmp(mode, "whitelist") == 0) {
        taskENTER_CRITICAL();
        cfg->policy = SMI_POLICY_WHITELIST;
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }
    if (strcasecmp(mode, "blacklist") == 0) {
        taskENTER_CRITICAL();
        cfg->policy = SMI_POLICY_BLACKLIST;
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }
    write_str(fd, "ERR\r\n");
}

static int u8_list_contains(const uint8_t *a, size_t len, uint8_t v)
{
    if (!a) return 0;
    for (size_t i = 0; i < len; ++i) {
        if (a[i] == v) return 1;
    }
    return 0;
}

static void u8_list_add(uint8_t *a, size_t *len, size_t max, uint8_t v)
{
    if (!a || !len) return;
    if (u8_list_contains(a, *len, v)) return;
    if (*len < max) a[(*len)++] = v;
}

static void u8_list_remove(uint8_t *a, size_t *len, uint8_t v)
{
    if (!a || !len) return;
    size_t w = 0;
    for (size_t i = 0; i < *len; ++i) {
        if (a[i] == v) continue;
        a[w++] = a[i];
    }
    *len = w;
}

static void cmd_rule_edit(int fd, smi_phy_config_t *cfg, const char *op, const char *s_reg)
{
    if (!cfg || !op || !s_reg) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false;
    unsigned long reg = parse_num(s_reg, &okr);
    if (!okr || reg > 31ul) { write_str(fd, "ERR\r\n"); return; }
    if (reg >= cfg->reg_count) { write_str(fd, "ERR\r\n"); return; }

    taskENTER_CRITICAL();
    if (strcasecmp(op, "allow") == 0) {
        u8_list_add(cfg->write_allow_regs, &cfg->write_allow_regs_len, SMI_CFG_WRITE_REGS_MAX, (uint8_t)reg);
    } else if (strcasecmp(op, "deny") == 0) {
        u8_list_add(cfg->write_deny_regs, &cfg->write_deny_regs_len, SMI_CFG_WRITE_REGS_MAX, (uint8_t)reg);
    } else if (strcasecmp(op, "clear") == 0) {
        u8_list_remove(cfg->write_allow_regs, &cfg->write_allow_regs_len, (uint8_t)reg);
        u8_list_remove(cfg->write_deny_regs, &cfg->write_deny_regs_len, (uint8_t)reg);
    } else {
        taskEXIT_CRITICAL();
        write_str(fd, "ERR\r\n");
        return;
    }
    taskEXIT_CRITICAL();

    persist_cfg(fd, cfg);
    write_str(fd, "OK\r\n");
}

static void cmd_rules(int fd, const smi_phy_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    const char *pol = (cfg->policy == SMI_POLICY_BLACKLIST) ? "BLACKLIST" : "WHITELIST";
    smi_writef(fd, "POLICY=%s\r\n", pol);
    smi_writef(fd, "WRITE_ALLOW (%u):\r\n", (unsigned)cfg->write_allow_regs_len);
    for (size_t i = 0; i < cfg->write_allow_regs_len; ++i) smi_writef(fd, "  0x%02X\r\n", cfg->write_allow_regs[i]);
    smi_writef(fd, "WRITE_DENY (%u):\r\n", (unsigned)cfg->write_deny_regs_len);
    for (size_t i = 0; i < cfg->write_deny_regs_len; ++i) smi_writef(fd, "  0x%02X\r\n", cfg->write_deny_regs[i]);
}

static void cmd_autopoll_show(int fd, const smi_phy_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    smi_writef(fd, "autopoll: %s reg_delay_ms=%u cycle_delay_ms=%u regs_len=%u\r\n",
               cfg->autopoll_enabled ? "on" : "off",
               (unsigned)cfg->autopoll_reg_delay_ms,
               (unsigned)cfg->autopoll_cycle_delay_ms,
               (unsigned)cfg->autopoll_regs_len);
    if (cfg->autopoll_regs_len) {
        write_str(fd, "regs:");
        for (size_t i = 0; i < cfg->autopoll_regs_len; ++i) smi_writef(fd, " 0x%02X", cfg->autopoll_regs[i]);
        write_str(fd, "\r\n");
    }
}

static void cmd_autopoll_set(int fd, smi_phy_config_t *cfg, const char *sub, char **save)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    if (!sub) { cmd_autopoll_show(fd, cfg); return; }

    if (strcasecmp(sub, "on") == 0 || strcasecmp(sub, "off") == 0) {
        taskENTER_CRITICAL();
        cfg->autopoll_enabled = (strcasecmp(sub, "on") == 0);
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }

    if (strcasecmp(sub, "reg_delay") == 0) {
        char *s_ms = strtok_r(NULL, " \t", save);
        if (!s_ms) { write_str(fd, "ERR\r\n"); return; }
        bool ok = false;
        unsigned long ms = parse_num(s_ms, &ok);
        if (!ok || ms > 60000ul) { write_str(fd, "ERR\r\n"); return; }
        taskENTER_CRITICAL();
        cfg->autopoll_reg_delay_ms = (uint32_t)ms;
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }

    if (strcasecmp(sub, "cycle_delay") == 0) {
        char *s_ms = strtok_r(NULL, " \t", save);
        if (!s_ms) { write_str(fd, "ERR\r\n"); return; }
        bool ok = false;
        unsigned long ms = parse_num(s_ms, &ok);
        if (!ok || ms > 600000ul) { write_str(fd, "ERR\r\n"); return; }
        taskENTER_CRITICAL();
        cfg->autopoll_cycle_delay_ms = (uint32_t)(ms ? ms : 1ul);
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }

    if (strcasecmp(sub, "regs") == 0) {
        taskENTER_CRITICAL();
        cfg->autopoll_regs_len = 0;
        for (;;) {
            char *s_reg = strtok_r(NULL, " \t", save);
            if (!s_reg) break;
            bool ok = false;
            unsigned long r = parse_num(s_reg, &ok);
            if (!ok || r > 31ul || r >= cfg->reg_count) continue;
            if (cfg->autopoll_regs_len < SMI_CFG_AUTOPOLL_REGS_MAX) {
                cfg->autopoll_regs[cfg->autopoll_regs_len++] = (uint8_t)r;
            }
        }
        taskEXIT_CRITICAL();
        persist_cfg(fd, cfg);
        write_str(fd, "OK\r\n");
        return;
    }

    write_str(fd, "ERR\r\n");
}

static void cmd_settings_show(int fd, const smi_phy_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    smi_writef(fd, "SETTINGS (%u):\r\n", (unsigned)cfg->settings_len);
    for (size_t i = 0; i < cfg->settings_len; ++i) {
        smi_writef(fd, "  { reg:0x%02X val:0x%04X }\r\n", cfg->settings[i].reg, (unsigned)cfg->settings[i].val);
    }
}

static void cmd_settings_clear(int fd, smi_phy_config_t *cfg)
{
    if (!cfg) { write_str(fd, "ERR (no device)\r\n"); return; }
    taskENTER_CRITICAL();
    cfg->settings_len = 0;
    taskEXIT_CRITICAL();
    persist_cfg(fd, cfg);
    write_str(fd, "OK\r\n");
}

static void cmd_help_smi(int fd)
{
    write_str(fd, "smi usage:\r\n");
    write_str(fd, "  smi -h|--help\r\n");
    write_str(fd, "  smi list\r\n");
    write_str(fd, "  smi r <phy> <reg>\r\n");
    write_str(fd, "  smi w <phy> <reg> <data>\r\n");
    write_str(fd, "  smi <name|phy|@phy> [info]\r\n");
    write_str(fd, "  smi <sel> r <reg>\r\n");
    write_str(fd, "  smi <sel> w <reg> <data>\r\n");
    write_str(fd, "  smi <sel> rules\r\n");
    write_str(fd, "  smi <sel> policy <whitelist|blacklist>\r\n");
    write_str(fd, "  smi <sel> allow <reg>\r\n");
    write_str(fd, "  smi <sel> deny <reg>\r\n");
    write_str(fd, "  smi <sel> clear <reg>\r\n");
    write_str(fd, "  smi <sel> autopoll [on|off|reg_delay <ms>|cycle_delay <ms>|regs <r0> <r1> ...]\r\n");
    write_str(fd, "  smi <sel> settings [clear]\r\n");
    write_str(fd, "  smi <sel> save\r\n");
}

bool smi_handle(char *tok, char **save, int fd)
{
    if (!tok || strcasecmp(tok, "smi") != 0) return false;

    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0) { cmd_help_smi(fd); return true; }

    if (strcasecmp(sub, "list") == 0) { cmd_list(fd); return true; }

    /* Legacy forms: smi r <phy> <reg>, smi w <phy> <reg> <data> */
    if (strcasecmp(sub, "r") == 0) {
        char *s_phy = strtok_r(NULL, " \t", save);
        char *s_reg = strtok_r(NULL, " \t", save);
        if (!s_phy || !s_reg) { write_str(fd, "ERR\r\n"); return true; }
        bool ok = false;
        unsigned long phy = parse_num(s_phy, &ok);
        if (!ok || phy > 31ul) { write_str(fd, "ERR\r\n"); return true; }
        cmd_r(fd, (uint8_t)phy, s_reg);
        return true;
    }
    if (strcasecmp(sub, "w") == 0) {
        char *s_phy = strtok_r(NULL, " \t", save);
        char *s_reg = strtok_r(NULL, " \t", save);
        char *s_dat = strtok_r(NULL, " \t", save);
        if (!s_phy || !s_reg || !s_dat) { write_str(fd, "ERR\r\n"); return true; }
        bool ok = false;
        unsigned long phy = parse_num(s_phy, &ok);
        if (!ok || phy > 31ul) { write_str(fd, "ERR\r\n"); return true; }
        cmd_w(fd, (uint8_t)phy, s_reg, s_dat);
        return true;
    }

    const smi_phy_config_t *cfg_c = NULL;
    if (!parse_selector(sub, NULL, &cfg_c)) { write_str(fd, "ERR (device not found)\r\n"); return true; }
    smi_phy_config_t *cfg = (smi_phy_config_t *)cfg_c;

    char *cmd = strtok_r(NULL, " \t", save);
    if (!cmd) { cmd_info(fd, cfg); return true; }

    if (strcasecmp(cmd, "info") == 0) { cmd_info(fd, cfg); return true; }
    if (strcasecmp(cmd, "r") == 0) { cmd_r(fd, cfg->phy_addr, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(cmd, "w") == 0) { cmd_w(fd, cfg->phy_addr, strtok_r(NULL, " \t", save), strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(cmd, "policy") == 0) { cmd_policy(fd, cfg, strtok_r(NULL, " \t", save)); return true; }
    if (strcasecmp(cmd, "rules") == 0) { cmd_rules(fd, cfg); return true; }
    if (strcasecmp(cmd, "allow") == 0 || strcasecmp(cmd, "deny") == 0 || strcasecmp(cmd, "clear") == 0) {
        cmd_rule_edit(fd, cfg, cmd, strtok_r(NULL, " \t", save));
        return true;
    }
    if (strcasecmp(cmd, "autopoll") == 0) {
        cmd_autopoll_set(fd, cfg, strtok_r(NULL, " \t", save), save);
        return true;
    }
    if (strcasecmp(cmd, "settings") == 0) {
        char *s2 = strtok_r(NULL, " \t", save);
        if (!s2) { cmd_settings_show(fd, cfg); return true; }
        if (strcasecmp(s2, "clear") == 0) { cmd_settings_clear(fd, cfg); return true; }
        write_str(fd, "ERR\r\n");
        return true;
    }
    if (strcasecmp(cmd, "save") == 0) { persist_cfg(fd, cfg); write_str(fd, "OK\r\n"); return true; }

    write_str(fd, "ERR\r\n");
    return true;
}

void smi_help(int fd)
{
    cmd_help_smi(fd);
}
