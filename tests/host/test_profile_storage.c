#define _POSIX_C_SOURCE 200809L
#include "apps/neutrino/config/bvstk_i2c_config_store.h"
#include "apps/neutrino/config/bvstk_mdio_config_store.h"
#include "apps/neutrino/config/bvstk_profile_store.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_mdio_config_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bvstk_neutrino_i2c_config_store_t i2c, reloaded_i2c;
static bvstk_neutrino_mdio_config_store_t mdio, reloaded_mdio;
static char json[BVSTK_PROFILE_JSON_MAX + 1U];
static void path(char *out, size_t cap, const char *a, const char *b)
{
    int n = snprintf(out, cap, "%s/%s", a, b);
    assert(n > 0 && (size_t)n < cap);
}
static void read_json(const char *file)
{
    FILE *f = fopen(file, "rb");
    assert(f);
    size_t n = fread(json, 1, sizeof(json) - 1U, f);
    assert(feof(f) && !ferror(f) && !fclose(f));
    json[n] = 0;
}
static void invalid(const char *file)
{
    FILE *f = fopen(file, "wb");
    assert(f && fwrite("{invalid}", 1, 9, f) == 9 && !fclose(f));
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    char source[512], idir[512], mdir[512], primary[512], backup[520];
    assert(!mkdir(argv[1], 0700));
    path(idir, sizeof(idir), argv[1], "i2c");
    path(mdir, sizeof(mdir), argv[1], "mdio");
    path(source, sizeof(source), argv[2], "configs/i2c/axp15060/axp15060.json");
    read_json(source);
    assert(bvstk_neutrino_profile_save(idir, "axp15060/axp15060.json", json, strlen(json)));
    assert(bvstk_neutrino_i2c_config_store_init(&i2c, idir, "/missing-legacy") && i2c.device_count == 1);
    i2c_device_config_t changed = i2c.devices[0];
    strcpy(changed.profile.label, "changed I2C");
    assert(bvstk_neutrino_i2c_config_store_save(&i2c, &changed));
    path(primary, sizeof(primary), idir, "axp15060/axp15060.json");
    assert(snprintf(backup, sizeof(backup), "%s.bak", primary) < (int)sizeof(backup));
    read_json(backup);
    i2c_device_config_t previous_i2c;
    assert(bvstk_i2c_config_parse_json(json, &previous_i2c) == BVSTK_OK);
    assert(strcmp(previous_i2c.profile.label, changed.profile.label));
    assert(!unlink(primary));
    assert(bvstk_neutrino_i2c_config_store_init(&reloaded_i2c, idir, "/missing-legacy"));
    assert(reloaded_i2c.device_count == 1);
    assert(!strcmp(reloaded_i2c.devices[0].profile.label, previous_i2c.profile.label));
    assert(!strcmp(reloaded_i2c.devices[0].profile.icon, previous_i2c.profile.icon));
    invalid(primary);
    assert(!bvstk_neutrino_i2c_config_store_init(&reloaded_i2c, idir, "/missing-legacy"));
    assert(!unlink(primary));
    assert(bvstk_neutrino_i2c_config_store_save(&i2c, &changed));
    assert(bvstk_neutrino_i2c_config_store_init(&reloaded_i2c, idir, "/missing-legacy"));
    assert(!strcmp(reloaded_i2c.devices[0].profile.label, changed.profile.label));

    path(source, sizeof(source), argv[2], "configs/mdio/lan8720/lan8720.json");
    read_json(source);
    assert(bvstk_neutrino_profile_save(mdir, "lan8720/lan8720.json", json, strlen(json)));
    assert(bvstk_neutrino_mdio_config_store_init(&mdio, mdir, "/missing-legacy") && mdio.device_count == 1);
    mdio_phy_config_t mchanged = mdio.devices[0];
    strcpy(mchanged.profile.label, "changed MDIO");
    assert(bvstk_neutrino_mdio_config_store_save(&mdio, &mchanged));
    path(primary, sizeof(primary), mdir, "lan8720/lan8720.json");
    assert(snprintf(backup, sizeof(backup), "%s.bak", primary) < (int)sizeof(backup));
    read_json(backup);
    mdio_phy_config_t previous_mdio;
    assert(bvstk_mdio_config_parse(json, &previous_mdio) == BVSTK_OK);
    assert(strcmp(previous_mdio.profile.label, mchanged.profile.label));
    assert(!unlink(primary));
    assert(bvstk_neutrino_mdio_config_store_init(&reloaded_mdio, mdir, "/missing-legacy"));
    assert(reloaded_mdio.device_count == 1);
    assert(!strcmp(reloaded_mdio.devices[0].profile.label, previous_mdio.profile.label));
    assert(!strcmp(reloaded_mdio.devices[0].profile.icon, previous_mdio.profile.icon));
    invalid(primary);
    assert(!bvstk_neutrino_mdio_config_store_init(&reloaded_mdio, mdir, "/missing-legacy"));
    assert(!unlink(primary));
    assert(bvstk_neutrino_mdio_config_store_save(&mdio, &mchanged));
    assert(bvstk_neutrino_mdio_config_store_init(&reloaded_mdio, mdir, "/missing-legacy"));
    assert(!strcmp(reloaded_mdio.devices[0].profile.label, mchanged.profile.label));
    puts("Profile storage: I2C/MDIO backups, metadata, missing-file recovery and invalid-file rejection passed");
    return 0;
}
