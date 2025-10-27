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

void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd)
{
    if (data_length < 1) return;
    switch (data_buffer[0]) {
        case 0x00: pong(data_buffer, data_length, socket_fd); break;
        case 0x01: select_reg_write_read(data_buffer, data_length, socket_fd); break;
        case 0x02: break;
        case 0xFF: break;
        default: break;
    }
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
    write_str(socket_fd, "ERR\r\n");
}
