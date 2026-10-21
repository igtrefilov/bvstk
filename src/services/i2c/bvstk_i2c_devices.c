#include "services/i2c/bvstk_i2c_devices.h"

#include <string.h>
#include <strings.h>
#include "shared/config/bvstk_i2c_config_codec.h"

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
           memchr(item->name, 0, sizeof(item->name)) &&
           item->addr_7b <= UINT8_C(0x7F) && item->reg_count != 0U &&
           item->reg_count <= I2C_CFG_MAX_REG_COUNT &&
           item->presence_check_enabled <= 1U &&
           (!item->presence_check_enabled || item->presence_check_reg < item->reg_count) &&
           (!item->endpoint.space_count || item->addr_7b == item->endpoint.spaces[0].address) &&
           bvstk_i2c_endpoint_validate(&item->endpoint, item->reg_count) == BVSTK_OK;
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
    size_t i;

    if (!devices || count > I2C_CFG_MAX_DEVICES || (count != 0U && configs == NULL)) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(devices, 0, sizeof(*devices));
    for (i = 0U; i < count; ++i) {
        bvstk_i2c_device_t *item = &devices->items[i];
        strncpy(item->name, configs[i].name, sizeof(item->name) - 1U);
        item->addr_7b = configs[i].addr_7b;
        item->reg_count = configs[i].reg_count;
        item->max_value_code = configs[i].max_value_code;
        item->presence_check_enabled = configs[i].presence_check_enabled;
        item->presence_check_reg = configs[i].presence_check_reg;
        item->endpoint = configs[i].endpoint;
        if (!item_valid(item)) return BVSTK_ERR_MALFORMED;
    }
    devices->count = count;
    devices->initialized = 1U;
    return BVSTK_OK;
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
    size_t i, found = SIZE_MAX;

    if (devices == NULL || device_id == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (devices->initialized == 0U) {
        return BVSTK_ERR_NOT_READY;
    }
    addr_7b &= UINT8_C(0x7F);
    for (i = 0U; i < devices->count; ++i) {
        if (bvstk_i2c_address_space(&devices->items[i].endpoint, devices->items[i].addr_7b, addr_7b) >= 0) {
            if (found != SIZE_MAX) return BVSTK_ERR_UNSUPPORTED;
            found = i;
        }
    }
    if (found == SIZE_MAX) return BVSTK_ERR_NOT_FOUND;
    *device_id = found;
    return BVSTK_OK;
}

bvstk_status_t bvstk_i2c_devices_route(const bvstk_i2c_devices_t *devices,
    size_t device_id, size_t switches[BVSTK_I2C_ROUTE_MAX],
    uint8_t channels[BVSTK_I2C_ROUTE_MAX], size_t *count)
{
    size_t current = device_id, n = 0, parent, i;
    if (!count || !switches || !channels || device_status(devices, device_id) != BVSTK_OK)
        return BVSTK_ERR_MALFORMED;
    *count = 0;
    while (devices->items[current].endpoint.parent[0]) {
        const bvstk_i2c_device_t *d = &devices->items[current];
        if (n >= BVSTK_I2C_ROUTE_MAX ||
            bvstk_i2c_devices_find_by_name(devices, d->endpoint.parent, &parent) != BVSTK_OK ||
            parent == device_id || d->endpoint.channel >= devices->items[parent].endpoint.router.channel_count)
            return BVSTK_ERR_MALFORMED;
        for (i = 0; i < n; ++i) if (switches[i] == parent) return BVSTK_ERR_MALFORMED;
        switches[n] = parent; channels[n++] = d->endpoint.channel; current = parent;
    }
    for (i = 0; i < n / 2U; ++i) {
        size_t s = switches[i]; uint8_t c = channels[i];
        switches[i] = switches[n - 1U - i]; channels[i] = channels[n - 1U - i];
        switches[n - 1U - i] = s; channels[n - 1U - i] = c;
    }
    *count = n;
    return BVSTK_OK;
}

size_t bvstk_i2c_devices_addresses(const bvstk_i2c_devices_t *devices, uint8_t out[128])
{
    uint8_t seen[128] = {0};
    size_t count = 0;
    if (!devices || !devices->initialized || !out) return 0;
    for (size_t i = 0; i < devices->count; ++i) {
        const bvstk_i2c_device_t *d = &devices->items[i];
        for (unsigned j = 0; j < (d->endpoint.space_count ? d->endpoint.space_count : 1U); ++j) {
            uint8_t address = d->endpoint.space_count ? d->endpoint.spaces[j].address : d->addr_7b;
            if (!seen[address]) { seen[address] = 1; out[count++] = address; }
        }
    }
    return count;
}

bvstk_status_t bvstk_i2c_devices_validate_topology(const bvstk_i2c_devices_t *devices)
{
    size_t i, j, k, a[BVSTK_I2C_ROUTE_MAX], b[BVSTK_I2C_ROUTE_MAX], na, nb;
    uint8_t ca[BVSTK_I2C_ROUTE_MAX], cb[BVSTK_I2C_ROUTE_MAX];
    if (!devices || !devices->initialized) return BVSTK_ERR_NOT_READY;
    for (i = 0; i < devices->count; ++i) {
        if (bvstk_i2c_devices_route(devices, i, a, ca, &na) != BVSTK_OK) return BVSTK_ERR_MALFORMED;
        for (j = 0; j < i; ++j) {
            if (!strcasecmp(devices->items[i].name, devices->items[j].name)) return BVSTK_ERR_MALFORMED;
            const bvstk_i2c_device_t *di = &devices->items[i], *dj = &devices->items[j];
            int collision = 0;
            unsigned spaces = di->endpoint.space_count ? di->endpoint.space_count : 1U;
            for (unsigned s = 0; s < spaces; ++s) {
                unsigned address = di->endpoint.space_count ? di->endpoint.spaces[s].address : di->addr_7b;
                if (bvstk_i2c_address_space(&dj->endpoint, dj->addr_7b, address) >= 0) collision = 1;
            }
            if (!collision) continue;
            if (bvstk_i2c_devices_route(devices, j, b, cb, &nb) != BVSTK_OK) return BVSTK_ERR_MALFORMED;
            /* Equal addresses are safe only behind different channels of a
             * shared switch; root/ancestor address collisions cannot be isolated. */
            for (k = 0; k < na && k < nb; ++k) {
                if (a[k] != b[k]) break;
                if (ca[k] != cb[k]) break;
            }
            if (k == na || k == nb || a[k] != b[k]) return BVSTK_ERR_MALFORMED;
        }
    }
    return BVSTK_OK;
}
