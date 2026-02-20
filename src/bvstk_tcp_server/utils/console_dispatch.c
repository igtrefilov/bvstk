#include "utils.h"

#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>

#include "fs_shell.h"
#include "smi_shell.h"
#include "mem_shell.h"
#include "i2c_shell.h"
#include "spi_shell.h"
#include "tar_shell.h"
#include "ip_shell.h"

#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xscuwdt.h"

static volatile int s_close_requested = 0;

void utils_reset_close(void)
{
    s_close_requested = 0;
}

int utils_should_close(void)
{
    return s_close_requested;
}

static void cmd_help_top(int fd)
{
    write_str(fd, "available utilities (use <name> -h):\r\n");
    write_str(fd, "  fs\r\n");
    write_str(fd, "  tar\r\n");
    write_str(fd, "  ip\r\n");
    write_str(fd, "  smi\r\n");
    write_str(fd, "  spi\r\n");
    write_str(fd, "  mem\r\n");
    write_str(fd, "  i2c\r\n");
    write_str(fd, "built-ins:\r\n");
    write_str(fd, "  help|-h|--help|-help\r\n");
    write_str(fd, "  reboot\r\n");
    write_str(fd, "  quit|exit\r\n");
}

static void reboot_task(void *arg)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)arg;
    if (delay_ms) vTaskDelay(pdMS_TO_TICKS(delay_ms));

    XScuWdt wdt;
    XScuWdt_Config *cfg = NULL;
#ifdef XPAR_SCUWDT_0_DEVICE_ID
    cfg = XScuWdt_LookupConfig(XPAR_SCUWDT_0_DEVICE_ID);
#endif
#ifdef XPAR_PS7_SCUWDT_0_DEVICE_ID
    if (!cfg) cfg = XScuWdt_LookupConfig(XPAR_PS7_SCUWDT_0_DEVICE_ID);
#endif

    if (cfg) {
        (void)XScuWdt_CfgInitialize(&wdt, cfg, cfg->BaseAddr);
        XScuWdt_SetWdMode(&wdt);
        XScuWdt_LoadWdt(&wdt, 0x00000FFFu);
        XScuWdt_Start(&wdt);
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void cmd_reboot(int fd, char **save)
{
    char *arg1 = strtok_r(NULL, " \t", save);
    if (!arg1 || strcasecmp(arg1, "-h") == 0 || strcasecmp(arg1, "--help") == 0) {
        write_str(fd, "reboot usage:\r\n");
        write_str(fd, "  reboot -y [delay_ms]\r\n");
        write_str(fd, "  reboot confirm [delay_ms]\r\n");
        write_str(fd, "notes:\r\n");
        write_str(fd, "  delay_ms default: 1000, max: 5000\r\n");
        return;
    }

    int confirmed = 0;
    if (strcasecmp(arg1, "-y") == 0 || strcasecmp(arg1, "--yes") == 0 || strcasecmp(arg1, "confirm") == 0) {
        confirmed = 1;
    }
    if (!confirmed) {
        write_str(fd, "ERR: confirm required (use `reboot -y`)\r\n");
        return;
    }

    uint32_t delay_ms = 1000;
    char *arg2 = strtok_r(NULL, " \t", save);
    if (arg2) {
        char *end = NULL;
        unsigned long v = strtoul(arg2, &end, 10);
        if (!end || *end != '\0') { write_str(fd, "ERR\r\n"); return; }
        if (v > 5000UL) v = 5000UL;
        delay_ms = (uint32_t)v;
    }

    write_str(fd, "OK (rebooting)\r\n");
    (void)xTaskCreate(reboot_task, "reboot", 512, (void *)(uintptr_t)delay_ms, tskIDLE_PRIORITY + 3, NULL);
    s_close_requested = 1;
}

void process_console_line(const char *line, int socket_fd, console_session_t *session)
{
    char tmp[256];
    strncpy(tmp, line, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *save = NULL;
    char *tok = strtok_r(tmp, " \t", &save);
    if (!tok) return;

    if (fs_handle(tok, &save, socket_fd, session)) return;
    if (strcasecmp(tok, "help") == 0 || strcasecmp(tok, "-h") == 0 || strcasecmp(tok, "--help") == 0 || strcasecmp(tok, "-help") == 0) { cmd_help_top(socket_fd); return; }
    if (strcasecmp(tok, "reboot") == 0) { cmd_reboot(socket_fd, &save); return; }
    if (strcasecmp(tok, "quit") == 0 || strcasecmp(tok, "exit") == 0) { write_str(socket_fd, "Bye!\r\n"); s_close_requested = 1; return; }
    if (tar_handle(tok, &save, socket_fd, session)) return;
    if (ip_handle(tok, &save, socket_fd, session)) return;
    if (smi_handle(tok, &save, socket_fd)) return;
    if (spi_handle(tok, &save, socket_fd)) return;
    if (mem_handle(tok, &save, socket_fd)) return;
    if (i2c_handle(tok, &save, socket_fd)) return;
    write_str(socket_fd, "ERR\r\n");
}
