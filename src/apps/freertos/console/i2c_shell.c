#include "apps/freertos/console/i2c_shell.h"
#include "apps/freertos/console/bvstk_cli_freertos.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include <string.h>
#include <strings.h>

int i2c_complete_line(const char *line, size_t length, size_t cursor,
                      bvstk_i2c_completion_result_t *result)
{
    bvstk_i2c_completion_device_t entries[I2C_CFG_MAX_DEVICES];
    bvstk_i2c_master_service_t *service = bvstk_runtime_i2c_master_service();
    size_t count = bvstk_i2c_master_service_device_count(service), i;

    if (count > I2C_CFG_MAX_DEVICES) count = I2C_CFG_MAX_DEVICES;
    for (i = 0U; i < count; ++i) {
        bvstk_i2c_device_t device;
        if (bvstk_i2c_master_service_device_info(service, i, &device) != BVSTK_OK) break;
        memcpy(entries[i].name, device.name, sizeof(entries[i].name));
        entries[i].addr_7b = device.addr_7b;
    }
    return bvstk_i2c_complete_line_ex(line, length, cursor, entries, i,
                                      BVSTK_I2C_COMPLETION_EXPLICIT_SAVE, result);
}


bool i2c_handle(char *token, char **save, int fd)
{
    if (!token || strcasecmp(token, "i2c")) return false;
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    char *argv[40]; int argc = 0;
    while ((token = strtok_r(NULL, " \t", save))) {
        if (argc == 40) { bvstk_cli_result(&cli, "i2c", BVSTK_ERR_RANGE); return true; }
        argv[argc++] = token;
    }
    return bvstk_i2c_cli(&cli, argc, argv);
}
void i2c_help(int fd)
{
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    bvstk_i2c_cli_help(&cli);
}
