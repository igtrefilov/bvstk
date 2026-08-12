#include "services/i2c/bvstk_i2c_devices.h"

#include <string.h>
#include <strings.h>

static bvstk_status_t device_status(const bvstk_i2c_devices_t *devices,
                                    size_t device_id)
{
    if (devices == NULL || devices->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    return device_id < devices->count ? BVSTK_OK : BVSTK_ERR_NOT_FOUND;
}

static int item_valid(const bvstk_i2c_device_t *item)
{
    return item != NULL && item->name[0] != '\0' &&
           item->addr_7b <= UINT8_C(0x7F) && item->reg_count != 0U &&
           item->reg_count <= I2C_CFG_MAX_REG_COUNT;
}

bvstk_status_t bvstk_i2c_devices_init(bvstk_i2c_devices_t *devices,
                                      const bvstk_i2c_device_t *items,
                                      size_t count)
{
    size_t i;

    if (devices == NULL || count > I2C_CFG_MAX_DEVICES ||
        (count != 0U && items == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(devices, 0, sizeof(*devices));
    for (i = 0U; i < count; ++i) {
        if (!item_valid(&items[i])) {
            return BVSTK_ERR_MALFORMED;
        }
        devices->items[i] = items[i];
    }
    devices->count = count;
    devices->initialized = 1U;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_devices_init_from_config(
    bvstk_i2c_devices_t *devices,
    const i2c_device_config_t *configs,
    size_t count)
{
    bvstk_i2c_device_t items[I2C_CFG_MAX_DEVICES];
    size_t i;

    if (count > I2C_CFG_MAX_DEVICES || (count != 0U && configs == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(items, 0, sizeof(items));
    for (i = 0U; i < count; ++i) {
        strncpy(items[i].name, configs[i].name, sizeof(items[i].name) - 1U);
        items[i].addr_7b = configs[i].addr_7b;
        items[i].reg_count = configs[i].reg_count;
        items[i].max_value_code = configs[i].max_value_code;
    }
    return bvstk_i2c_devices_init(devices, items, count);
}

void bvstk_i2c_devices_shutdown(bvstk_i2c_devices_t *devices)
{
    if (devices != NULL) {
        memset(devices, 0, sizeof(*devices));
    }
}

size_t bvstk_i2c_devices_count(const bvstk_i2c_devices_t *devices)
{
    return devices != NULL && devices->initialized != 0U ? devices->count : 0U;
}

bvstk_status_t bvstk_i2c_devices_get(const bvstk_i2c_devices_t *devices,
                                     size_t device_id,
                                     bvstk_i2c_device_t *out)
{
    bvstk_status_t status = device_status(devices, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (out == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    *out = devices->items[device_id];
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_devices_set(bvstk_i2c_devices_t *devices,
                                     size_t device_id,
                                     const bvstk_i2c_device_t *item)
{
    bvstk_status_t status = device_status(devices, device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (!item_valid(item)) {
        return BVSTK_ERR_MALFORMED;
    }
    devices->items[device_id] = *item;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_devices_find_by_name(
    const bvstk_i2c_devices_t *devices,
    const char *name,
    size_t *device_id)
{
    size_t i;

    if (devices == NULL || name == NULL || name[0] == '\0' ||
        device_id == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (devices->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    for (i = 0U; i < devices->count; ++i) {
        if (strcasecmp(devices->items[i].name, name) == 0) {
            *device_id = i;
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_NOT_FOUND;
}

bvstk_status_t bvstk_i2c_devices_find_by_addr(
    const bvstk_i2c_devices_t *devices,
    uint8_t addr_7b,
    size_t *device_id)
{
    size_t i;

    if (devices == NULL || device_id == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (devices->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    addr_7b &= UINT8_C(0x7F);
    for (i = 0U; i < devices->count; ++i) {
        if (devices->items[i].addr_7b == addr_7b) {
            *device_id = i;
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_NOT_FOUND;
}
