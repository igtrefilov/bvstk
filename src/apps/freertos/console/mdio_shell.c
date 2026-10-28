#include "apps/freertos/console/mdio_shell.h"
#include "apps/freertos/console/bvstk_cli_freertos.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include <string.h>
#include <strings.h>

int mdio_complete_line(const char *line, size_t length, size_t cursor,
                      bvstk_mdio_completion_result_t *r)
{
    bvstk_mdio_completion_device_t devices[MDIO_CFG_MAX_DEVICES];
    bvstk_mdio_service_t *s = bvstk_runtime_mdio_service();
    size_t n = bvstk_mdio_service_device_count(s), i;
    for (i = 0; i < n; ++i) {
        bvstk_mdio_device_info_t info;
        if (bvstk_mdio_service_device_info(s, i, &info) != BVSTK_OK) break;
        memcpy(devices[i].name, info.name, sizeof(devices[i].name));
        devices[i].phy_addr = info.phy_addr;
    }
    return bvstk_mdio_complete_line(line, length, cursor, devices, i, r);
}

bool mdio_handle(char *token, char **save, int fd)
{
    if (!token || strcasecmp(token, "mdio")) return false;
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    char *argv[40]; int argc = 0;
    while ((token = strtok_r(NULL, " \t", save))) {
        if (argc == 40) { bvstk_cli_result(&cli, "mdio", BVSTK_ERR_RANGE); return true; }
        argv[argc++] = token;
    }
    return bvstk_mdio_cli(&cli, argc, argv);
}
void mdio_help(int fd)
{
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    bvstk_mdio_cli_help(&cli);
}
