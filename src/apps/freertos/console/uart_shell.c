#include "apps/freertos/console/uart_shell.h"

#include <string.h>
#include <strings.h>

#include "apps/freertos/console/stream_shell.h"
#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"

bool uart_handle(char *tok, char **save, int fd)
{
    char *sub;

    if (tok == NULL || strcasecmp(tok, "uart") != 0) {
        return false;
    }

    sub = strtok_r(NULL, " \t", save);
    if (sub == NULL || strcasecmp(sub, "-h") == 0 ||
        strcasecmp(sub, "--help") == 0) {
        uart_help(fd);
        return true;
    }
    if (strcasecmp(sub, "stream") == 0) {
        (void)dcp2_stream_shell_handle(DCP2_STREAM_SERVICE_UART, save, fd);
        return true;
    }

    write_str(fd, "ERR (UART supports stream commands only)\r\n");
    return true;
}

void uart_help(int fd)
{
    write_str(fd, "uart usage:\r\n");
    write_str(fd, "  uart stream fake enable [period_ms] [counter|ramp|toggle] [words]\r\n");
    write_str(fd, "  uart stream fake disable|status\r\n");
}
