#include "apps/freertos/console/bvstk_system_freertos.h"
#include "services/cli/bvstk_cli.h"
#include "apps/freertos/console/utils.h"

#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>

#include "fs_shell.h"
#include "apps/freertos/console/bvstk_cli_freertos.h"
#include "services/cli/bvstk_storage_cli.h"
#include "shared/cli/bvstk_line_editor.h"
#include "mdio_shell.h"
#include "mem_shell.h"
#include "i2c_shell.h"
#include "spi_shell.h"
#include "uart_shell.h"
#include "sd_pl_shell.h"
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
    write_str(fd, "  system runtime|network\r\n");
    write_str(fd, "  mdio\r\n");
    write_str(fd, "  spi\r\n");
    write_str(fd, "  uart\r\n");
    write_str(fd, "  sd-pl\r\n");
    write_str(fd, "  mem\r\n");
    write_str(fd, "  pl\r\n");
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

bvstk_status_t bvstk_freertos_schedule_reboot(unsigned delay)
{
    if (delay > 5000U) return BVSTK_ERR_RANGE;
    return xTaskCreate(reboot_task, "reboot", 512, (void *)(uintptr_t)delay,
        tskIDLE_PRIORITY + 3, NULL) == pdPASS ? BVSTK_OK : BVSTK_ERR_INTERNAL;
}

static void cmd_reboot(int fd, char **save)
{
    char *argv[8]={(char *)"reboot"};int argc=1;char *token;
    while ((token=strtok_r(NULL," \t",save))) {
        if (argc==8) { write_str(fd,"ERR arguments\r\n");return; }
        argv[argc++]=token;
    }
    if (!bvstk_system_freertos_command(argc,argv,fd)) s_close_requested=1;
}

void process_console_line(const char *line, int socket_fd, console_session_t *session)
{
    char tmp[BVSTK_LINE_EDITOR_LINE_MAX];
    if (!line || strlen(line) >= sizeof(tmp)) {
        write_str(socket_fd, "ERR command range\r\n");
        return;
    }
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
    if (!strcasecmp(tok, "system") || !strcasecmp(tok, "ip")) {
        char system_line[512], *args[32];
        strncpy(system_line, line, sizeof(system_line)-1); system_line[sizeof(system_line)-1]=0;
        int argc=bvstk_cli_tokens(system_line,args,32);
        if (argc>0) (void)bvstk_system_freertos_command(argc,args,socket_fd);
        return;
    }
    if (mdio_handle(tok, &save, socket_fd)) return;
    if (spi_handle(tok, &save, socket_fd)) return;
    if (uart_handle(tok, &save, socket_fd)) return;
    if (sd_pl_handle(tok, &save, socket_fd)) return;
    if (!strcasecmp(tok, "pl")) {
        bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, socket_fd);
        bvstk_pl_service_t pl; bvstk_pl_service_init(&pl);
        bvstk_control_api_t control = {.pl = &pl};
        char *args[8]; int argc = bvstk_cli_tokens(save ? save : (char *)"", args, 8);
        if (argc < 0) bvstk_cli_result(&cli, "pl", BVSTK_ERR_RANGE);
        else (void)bvstk_pl_cli(&cli, &control, argc, args);
        bvstk_pl_service_shutdown(&pl);
        return;
    }
    if (mem_handle(tok, &save, socket_fd)) return;
    if (i2c_handle(tok, &save, socket_fd)) return;
    write_str(socket_fd, "ERR\r\n");
}
