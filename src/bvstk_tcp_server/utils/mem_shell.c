#include "mem_shell.h"

#include <stdio.h>
#include <strings.h>
#include <inttypes.h>
#include <stdint.h>

#include "xil_io.h"
#include "lwip/sockets.h"

static void cmd_help_mem(int fd)
{
    write_str(fd, "mem usage:\r\n");
    write_str(fd, "  mem r <addr>\r\n");
    write_str(fd, "    reads 32-bit if <addr> aligned to 4, otherwise 8-bit\r\n");
    write_str(fd, "  mem w <addr> <value>\r\n");
    write_str(fd, "    writes 32-bit if aligned; if unaligned, allows 8-bit when value<=0xFF\r\n");
}

bool mem_handle(char *tok, char **save, int fd)
{
    if (!tok || strcasecmp(tok, "mem") != 0) return false;
    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub, "-h")==0 || strcasecmp(sub, "--help")==0) { cmd_help_mem(fd); return true; }
    if (strcasecmp(sub, "r") == 0) {
        char *s_addr = strtok_r(NULL, " \t", save);
        if (!s_addr) { write_str(fd, "ERR\r\n"); return true; }
        bool ok=false;
        unsigned long addr_ul = parse_num(s_addr, &ok);
        if (!ok) { write_str(fd, "ERR\r\n"); return true; }
        uintptr_t addr = (uintptr_t)addr_ul;
        if ((addr & 3U) == 0U) {
            uint32_t v = Xil_In32((UINTPTR)addr);
            char out[64];
            int n = snprintf(out, sizeof(out), "OK 0x%08" PRIX32 " %" PRIu32 "\r\n", v, v);
            (void)lwip_write(fd, out, n);
        } else {
            uint8_t v = Xil_In8((UINTPTR)addr);
            char out[64];
            int n = snprintf(out, sizeof(out), "OK 0x%02" PRIX8 " %" PRIu8 "\r\n", v, v);
            (void)lwip_write(fd, out, n);
        }
        return true;
    }
    if (strcasecmp(sub, "w") == 0) {
        char *s_addr = strtok_r(NULL, " \t", save);
        char *s_val  = strtok_r(NULL, " \t", save);
        if (!s_addr || !s_val) { write_str(fd, "ERR\r\n"); return true; }
        bool ok1=false, ok2=false;
        unsigned long addr_ul = parse_num(s_addr, &ok1);
        unsigned long val_ul  = parse_num(s_val,  &ok2);
        if (!ok1 || !ok2) { write_str(fd, "ERR\r\n"); return true; }
        uintptr_t addr = (uintptr_t)addr_ul;
        if ((addr & 3U) == 0U) {
            uint32_t v = (uint32_t)val_ul;
            Xil_Out32((UINTPTR)addr, v);
            write_str(fd, "OK\r\n");
        } else {
            if (val_ul <= 0xFFUL) {
                uint8_t v = (uint8_t)val_ul;
                Xil_Out8((UINTPTR)addr, v);
                write_str(fd, "OK\r\n");
            } else {
                write_str(fd, "ERR\r\n");
            }
        }
        return true;
    }
    write_str(fd, "ERR\r\n");
    return true;
}

void mem_help(int fd)
{
    write_str(fd, "  mem -h|--help\r\n");
    write_str(fd, "  mem r <addr>\r\n");
    write_str(fd, "  mem w <addr> <value>\r\n");
}
