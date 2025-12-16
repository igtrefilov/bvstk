#include "axp_shell.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>

#include "lwip/sockets.h"

#include "../../bvstk_i2c/bvstk_i2c.h"
#include "../../config/config_store.h"

static void cfg_rule_add(axp_rule_entry_t *rules, size_t *len, size_t max, uint8_t reg, uint8_t val)
{
    if (!rules || !len) return;
    for (size_t i = 0; i < *len; ++i) {
        if (rules[i].reg == reg && rules[i].val == val) return;
    }
    if (*len < max) {
        rules[*len].reg = reg;
        rules[*len].val = val;
        (*len)++;
    }
}

static void cfg_rule_remove(axp_rule_entry_t *rules, size_t *len, uint8_t reg, uint8_t val)
{
    if (!rules || !len) return;
    size_t w = 0;
    for (size_t i = 0; i < *len; ++i) {
        if (rules[i].reg == reg && rules[i].val == val) continue;
        rules[w++] = rules[i];
    }
    *len = w;
}

static void persist_cfg(int fd, const axp15060_config_t *cfg)
{
    if (!cfg) return;
    (void)config_store_set_axp15060(cfg);
    int saved = config_store_save_axp15060();
    if (!saved) {
        write_str(fd, "WARN: failed to save to flash:/configs/axp15060.json\r\n");
    }
}

static uint16_t dcdc12x_mv(uint8_t code)
{
    if (code <= 70) return (uint16_t)(500 + 10 * code);
    uint8_t k = (uint8_t)(code - 71);
    return (uint16_t)(1220 + 20 * (k < 17 ? k : 16));
}

static uint16_t dcdc5_mv(uint8_t code)
{
    if (code <= 32) return (uint16_t)(800 + 10 * code);
    uint8_t k = (uint8_t)(code - 33);
    return (uint16_t)(1140 + 20 * (k < 36 ? k : 35));
}

static uint16_t dcdc1_mv(uint8_t code5)
{
    uint8_t c = (uint8_t)(code5 & 0x1F);
    return (uint16_t)(1500 + 100 * c);
}

static uint16_t dcdc6_mv(uint8_t code5)
{
    uint8_t c = (uint8_t)(code5 & 0x1F);
    return (uint16_t)(500 + 100 * c);
}

static uint16_t ldo_0p7_step100mv(uint8_t code5)
{
    uint8_t c = (uint8_t)(code5 & 0x1F);
    return (uint16_t)(700 + 100 * c);
}

static uint16_t cldo4_mv(uint8_t code6)
{
    uint8_t c = (uint8_t)(code6 & 0x3F);
    return (uint16_t)(700 + 100 * c);
}

static uint16_t cpusldo_mv(uint8_t code4)
{
    uint8_t c = (uint8_t)(code4 & 0x0F);
    if (c == 0x0F) return 0;
    return (uint16_t)(700 + 50 * c);
}

static void axp_print_status_line(int fd, const char *name, int enabled, uint16_t mv, uint8_t code)
{
    char out[128];
    if (mv == 0) {
        int n = snprintf(out, sizeof(out), "%s: %s code=0x%02X\r\n", name, enabled ? "on" : "off", code);
        (void)lwip_write(fd, out, n);
    } else {
        int n = snprintf(out, sizeof(out), "%s: %s %u mV code=0x%02X\r\n", name, enabled ? "on" : "off", mv, code);
        (void)lwip_write(fd, out, n);
    }
}

static void print_bitmap_list(int fd, const char *title, const uint8_t map[I2CDEV_REG_COUNT][I2CDEV_MAX_VALUE_CODE + 1])
{
    char line[256];
    int printed_any = 0;
    int n = snprintf(line, sizeof(line), "%s\r\n", title);
    (void)lwip_write(fd, line, n);
    for (uint32_t r = 0; r < I2CDEV_REG_COUNT; ++r) {
        int first = 1;
        char buf[256];
        int m = 0;
        for (uint32_t v = 0; v <= I2CDEV_MAX_VALUE_CODE; ++v) {
            if (map[r][v]) {
                if (first) {
                    m += snprintf(buf + m, sizeof(buf) - m, "  reg 0x%02" PRIX32 ": ", r);
                    first = 0;
                } else {
                    m += snprintf(buf + m, sizeof(buf) - m, " ");
                }
                m += snprintf(buf + m, sizeof(buf) - m, "0x%02" PRIX32, v);
                if (m >= (int)sizeof(buf) - 8) break;
            }
        }
        if (!first) {
            printed_any = 1;
            m += snprintf(buf + m, sizeof(buf) - m, "\r\n");
            (void)lwip_write(fd, buf, m);
        }
    }
    if (!printed_any) {
        int k = snprintf(line, sizeof(line), "  <empty>\r\n");
        (void)lwip_write(fd, line, k);
    }
}

static void cmd_axp_status(int fd)
{
    uint8_t r00=0,r10=0,r11=0,r12=0,r13=0,r14=0,r15=0,r16=0,r17=0,r18=0;
    uint8_t r19=0,r20=0,r21=0,r22=0,r23=0,r24=0,r25=0,r26=0,r27=0,r28=0,r29=0,r2a=0,r2b=0,r2c=0,r2d=0,r2e=0;
    uint8_t r1a=0,r1b=0,r1e=0,r1f=0,r31=0,r32=0,r36=0,r40=0,r41=0,r48=0,r49=0;
    i2cdev_read_reg(0x00,&r00);
    i2cdev_read_reg(0x10,&r10);
    i2cdev_read_reg(0x11,&r11);
    i2cdev_read_reg(0x12,&r12);
    i2cdev_read_reg(0x13,&r13);
    i2cdev_read_reg(0x14,&r14);
    i2cdev_read_reg(0x15,&r15);
    i2cdev_read_reg(0x16,&r16);
    i2cdev_read_reg(0x17,&r17);
    i2cdev_read_reg(0x18,&r18);
    i2cdev_read_reg(0x19,&r19);
    i2cdev_read_reg(0x1A,&r1a);
    i2cdev_read_reg(0x1B,&r1b);
    i2cdev_read_reg(0x1E,&r1e);
    i2cdev_read_reg(0x1F,&r1f);
    i2cdev_read_reg(0x20,&r20);
    i2cdev_read_reg(0x21,&r21);
    i2cdev_read_reg(0x22,&r22);
    i2cdev_read_reg(0x23,&r23);
    i2cdev_read_reg(0x24,&r24);
    i2cdev_read_reg(0x25,&r25);
    i2cdev_read_reg(0x26,&r26);
    i2cdev_read_reg(0x27,&r27);
    i2cdev_read_reg(0x28,&r28);
    i2cdev_read_reg(0x29,&r29);
    i2cdev_read_reg(0x2A,&r2a);
    i2cdev_read_reg(0x2B,&r2b);
    i2cdev_read_reg(0x2C,&r2c);
    i2cdev_read_reg(0x2D,&r2d);
    i2cdev_read_reg(0x2E,&r2e);
    i2cdev_read_reg(0x31,&r31);
    i2cdev_read_reg(0x32,&r32);
    i2cdev_read_reg(0x36,&r36);
    i2cdev_read_reg(0x40,&r40);
    i2cdev_read_reg(0x41,&r41);
    i2cdev_read_reg(0x48,&r48);
    i2cdev_read_reg(0x49,&r49);
    char line[128];
    int n = snprintf(line,sizeof(line),"AXP15060 @0x%02X\r\n",I2CDEV_I2C_ADDR_7B);
    (void)lwip_write(fd,line,n);
    n = snprintf(line,sizeof(line),"SRC=0x%02X PWRCTRL=[10:0x%02X 11:0x%02X 12:0x%02X] MODE=[1A:0x%02X 1B:0x%02X] MON=0x%02X SET=0x%02X\r\n",r00,r10,r11,r12,r1a,r1b,r1e,r1f);
    (void)lwip_write(fd,line,n);
    axp_print_status_line(fd,"DCDC1",(r10&0x01)?1:0,dcdc1_mv(r13&0x1F),r13);
    axp_print_status_line(fd,"DCDC2",(r10&0x02)?1:0,dcdc12x_mv(r14&0x7F),r14);
    axp_print_status_line(fd,"DCDC3",(r10&0x04)?1:0,dcdc12x_mv(r15&0x7F),r15);
    axp_print_status_line(fd,"DCDC4",(r10&0x08)?1:0,dcdc12x_mv(r16&0x7F),r16);
    axp_print_status_line(fd,"DCDC5",(r10&0x10)?1:0,dcdc5_mv(r17&0x7F),r17);
    axp_print_status_line(fd,"DCDC6",(r10&0x20)?1:0,dcdc6_mv(r18&0x1F),r18);
    axp_print_status_line(fd,"ALDO1",(r11&0x01)?1:0,ldo_0p7_step100mv(r19&0x1F),r19);
    axp_print_status_line(fd,"ALDO2",(r11&0x02)?1:0,ldo_0p7_step100mv(r20&0x1F),r20);
    axp_print_status_line(fd,"ALDO3",(r11&0x04)?1:0,ldo_0p7_step100mv(r21&0x1F),r21);
    axp_print_status_line(fd,"ALDO4",(r11&0x08)?1:0,ldo_0p7_step100mv(r22&0x1F),r22);
    axp_print_status_line(fd,"ALDO5",(r11&0x10)?1:0,ldo_0p7_step100mv(r23&0x1F),r23);
    axp_print_status_line(fd,"BLDO1",(r11&0x20)?1:0,ldo_0p7_step100mv(r24&0x1F),r24);
    axp_print_status_line(fd,"BLDO2",(r11&0x40)?1:0,ldo_0p7_step100mv(r25&0x1F),r25);
    axp_print_status_line(fd,"BLDO3",(r11&0x80)?1:0,ldo_0p7_step100mv(r26&0x1F),r26);
    axp_print_status_line(fd,"BLDO4",(r12&0x01)?1:0,ldo_0p7_step100mv(r27&0x1F),r27);
    axp_print_status_line(fd,"BLDO5",(r12&0x02)?1:0,ldo_0p7_step100mv(r28&0x1F),r28);
    axp_print_status_line(fd,"CLDO1",(r12&0x04)?1:0,ldo_0p7_step100mv(r29&0x1F),r29);
    axp_print_status_line(fd,"CLDO2",(r12&0x08)?1:0,ldo_0p7_step100mv(r2a&0x1F),r2a);
    axp_print_status_line(fd,"CLDO3",(r12&0x10)?1:0,ldo_0p7_step100mv(r2b&0x1F),r2b);
    axp_print_status_line(fd,"CLDO4",(r12&0x20)?1:0,cldo4_mv(r2d&0x3F),r2d);
    axp_print_status_line(fd,"CPUSLDO",(r12&0x40)?1:0,cpusldo_mv(r2e&0x0F),r2e);
    n = snprintf(line,sizeof(line),"POK=0x%02X IRQ_EN=[40:0x%02X 41:0x%02X] IRQ_ST=[48:0x%02X 49:0x%02X]\r\n",r36,r40,r41,r48,r49);
    (void)lwip_write(fd,line,n);
}

static void cmd_axp_rules(int fd)
{
    char line[64];
    const char *pol = (i2cdev_get_policy() == I2CDEV_POLICY_WHITELIST) ? "WHITELIST" : "BLACKLIST";
    int n = snprintf(line, sizeof(line), "POLICY=%s\r\n", pol);
    (void)lwip_write(fd, line, n);
    print_bitmap_list(fd, "WHITELIST:", i2cdev_whitelist_bitmap);
    print_bitmap_list(fd, "BLACKLIST:", i2cdev_blacklist_bitmap);
}

static void cmd_help_axp(int fd)
{
    write_str(fd, "axp usage:\r\n");
    write_str(fd, "  axp status\r\n");
    write_str(fd, "  axp r <reg>\r\n");
    write_str(fd, "  axp w <reg> <val>\r\n");
    write_str(fd, "  axp rules\r\n");
    write_str(fd, "  axp policy <whitelist|blacklist>\r\n");
    write_str(fd, "  axp allow <reg> <val>\r\n");
    write_str(fd, "  axp deny <reg> <val>\r\n");
    write_str(fd, "  axp clear <reg> <val>\r\n");
    write_str(fd, "  axp reset\r\n");
    write_str(fd, "notes:\r\n");
    write_str(fd, "  policy/rules persist to flash:/configs/axp15060.json\r\n");
}

static void cmd_axp_policy(int fd, const char *mode)
{
    if (!mode) { write_str(fd, "ERR\r\n"); return; }
    axp15060_config_t cfg;
    if (!config_store_get_axp15060(&cfg)) {
        write_str(fd, "ERR (config not ready)\r\n");
        return;
    }
    if (strcasecmp(mode, "whitelist") == 0) {
        i2cdev_set_policy(I2CDEV_POLICY_WHITELIST);
        cfg.policy = AXP_POLICY_WHITELIST;
        persist_cfg(fd, &cfg);
        write_str(fd, "OK\r\n");
        return;
    }
    if (strcasecmp(mode, "blacklist") == 0) {
        i2cdev_set_policy(I2CDEV_POLICY_BLACKLIST);
        cfg.policy = AXP_POLICY_BLACKLIST;
        persist_cfg(fd, &cfg);
        write_str(fd, "OK\r\n");
        return;
    }
    write_str(fd, "ERR\r\n");
}

static void cmd_axp_rule_edit(int fd, const char *op, const char *s_reg, const char *s_val)
{
    if (!op || !s_reg || !s_val) { write_str(fd, "ERR\r\n"); return; }
    bool okr = false, okv = false;
    unsigned long reg = parse_num(s_reg, &okr);
    unsigned long val = parse_num(s_val, &okv);
    if (!okr || !okv || reg >= I2CDEV_REG_COUNT || val > I2CDEV_MAX_VALUE_CODE) { write_str(fd, "ERR\r\n"); return; }

    axp15060_config_t cfg;
    if (!config_store_get_axp15060(&cfg)) {
        write_str(fd, "ERR (config not ready)\r\n");
        return;
    }

    bool ok = false;
    if (strcasecmp(op, "allow") == 0) {
        ok = i2cdev_rule_allow((uint8_t)reg, (uint8_t)val);
        if (ok) cfg_rule_add(cfg.whitelist, &cfg.whitelist_len, AXP15060_RULES_MAX, (uint8_t)reg, (uint8_t)val);
    } else if (strcasecmp(op, "deny") == 0) {
        ok = i2cdev_rule_deny((uint8_t)reg, (uint8_t)val);
        if (ok) cfg_rule_add(cfg.blacklist, &cfg.blacklist_len, AXP15060_RULES_MAX, (uint8_t)reg, (uint8_t)val);
    } else if (strcasecmp(op, "clear") == 0) {
        ok = i2cdev_rule_clear((uint8_t)reg, (uint8_t)val);
        if (ok) {
            cfg_rule_remove(cfg.whitelist, &cfg.whitelist_len, (uint8_t)reg, (uint8_t)val);
            cfg_rule_remove(cfg.blacklist, &cfg.blacklist_len, (uint8_t)reg, (uint8_t)val);
        }
    }
    else { write_str(fd, "ERR\r\n"); return; }

    if (ok) persist_cfg(fd, &cfg);
    write_str(fd, ok ? "OK\r\n" : "ERR\r\n");
}

bool axp_handle(char *tok, char **save, int fd)
{
    if (!tok || strcasecmp(tok, "axp") != 0) return false;
    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub,"-h")==0 || strcasecmp(sub,"--help")==0) { cmd_help_axp(fd); return true; }
    if (strcasecmp(sub,"status")==0) { cmd_axp_status(fd); return true; }
    if (strcasecmp(sub,"r")==0) {
        char *s_reg = strtok_r(NULL, " \t", save);
        if (!s_reg) { write_str(fd, "ERR\r\n"); return true; }
        bool okr=false;
        unsigned long reg = parse_num(s_reg, &okr);
        if (!okr || reg>=I2CDEV_REG_COUNT) { write_str(fd, "ERR UNSUPPORTED\r\n"); return true; }
        uint8_t v=0;
        if (!i2cdev_read_reg((uint8_t)reg, &v)) { write_str(fd, "ERR UNSUPPORTED\r\n"); return true; }
        char out[64];
        int n = snprintf(out,sizeof(out),"OK REG 0x%02lX = 0x%02X %u\r\n",reg,v,v);
        (void)lwip_write(fd,out,n);
        return true;
    }
    if (strcasecmp(sub,"w")==0) {
        char *s_reg = strtok_r(NULL, " \t", save);
        char *s_val = strtok_r(NULL, " \t", save);
        if (!s_reg || !s_val) { write_str(fd, "ERR\r\n"); return true; }
        bool okr=false, okv=false;
        unsigned long reg = parse_num(s_reg, &okr);
        unsigned long val = parse_num(s_val, &okv);
        if (!okr || reg>=I2CDEV_REG_COUNT) { write_str(fd, "ERR UNSUPPORTED\r\n"); return true; }
        if (!okv || val>0xFF) { write_str(fd, "ERR\r\n"); return true; }
        if (!i2cdev_write_reg((uint8_t)reg,(uint8_t)val)) { write_str(fd, "ERR DENIED\r\n"); return true; }
        write_str(fd,"OK\r\n");
        return true;
    }
    if (strcasecmp(sub,"rules")==0) { cmd_axp_rules(fd); return true; }
    if (strcasecmp(sub,"policy")==0) { char *mode=strtok_r(NULL," \t",save); cmd_axp_policy(fd, mode); return true; }
    if (strcasecmp(sub,"allow")==0 || strcasecmp(sub,"deny")==0 || strcasecmp(sub,"clear")==0) {
        char *s_reg = strtok_r(NULL, " \t", save);
        char *s_val = strtok_r(NULL, " \t", save);
        cmd_axp_rule_edit(fd, sub, s_reg, s_val);
        return true;
    }
    if (strcasecmp(sub,"reset")==0) {
        (void)config_store_set_axp15060_defaults();
        axp15060_config_t cfg;
        if (!config_store_get_axp15060(&cfg)) { write_str(fd, "ERR (config not ready)\r\n"); return true; }
        i2cdev_policy_clear_all();
        i2cdev_set_policy((cfg.policy == AXP_POLICY_BLACKLIST) ? I2CDEV_POLICY_BLACKLIST : I2CDEV_POLICY_WHITELIST);
        for (size_t i = 0; i < cfg.whitelist_len; ++i) (void)i2cdev_rule_allow(cfg.whitelist[i].reg, cfg.whitelist[i].val);
        for (size_t i = 0; i < cfg.blacklist_len; ++i) (void)i2cdev_rule_deny(cfg.blacklist[i].reg, cfg.blacklist[i].val);
        i2cdev_autopoll_configure(cfg.autopoll_regs,
                                  cfg.autopoll_regs_len,
                                  cfg.autopoll_reg_delay_ms,
                                  cfg.autopoll_cycle_delay_ms,
                                  cfg.autopoll_enabled);
        persist_cfg(fd, &cfg);
        write_str(fd,"OK\r\n");
        return true;
    }
    write_str(fd, "ERR\r\n");
    return true;
}

void axp_help(int fd)
{
    write_str(fd, "  axp status\r\n");
    write_str(fd, "  axp r <reg>\r\n");
    write_str(fd, "  axp w <reg> <val>\r\n");
    write_str(fd, "  axp rules\r\n");
    write_str(fd, "  axp policy <whitelist|blacklist>\r\n");
    write_str(fd, "  axp allow <reg> <val>\r\n");
    write_str(fd, "  axp deny <reg> <val>\r\n");
    write_str(fd, "  axp clear <reg> <val>\r\n");
    write_str(fd, "  axp reset\r\n");
    write_str(fd, "  (policy/rules persist to flash:/configs/axp15060.json)\r\n");
}
