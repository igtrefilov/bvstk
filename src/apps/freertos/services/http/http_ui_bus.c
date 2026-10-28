#include "apps/freertos/services/http/http_ui.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/runtime/bvstk_observer.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/storage/fs/fs_dcp2_backend.h"
#include "task.h"
#include <string.h>
typedef http_bus_config_t bus_config_t;
void http_freertos_volumes(int);
void http_freertos_sd_info(int);
static bvstk_status_t snapshot(int mdio, size_t id, bus_config_t *out)
{
    if (mdio) return bvstk_mdio_service_get_config(bvstk_runtime_mdio_service(), id, &out->mdio);
    bvstk_status_t st;
    taskENTER_CRITICAL();
    st = bvstk_i2c_master_service_get_config(bvstk_runtime_i2c_master_service(), id, &out->i2c);
    if (st == BVSTK_OK) {
        const i2c_device_config_t *stored = config_store_find_i2c_device_by_name(out->i2c.name);
        if (!stored) st = BVSTK_ERR_NOT_READY;
        else {
            memcpy(out->i2c.file_name, stored->file_name, sizeof(out->i2c.file_name));
            out->i2c.settings_len = stored->settings_len;
            memcpy(out->i2c.settings, stored->settings, sizeof(out->i2c.settings));
            out->i2c.profile = stored->profile;
        }
    }
    taskEXIT_CRITICAL();
    return st;
}

static bvstk_status_t apply(int mdio,size_t id,const http_bus_config_t *c,const char *revision)
{
    (void)revision;
    if (mdio) return bvstk_mdio_service_set_config(bvstk_runtime_mdio_service(),id,&c->mdio);
    return bvstk_runtime_i2c_apply_config(&c->i2c)&&config_store_set_i2c_device(&c->i2c)?BVSTK_OK:BVSTK_ERR_IO;
}
static int save(int mdio,size_t id,const http_bus_config_t *c)
{ (void)id;return mdio?config_store_save_mdio_device(&c->mdio):config_store_save_i2c_device(&c->i2c); }
static void sync_device(size_t id) { (void)bvstk_runtime_i2c_sync_device(id); }
static void observe(unsigned bus,unsigned channel,int address,int device,uint64_t after,bvstk_journal_batch_t *out)
{ if(device>=0) bvstk_observer_read_device(bus,channel,device,after,out);else bvstk_observer_read(bus,channel,address,after,out); }
const http_ui_backend_t *http_ui_backend(void)
{
    static const http_ui_backend_t b={bvstk_runtime_i2c_master_service,bvstk_runtime_mdio_service,snapshot,apply,save,sync_device,fs_storage_backend,http_freertos_volumes,http_freertos_sd_info,pvPortMalloc,vPortFree,observe,true};return &b;
}
