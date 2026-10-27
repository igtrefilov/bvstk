#define _POSIX_C_SOURCE 200809L
#include "apps/neutrino/config/bvstk_i2c_config_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "default_configs.h"
#include "apps/neutrino/config/bvstk_profile_store.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_device_profile.h"

static int duplicate_name(const bvstk_neutrino_i2c_config_store_t *store,
                          const char *name)
{
    size_t index;

    for (index = 0U; index < store->device_count; ++index) {
        if (strcasecmp(store->devices[index].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int accept_profile(void *context, const char *json, const char *relative)
{
    bvstk_neutrino_i2c_config_store_t *store = context;
    i2c_device_config_t config;
    if (store->device_count >= I2C_CFG_MAX_DEVICES ||
        bvstk_i2c_config_parse_json(json, &config) != BVSTK_OK ||
        duplicate_name(store, config.name) ||
        !bvstk_device_profile_location_valid(relative, config.name, config.endpoint.parent)) return 0;
    strcpy(config.file_name, relative);
    store->devices[store->device_count++] = config;
    return 1;
}

static size_t load_embedded_defaults(
    bvstk_neutrino_i2c_config_store_t *store)
{
    unsigned int index;

    for (index = 0U;
         index < DEFAULT_I2C_CONFIG_FILES_COUNT &&
         store->device_count < I2C_CFG_MAX_DEVICES;
         ++index) {
        const default_json_file_t *source = &DEFAULT_I2C_CONFIG_FILES[index];
        i2c_device_config_t config;

        if (!bvstk_device_profile_path_valid(source->file_name) ||
            bvstk_i2c_config_parse_json(source->json, &config) != BVSTK_OK ||
            duplicate_name(store, config.name)) {
            continue;
        }
        strncpy(config.file_name,
                source->file_name,
                sizeof(config.file_name) - 1U);
        config.file_name[sizeof(config.file_name) - 1U] = '\0';
        store->devices[store->device_count++] = config;
    }
    return store->device_count;
}

int bvstk_neutrino_i2c_config_store_save(
    const bvstk_neutrino_i2c_config_store_t *store, const i2c_device_config_t *config)
{
    char relative[I2C_CFG_FILE_NAME_MAX];
    size_t size = 0;
    if (!store || !store->initialized || !config ||
        !bvstk_device_profile_save_path(config->name, config->file_name, relative, sizeof(relative))) return 0;
    char *json = malloc(BVSTK_PROFILE_JSON_MAX + 1U);
    if (!json) return 0;
    bvstk_status_t status = bvstk_device_profile_format_i2c(config, json,
        BVSTK_PROFILE_JSON_MAX + 1U, &size);
    if (status == BVSTK_ERR_UNSUPPORTED && !config->profile.version)
        status = bvstk_i2c_config_serialize_json(config, json, BVSTK_PROFILE_JSON_MAX + 1U, &size);
    int ok = status == BVSTK_OK && bvstk_neutrino_profile_save(store->primary_dir, relative, json, size);
    free(json);
    return ok;
}

int bvstk_neutrino_i2c_config_store_init(
    bvstk_neutrino_i2c_config_store_t *store,
    const char *primary_dir,
    const char *legacy_dir)
{
    size_t loaded;
    size_t index;
    int migrate_to_primary = 0;

    if (store == NULL) {
        return 0;
    }
    memset(store, 0, sizeof(*store));
    if (primary_dir == NULL) {
        primary_dir = BVSTK_NEUTRINO_I2C_CONFIG_DIR;
    }
    if (legacy_dir == NULL) {
        legacy_dir = BVSTK_NEUTRINO_I2C_LEGACY_DIR;
    }
    if (strlen(primary_dir) >= sizeof(store->primary_dir)) {
        return 0;
    }
    strcpy(store->primary_dir, primary_dir);
    loaded = bvstk_neutrino_profile_load(primary_dir, accept_profile, store);
    if (loaded == 0U) {
        loaded = bvstk_neutrino_profile_load(legacy_dir, accept_profile, store);
        migrate_to_primary = loaded != 0U;
    }
    if (loaded == 0U) {
        loaded = load_embedded_defaults(store);
        migrate_to_primary = loaded != 0U;
    }
    if (loaded == 0U || loaded == SIZE_MAX) {
        memset(store, 0, sizeof(*store));
        return 0;
    }
    store->initialized = 1U;

    /* Migrate legacy/default configurations to the canonical path. */
    if (migrate_to_primary && bvstk_neutrino_profile_mkdir(store->primary_dir)) {
        for (index = 0U; index < store->device_count; ++index) {
            (void)bvstk_neutrino_i2c_config_store_save(store,
                                                        &store->devices[index]);
        }
    }
    return 1;
}

void bvstk_neutrino_i2c_config_store_shutdown(
    bvstk_neutrino_i2c_config_store_t *store)
{
    if (store != NULL) {
        memset(store, 0, sizeof(*store));
    }
}

size_t bvstk_neutrino_i2c_config_store_count(
    const bvstk_neutrino_i2c_config_store_t *store)
{
    return store != NULL && store->initialized != 0U
               ? store->device_count : 0U;
}

const i2c_device_config_t *bvstk_neutrino_i2c_config_store_devices(
    const bvstk_neutrino_i2c_config_store_t *store)
{
    return store != NULL && store->initialized != 0U
               ? store->devices : NULL;
}

const i2c_device_config_t *bvstk_neutrino_i2c_config_store_get(
    const bvstk_neutrino_i2c_config_store_t *store,
    size_t device_id)
{
    return store != NULL && store->initialized != 0U &&
           device_id < store->device_count
               ? &store->devices[device_id] : NULL;
}

const i2c_device_config_t *bvstk_neutrino_i2c_config_store_find_name(
    const bvstk_neutrino_i2c_config_store_t *store,
    const char *name,
    size_t *device_id)
{
    size_t index;

    if (store == NULL || store->initialized == 0U || name == NULL) {
        return NULL;
    }
    for (index = 0U; index < store->device_count; ++index) {
        if (strcasecmp(store->devices[index].name, name) == 0) {
            if (device_id != NULL) *device_id = index;
            return &store->devices[index];
        }
    }
    return NULL;
}

const i2c_device_config_t *bvstk_neutrino_i2c_config_store_find_addr(
    const bvstk_neutrino_i2c_config_store_t *store,
    uint8_t addr_7b,
    size_t *device_id)
{
    size_t index, found = SIZE_MAX;

    if (store == NULL || store->initialized == 0U) {
        return NULL;
    }
    addr_7b &= UINT8_C(0x7F);
    for (index = 0U; index < store->device_count; ++index) {
        if (bvstk_i2c_address_space(&store->devices[index].endpoint,
                                   store->devices[index].addr_7b, addr_7b) >= 0) {
            if (found != SIZE_MAX) return NULL;
            found = index;
        }
    }
    if (found == SIZE_MAX) return NULL;
    if (device_id) *device_id = found;
    return &store->devices[found];
}

int bvstk_neutrino_i2c_config_store_update(
    bvstk_neutrino_i2c_config_store_t *store,
    size_t device_id,
    const i2c_device_config_t *config)
{
    i2c_device_config_t next;

    if (store == NULL || store->initialized == 0U || config == NULL ||
        device_id >= store->device_count ||
        bvstk_i2c_config_validate(config) != BVSTK_OK) {
        return 0;
    }
    next = *config;
    strncpy(next.file_name,
            store->devices[device_id].file_name,
            sizeof(next.file_name) - 1U);
    next.file_name[sizeof(next.file_name) - 1U] = '\0';
    store->devices[device_id] = next;
    return 1;
}
