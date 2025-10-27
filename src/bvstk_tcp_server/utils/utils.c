#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <inttypes.h>
#include "lwip/sockets.h"
#include "xil_io.h"
#include "FreeRTOS.h"
#include "../../bvstk_smi/bvstk_smi.h"
#include "../../bvstk_i2c/bvstk_i2c.h"

static volatile int s_close_requested = 0;

void write_str(int fd, const char *s)
{
    (void)lwip_write(fd, s, strlen(s));
}

unsigned long parse_num(const char *s, bool *ok)
{
    char *end = NULL;
    unsigned long base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) base = 16;
    unsigned long v = strtoul(s, &end, base);
    *ok = (end && *end == '\0');
    return v;
}

uint16_t swap_endianness_16(uint16_t value)
{
    return (uint16_t)((value >> 8) | (value << 8));
}

uint32_t swap_endianness_32(uint32_t value)
{
    return ((value >> 24) & 0x000000FFu) |
           ((value >> 8)  & 0x0000FF00u) |
           ((value << 8)  & 0x00FF0000u) |
           ((value << 24) & 0xFF000000u);
}

uint64_t swap_endianness_64(uint64_t value)
{
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >> 8)  & 0x00000000FF000000ULL) |
           ((value << 8)  & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
}

void utils_reset_close(void)
{
    s_close_requested = 0;
}

int utils_should_close(void)
{
    return s_close_requested;
}

static void pong(uint8_t *data_buffer, int data_length, int socket_fd)
{
    int send_result = lwip_write(socket_fd, data_buffer, data_length);
    if (send_result < 0) return;
}

static void reg_write(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd)
{
    uint16_t total_bytes = (uint16_t)((data_buffer[1] << 8) | data_buffer[2]);
    int total_words = (total_bytes > 5) ? (int)((total_bytes - 5) / 4) : 0;
    uint32_t address = ((uint32_t)data_buffer[4] << 24) |
                       ((uint32_t)data_buffer[5] << 16) |
                       ((uint32_t)data_buffer[6] << 8)  |
                       ((uint32_t)data_buffer[7] << 0);
    for (int i = 0; i < total_words; i++) {
        if (8 + i * 4 + 3 >= data_length) return;
        uint32_t word_to_write = ((uint32_t)data_buffer[8 + i * 4] << 24) |
                                 ((uint32_t)data_buffer[8 + i * 4 + 1] << 16) |
                                 ((uint32_t)data_buffer[8 + i * 4 + 2] << 8)  |
                                 ((uint32_t)data_buffer[8 + i * 4 + 3] << 0);
        Xil_Out32(address, word_to_write);
        if (auto_increment) address += 4;
    }
    (void)socket_fd;
}

static void reg_read(uint8_t *data_buffer, int data_length, uint8_t auto_increment, int socket_fd)
{
    uint16_t total_bytes = (uint16_t)((data_buffer[1] << 8) | data_buffer[2]);
    uint16_t words_to_read = (uint16_t)((data_buffer[8] << 8) | data_buffer[9]);
    uint32_t address = ((uint32_t)data_buffer[4] << 24) |
                       ((uint32_t)data_buffer[5] << 16) |
                       ((uint32_t)data_buffer[6] << 8)  |
                       ((uint32_t)data_buffer[7] << 0);
    for (uint16_t i = 0; i < words_to_read; i++) {
        uint32_t word_read = Xil_In32(address);
        data_buffer[10 + i * 4]     = (uint8_t)((word_read >> 24) & 0xFF);
        data_buffer[10 + i * 4 + 1] = (uint8_t)((word_read >> 16) & 0xFF);
        data_buffer[10 + i * 4 + 2] = (uint8_t)((word_read >> 8)  & 0xFF);
        data_buffer[10 + i * 4 + 3] = (uint8_t)(word_read & 0xFF);
        if (auto_increment) address += 4;
    }
    total_bytes = (uint16_t)(words_to_read * 4 + 2 + 4 + 1);
    data_buffer[2] = (uint8_t)(total_bytes & 0xFF);
    data_buffer[1] = (uint8_t)(total_bytes >> 8);
    data_length = (int)total_bytes + 3;
    (void)lwip_write(socket_fd, data_buffer, data_length);
}

static void select_reg_write_read(uint8_t *data_buffer, int data_length, int socket_fd)
{
    if (data_length < 8) return;
    uint8_t wr_rd_flag = (uint8_t)(data_buffer[3] & 0x80);
    uint8_t auto_increment = (uint8_t)(data_buffer[3] & 0x40);
    if (wr_rd_flag) reg_write(data_buffer, data_length, auto_increment, socket_fd);
    else reg_read(data_buffer, data_length, auto_increment, socket_fd);
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

static void cmd_axp_rules(int fd)
{
    char line[64];
    const char *pol = (I2CDEV_DEFAULT_POLICY == I2CDEV_POLICY_WHITELIST) ? "WHITELIST" : "BLACKLIST";
    int n = snprintf(line, sizeof(line), "POLICY=%s\r\n", pol);
    (void)lwip_write(fd, line, n);
    print_bitmap_list(fd, "WHITELIST:", i2cdev_whitelist_bitmap);
    print_bitmap_list(fd, "BLACKLIST:", i2cdev_blacklist_bitmap);
}

static void cmd_help_top(int fd)
{
    write_str(fd, "Commands:\r\n");
    write_str(fd, "  smi -h|--help\r\n");
    write_str(fd, "  smi r <phy> <reg>\r\n");
    write_str(fd, "  smi w <phy> <reg> <data>\r\n");
    write_str(fd, "  mem -h|--help\r\n");
    write_str(fd, "  mem r <addr>\r\n");
    write_str(fd, "  mem w <addr> <value>\r\n");
    write_str(fd, "  axp status\r\n");
    write_str(fd, "  axp r <reg>\r\n");
    write_str(fd, "  axp w <reg> <val>\r\n");
    write_str(fd, "  axp rules\r\n");
    write_str(fd, "  help\r\n");
    write_str(fd, "  quit|exit\r\n");
}

static void cmd_help_smi(int fd)
{
    write_str(fd, "smi usage:\r\n");
    write_str(fd, "  smi r <phy> <reg>\r\n");
    write_str(fd, "  smi w <phy> <reg> <data>\r\n");
}

static void cmd_help_mem(int fd)
{
    write_str(fd, "mem usage:\r\n");
    write_str(fd, "  mem r <addr>\r\n");
    write_str(fd, "    reads 32-bit if <addr> aligned to 4, otherwise 8-bit\r\n");
    write_str(fd, "  mem w <addr> <value>\r\n");
    write_str(fd, "    writes 32-bit if aligned; if unaligned, allows 8-bit when value<=0xFF\r\n");
}

static void cmd_help_axp(int fd)
{
    write_str(fd, "axp usage:\r\n");
    write_str(fd, "  axp status\r\n");
    write_str(fd, "  axp r <reg>\r\n");
    write_str(fd, "  axp w <reg> <val>\r\n");
    write_str(fd, "  axp rules\r\n");
}

void process_console_line(const char *line, int socket_fd)
{
    char tmp[256];
    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *save = NULL;
    char *tok = strtok_r(tmp, " \t", &save);
    if (!tok) return;
    if (strcasecmp(tok, "help") == 0 || strcasecmp(tok, "-h") == 0 || strcasecmp(tok, "--help") == 0) { cmd_help_top(socket_fd); return; }
    if (strcasecmp(tok, "quit") == 0 || strcasecmp(tok, "exit") == 0) { write_str(socket_fd, "Bye!\r\n"); s_close_requested = 1; return; }
    if (strcasecmp(tok, "smi") == 0) {
        char *sub = strtok_r(NULL, " \t", &save);
        if (!sub || strcasecmp(sub, "-h")==0 || strcasecmp(sub, "--help")==0) { cmd_help_smi(socket_fd); return; }
        if (strcasecmp(sub, "r") == 0) {
            char *s_phy = strtok_r(NULL, " \t", &save);
            char *s_reg = strtok_r(NULL, " \t", &save);
            if (!s_phy || !s_reg) { write_str(socket_fd, "ERR\r\n"); return; }
            bool ok1=false, ok2=false;
            unsigned long phy = parse_num(s_phy, &ok1);
            unsigned long reg = parse_num(s_reg, &ok2);
            if (!ok1 || !ok2 || phy>31 || reg>31) { write_str(socket_fd, "ERR\r\n"); return; }
            uint16_t val = 0;
            bool ok = mdio_read_blocking((uint8_t)phy, (uint8_t)reg, &val, pdMS_TO_TICKS(100)) ? true : false;
            if (!ok) { write_str(socket_fd, "ERR\r\n"); return; }
            char out[64];
            int n = snprintf(out, sizeof(out), "OK 0x%04" PRIX16 " %" PRIu16 "\r\n", val, val);
            (void)lwip_write(socket_fd, out, n);
            return;
        }
        if (strcasecmp(sub, "w") == 0) {
            char *s_phy = strtok_r(NULL, " \t", &save);
            char *s_reg = strtok_r(NULL, " \t", &save);
            char *s_dat = strtok_r(NULL, " \t", &save);
            if (!s_phy || !s_reg || !s_dat) { write_str(socket_fd, "ERR\r\n"); return; }
            bool ok1=false, ok2=false, ok3=false;
            unsigned long phy = parse_num(s_phy, &ok1);
            unsigned long reg = parse_num(s_reg, &ok2);
            unsigned long dat = parse_num(s_dat, &ok3);
            if (!ok1 || !ok2 || !ok3 || phy>31 || reg>31 || dat>0xFFFF) { write_str(socket_fd, "ERR\r\n"); return; }
            mdio_write((uint8_t)phy, (uint8_t)reg, (uint16_t)dat);
            write_str(socket_fd, "OK\r\n");
            return;
        }
        write_str(socket_fd, "ERR\r\n");
        return;
    }
    if (strcasecmp(tok, "mem") == 0) {
        char *sub = strtok_r(NULL, " \t", &save);
        if (!sub || strcasecmp(sub, "-h")==0 || strcasecmp(sub, "--help")==0) { cmd_help_mem(socket_fd); return; }
        if (strcasecmp(sub, "r") == 0) {
            char *s_addr = strtok_r(NULL, " \t", &save);
            if (!s_addr) { write_str(socket_fd, "ERR\r\n"); return; }
            bool ok=false;
            unsigned long addr_ul = parse_num(s_addr, &ok);
            if (!ok) { write_str(socket_fd, "ERR\r\n"); return; }
            uintptr_t addr = (uintptr_t)addr_ul;
            if ((addr & 3U) == 0U) {
                uint32_t v = Xil_In32((UINTPTR)addr);
                char out[64];
                int n = snprintf(out, sizeof(out), "OK 0x%08" PRIX32 " %" PRIu32 "\r\n", v, v);
                (void)lwip_write(socket_fd, out, n);
            } else {
                uint8_t v = Xil_In8((UINTPTR)addr);
                char out[64];
                int n = snprintf(out, sizeof(out), "OK 0x%02" PRIX8 " %" PRIu8 "\r\n", v, v);
                (void)lwip_write(socket_fd, out, n);
            }
            return;
        }
        if (strcasecmp(sub, "w") == 0) {
            char *s_addr = strtok_r(NULL, " \t", &save);
            char *s_val  = strtok_r(NULL, " \t", &save);
            if (!s_addr || !s_val) { write_str(socket_fd, "ERR\r\n"); return; }
            bool ok1=false, ok2=false;
            unsigned long addr_ul = parse_num(s_addr, &ok1);
            unsigned long val_ul  = parse_num(s_val,  &ok2);
            if (!ok1 || !ok2) { write_str(socket_fd, "ERR\r\n"); return; }
            uintptr_t addr = (uintptr_t)addr_ul;
            if ((addr & 3U) == 0U) {
                uint32_t v = (uint32_t)val_ul;
                Xil_Out32((UINTPTR)addr, v);
                write_str(socket_fd, "OK\r\n");
            } else {
                if (val_ul <= 0xFFUL) {
                    uint8_t v = (uint8_t)val_ul;
                    Xil_Out8((UINTPTR)addr, v);
                    write_str(socket_fd, "OK\r\n");
                } else {
                    write_str(socket_fd, "ERR\r\n");
                }
            }
            return;
        }
        write_str(socket_fd, "ERR\r\n");
        return;
    }
    if (strcasecmp(tok, "axp") == 0) {
        char *sub = strtok_r(NULL, " \t", &save);
        if (!sub || strcasecmp(sub,"-h")==0 || strcasecmp(sub,"--help")==0) { cmd_help_axp(socket_fd); return; }
        if (strcasecmp(sub,"status")==0) { cmd_axp_status(socket_fd); return; }
        if (strcasecmp(sub,"r")==0) {
            char *s_reg = strtok_r(NULL, " \t", &save);
            if (!s_reg) { write_str(socket_fd, "ERR\r\n"); return; }
            bool okr=false;
            unsigned long reg = parse_num(s_reg, &okr);
            if (!okr || reg>=I2CDEV_REG_COUNT) { write_str(socket_fd, "ERR UNSUPPORTED\r\n"); return; }
            uint8_t v=0;
            if (!i2cdev_read_reg((uint8_t)reg, &v)) { write_str(socket_fd, "ERR UNSUPPORTED\r\n"); return; }
            char out[64];
            int n = snprintf(out,sizeof(out),"OK REG 0x%02lX = 0x%02X %u\r\n",reg,v,v);
            (void)lwip_write(socket_fd,out,n);
            return;
        }
        if (strcasecmp(sub,"w")==0) {
            char *s_reg = strtok_r(NULL, " \t", &save);
            char *s_val = strtok_r(NULL, " \t", &save);
            if (!s_reg || !s_val) { write_str(socket_fd, "ERR\r\n"); return; }
            bool okr=false, okv=false;
            unsigned long reg = parse_num(s_reg, &okr);
            unsigned long val = parse_num(s_val, &okv);
            if (!okr || reg>=I2CDEV_REG_COUNT) { write_str(socket_fd, "ERR UNSUPPORTED\r\n"); return; }
            if (!okv || val>0xFF) { write_str(socket_fd, "ERR\r\n"); return; }
            if (!i2cdev_write_reg((uint8_t)reg,(uint8_t)val)) { write_str(socket_fd, "ERR DENIED\r\n"); return; }
            write_str(socket_fd,"OK\r\n");
            return;
        }
        if (strcasecmp(sub,"rules")==0) { cmd_axp_rules(socket_fd); return; }
        write_str(socket_fd, "ERR\r\n");
        return;
    }
    write_str(socket_fd, "ERR\r\n");
}
