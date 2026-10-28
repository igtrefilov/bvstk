#include "apps/freertos/console/mem_shell.h"
#include "apps/freertos/console/bvstk_cli_freertos.h"
#include "services/cli/bvstk_storage_cli.h"
#include <string.h>
#include <strings.h>
bool mem_handle(char *token, char **save, int fd)
{
    if (!token || strcasecmp(token, "mem")) return false;
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    const bvstk_control_api_t control = {.memory = bvstk_platform_memory()};
    char *argv[8]; int argc = bvstk_cli_tokens(*save ? *save : (char *)"", argv, 8);
    if (argc < 0) bvstk_cli_result(&cli, "mem", BVSTK_ERR_RANGE);
    else (void)bvstk_memory_cli(&cli, &control, argc, argv);
    return true;
}
void mem_help(int fd)
{
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    (void)bvstk_memory_cli(&cli, NULL, 0, NULL);
}
