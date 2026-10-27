#include "apps/neutrino/config/bvstk_mdio_config_store.h"
#include "apps/neutrino/config/bvstk_profile_store.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_mdio_config_codec.h"
#include "default_configs.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int accept_profile(void *context, const char *json, const char *relative)
{
    bvstk_neutrino_mdio_config_store_t *store = context;
    mdio_phy_config_t config;
    if (store->device_count >= MDIO_CFG_MAX_DEVICES ||
        bvstk_mdio_config_parse(json, &config) != BVSTK_OK ||
        !bvstk_device_profile_location_valid(relative, config.name, "")) return 0;
    for (size_t i = 0; i < store->device_count; ++i)
        if (!strcasecmp(config.name, store->devices[i].name) ||
            config.phy_addr == store->devices[i].phy_addr) return 0;
    strcpy(config.file_name, relative);
    store->devices[store->device_count++] = config;
    return 1;
}

int bvstk_neutrino_mdio_config_store_init(bvstk_neutrino_mdio_config_store_t *store,
    const char *primary, const char *legacy)
{
    if (!store) return 0;
    memset(store, 0, sizeof(*store));
    if (!primary) primary = "/flash/config/mdio";
    if (!legacy) legacy = "/flash/configs/mdio";
    if (strlen(primary) >= sizeof(store->primary_dir)) return 0;
    strcpy(store->primary_dir, primary);
    size_t loaded = bvstk_neutrino_profile_load(primary, accept_profile, store);
    int migrate = 0;
    if (!loaded) {
        loaded = bvstk_neutrino_profile_load(legacy, accept_profile, store);
        migrate = loaded != 0;
    }
    if (!loaded) {
        for (unsigned i = 0; i < DEFAULT_MDIO_CONFIG_FILES_COUNT; ++i) {
            const default_json_file_t *file = &DEFAULT_MDIO_CONFIG_FILES[i];
            if (!accept_profile(store, file->json, file->file_name)) {
                loaded = SIZE_MAX;
                break;
            }
            ++loaded;
        }
        migrate = 1;
    }
    if (!loaded || loaded == SIZE_MAX) {
        memset(store, 0, sizeof(*store));
        return 0;
    }
    store->initialized = 1;
    if (migrate)
        for (size_t i = 0; i < store->device_count; ++i)
            (void)bvstk_neutrino_mdio_config_store_save(store, &store->devices[i]);
    return 1;
}

const mdio_phy_config_t *bvstk_neutrino_mdio_config_store_get(
    const bvstk_neutrino_mdio_config_store_t *store, size_t id)
{
    return store && store->initialized && id < store->device_count ? &store->devices[id] : NULL;
}

int bvstk_neutrino_mdio_config_store_update(bvstk_neutrino_mdio_config_store_t *store,
    size_t id, const mdio_phy_config_t *config)
{
    const mdio_phy_config_t *previous = bvstk_neutrino_mdio_config_store_get(store, id);
    if (!previous || !config || bvstk_mdio_config_validate(config) != BVSTK_OK ||
        strcmp(previous->name, config->name) || strcmp(previous->file_name, config->file_name)) return 0;
    store->devices[id] = *config;
    return 1;
}

int bvstk_neutrino_mdio_config_store_save(const bvstk_neutrino_mdio_config_store_t *store,
    const mdio_phy_config_t *config)
{
    char relative[MDIO_CFG_FILE_NAME_MAX];
    size_t size = 0;
    if (!store || !store->initialized || !config ||
        !bvstk_device_profile_save_path(config->name, config->file_name, relative, sizeof(relative))) return 0;
    char *json = malloc(BVSTK_PROFILE_JSON_MAX + 1U);
    if (!json) return 0;
    bvstk_status_t status = bvstk_device_profile_format_mdio(config, json,
        BVSTK_PROFILE_JSON_MAX + 1U, &size);
    if (status == BVSTK_ERR_UNSUPPORTED && !config->profile.version)
        status = bvstk_mdio_config_format(config, json, BVSTK_PROFILE_JSON_MAX + 1U, &size);
    int ok = status == BVSTK_OK && bvstk_neutrino_profile_save(store->primary_dir, relative, json, size);
    free(json);
    return ok;
}
