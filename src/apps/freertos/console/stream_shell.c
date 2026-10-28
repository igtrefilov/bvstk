#include "apps/freertos/console/stream_shell.h"
#include "apps/freertos/console/bvstk_cli_freertos.h"
#include <string.h>
void dcp2_stream_shell_help(uint8_t service, int fd)
{
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    bvstk_stream_cli_help(&cli, service);
}
bool dcp2_stream_shell_handle(uint8_t service, char **save, int fd)
{
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    char *argv[8], *token; int argc = 0;
    while ((token = strtok_r(NULL, " \t", save))) {
        if (argc == 8) { bvstk_cli_result(&cli, "stream", BVSTK_ERR_RANGE); return true; }
        argv[argc++] = token;
    }
    return bvstk_stream_cli(&cli, service, argc, argv);
}
