#include "smi_shell.h"

#include <stdio.h>
#include <strings.h>
#include <inttypes.h>
#include <stdint.h>

#include "lwip/sockets.h"
#include "FreeRTOS.h"

#include "../../bvstk_smi/bvstk_smi.h"

static void cmd_help_smi(int fd)
{
    write_str(fd, "smi usage:\r\n");
    write_str(fd, "  smi r <phy> <reg>\r\n");
    write_str(fd, "  smi w <phy> <reg> <data>\r\n");
}

bool smi_handle(char *tok, char **save, int fd)
{
    if (!tok || strcasecmp(tok, "smi") != 0) return false;
    char *sub = strtok_r(NULL, " \t", save);
    if (!sub || strcasecmp(sub, "-h")==0 || strcasecmp(sub, "--help")==0) { cmd_help_smi(fd); return true; }
    if (strcasecmp(sub, "r") == 0) {
        char *s_phy = strtok_r(NULL, " \t", save);
        char *s_reg = strtok_r(NULL, " \t", save);
        if (!s_phy || !s_reg) { write_str(fd, "ERR\r\n"); return true; }
        bool ok1=false, ok2=false;
        unsigned long phy = parse_num(s_phy, &ok1);
        unsigned long reg = parse_num(s_reg, &ok2);
        if (!ok1 || !ok2 || phy>31 || reg>31) { write_str(fd, "ERR\r\n"); return true; }
        uint16_t val = 0;
        bool ok = mdio_read_blocking((uint8_t)phy, (uint8_t)reg, &val, pdMS_TO_TICKS(100)) ? true : false;
        if (!ok) { write_str(fd, "ERR\r\n"); return true; }
        char out[64];
        int n = snprintf(out, sizeof(out), "OK 0x%04" PRIX16 " %" PRIu16 "\r\n", val, val);
        (void)lwip_write(fd, out, n);
        return true;
    }
    if (strcasecmp(sub, "w") == 0) {
        char *s_phy = strtok_r(NULL, " \t", save);
        char *s_reg = strtok_r(NULL, " \t", save);
        char *s_dat = strtok_r(NULL, " \t", save);
        if (!s_phy || !s_reg || !s_dat) { write_str(fd, "ERR\r\n"); return true; }
        bool ok1=false, ok2=false, ok3=false;
        unsigned long phy = parse_num(s_phy, &ok1);
        unsigned long reg = parse_num(s_reg, &ok2);
        unsigned long dat = parse_num(s_dat, &ok3);
        if (!ok1 || !ok2 || !ok3 || phy>31 || reg>31 || dat>0xFFFF) { write_str(fd, "ERR\r\n"); return true; }
        mdio_write((uint8_t)phy, (uint8_t)reg, (uint16_t)dat);
        write_str(fd, "OK\r\n");
        return true;
    }
    write_str(fd, "ERR\r\n");
    return true;
}

void smi_help(int fd)
{
    write_str(fd, "  smi -h|--help\r\n");
    write_str(fd, "  smi r <phy> <reg>\r\n");
    write_str(fd, "  smi w <phy> <reg> <data>\r\n");
}
