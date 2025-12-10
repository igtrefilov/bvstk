#include "utils.h"

#include <string.h>
#include <strings.h>

#include "fs_shell.h"
#include "smi_shell.h"
#include "mem_shell.h"
#include "axp_shell.h"

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
    write_str(fd, "  smi\r\n");
    write_str(fd, "  mem\r\n");
    write_str(fd, "  axp\r\n");
    write_str(fd, "built-ins:\r\n");
    write_str(fd, "  help|-h|--help|-help\r\n");
    write_str(fd, "  quit|exit\r\n");
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
    if (strcasecmp(tok, "quit") == 0 || strcasecmp(tok, "exit") == 0) { write_str(socket_fd, "Bye!\r\n"); s_close_requested = 1; return; }
    if (smi_handle(tok, &save, socket_fd)) return;
    if (mem_handle(tok, &save, socket_fd)) return;
    if (axp_handle(tok, &save, socket_fd)) return;
    write_str(socket_fd, "ERR\r\n");
}
