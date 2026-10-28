#include "apps/freertos/console/bvstk_cli_freertos.h"
#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"
#include <stdint.h>

static int output(void *context, const void *data, size_t size)
{ return console_stream_write((int)(intptr_t)context, data, size); }
static const i2c_device_config_t *i2c_config(void *context, size_t id)
{
    bvstk_cli_t *cli = context;
    bvstk_i2c_device_t device;
    return bvstk_i2c_master_service_device_info(cli->i2c, id, &device) == BVSTK_OK ?
        config_store_find_i2c_device_by_name(device.name) : NULL;
}
static bvstk_status_t i2c_apply(void *context, size_t id, const i2c_device_config_t *config)
{
    (void)context; (void)id;
    return bvstk_runtime_i2c_apply_config(config) && config_store_set_i2c_device(config) ?
        BVSTK_OK : BVSTK_ERR_INTERNAL;
}
static bvstk_status_t i2c_sync(void *context, size_t id)
{ (void)context; return bvstk_runtime_i2c_sync_device(id) ? BVSTK_OK : BVSTK_ERR_INTERNAL; }
static bvstk_status_t save(void *context, bool mdio, size_t id)
{
    bvstk_cli_t *cli = context;
    if (mdio) {
        mdio_phy_config_t config;
        bvstk_status_t st = bvstk_mdio_service_get_config(cli->mdio, id, &config);
        return st != BVSTK_OK ? st : config_store_save_mdio_device(&config) ? BVSTK_OK : BVSTK_ERR_IO;
    }
    if (i2c_sync(context, id) != BVSTK_OK) return BVSTK_ERR_INTERNAL;
    const i2c_device_config_t *config = i2c_config(context, id);
    if (!config) return BVSTK_ERR_NOT_FOUND;
    i2c_device_config_t snapshot = *config;
    return config_store_save_i2c_device(&snapshot) ? BVSTK_OK : BVSTK_ERR_IO;
}
void bvstk_cli_freertos_init(bvstk_cli_t *cli, int fd)
{
    *cli = (bvstk_cli_t){0};
    cli->context = cli;
    cli->output = (bvstk_output_t){output, (void *)(intptr_t)fd};
    cli->i2c = bvstk_runtime_i2c_master_service();
    cli->mdio = bvstk_runtime_mdio_service();
    cli->i2c_config = i2c_config; cli->i2c_apply = i2c_apply;
    cli->i2c_sync = i2c_sync; cli->save = save;
    cli->streams = dcp2_stream_sim_instance();
}
