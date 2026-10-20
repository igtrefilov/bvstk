#define _POSIX_C_SOURCE 200809L
#include "apps/neutrino/config/bvstk_i2c_config_store.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "default_configs.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_device_profile.h"

enum {
    BVSTK_I2C_CONFIG_FILE_MAX = BVSTK_PROFILE_JSON_MAX,
    BVSTK_I2C_CONFIG_JSON_MAX = BVSTK_PROFILE_JSON_MAX + 1U
};

static int directory_exists(const char *path)
{
    struct stat info;

    return path != NULL && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int ensure_directory_tree(const char *path)
{
    char current[512];
    char *cursor;
    size_t length;

    if (path == NULL || path[0] != '/' ||
        (length = strlen(path)) == 0U || length >= sizeof(current)) {
        return 0;
    }
    memcpy(current, path, length + 1U);
    for (cursor = current + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (mkdir(current, 0755) != 0 && errno != EEXIST) {
            return 0;
        }
        *cursor = '/';
    }
    return mkdir(current, 0755) == 0 || errno == EEXIST;
}

static int file_name_safe(const char *name)
{
    return bvstk_device_profile_path_valid(name);
}

static int read_file(const char *path, char **json)
{
    struct stat info;
    char *buffer;
    size_t offset = 0U;
    int fd;

    if (json != NULL) {
        *json = NULL;
    }
    if (path == NULL || json == NULL || stat(path, &info) != 0 ||
        info.st_size <= 0 || info.st_size > BVSTK_I2C_CONFIG_FILE_MAX) {
        return 0;
    }
    buffer = (char *)malloc((size_t)info.st_size + 1U);
    if (buffer == NULL) {
        return 0;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(buffer);
        return 0;
    }
    while (offset < (size_t)info.st_size) {
        ssize_t count = read(fd,
                             buffer + offset,
                             (size_t)info.st_size - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            close(fd);
            free(buffer);
            return 0;
        }
        offset += (size_t)count;
    }
    if (close(fd) != 0 || memchr(buffer, 0, offset) != NULL) {
        free(buffer);
        return 0;
    }
    buffer[offset] = '\0';
    *json = buffer;
    return 1;
}

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

static size_t load_directory(bvstk_neutrino_i2c_config_store_t *store,
                             const char *directory)
{
    char (*dirs)[I2C_CFG_FILE_NAME_MAX];
    size_t initial = store->device_count, count = 1;
    int failed = 0;
    if (!directory_exists(directory)) return 0;
    dirs = calloc(128U, sizeof(*dirs));
    if (!dirs) return SIZE_MAX;
    for (size_t d = 0; d < count && !failed; ++d) {
        char folder[512];
        DIR *handle;
        if (snprintf(folder, sizeof(folder), "%s%s%s", directory,
            d ? "/" : "", dirs[d]) >= (int)sizeof(folder) || !(handle = opendir(folder))) {
            failed = 1; break;
        }
        for (;;) {
            struct dirent *entry;
            struct stat info;
            char path[512], relative[I2C_CFG_FILE_NAME_MAX], alternative[512];
            char *json = NULL;
            i2c_device_config_t config;
            errno = 0;
            entry = readdir(handle);
            if (!entry) { if (errno) failed = 1; break; }
            if (entry->d_name[0] == '.') continue;
            if (snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name) >= (int)sizeof(path) ||
                lstat(path, &info) != 0 ||
                snprintf(relative, sizeof(relative), "%s%s%s", dirs[d], d ? "/" : "",
                    entry->d_name) >= (int)sizeof(relative)) { failed = 1; break; }
            if (S_ISLNK(info.st_mode)) { failed = 1; break; }
            if (S_ISDIR(info.st_mode)) {
                if (count >= 128U) { failed = 1; break; }
                strcpy(dirs[count++], relative);
                continue;
            }
            if (!file_name_safe(relative) || bvstk_device_profile_is_register_map(entry->d_name)) continue;
            const char *base = strrchr(dirs[d], '/');
            base = base ? base + 1 : dirs[d];
            if (d && !strcasecmp(entry->d_name, "profile.json")) {
                if (snprintf(alternative, sizeof(alternative), "%s/%s.json", folder, base) >= (int)sizeof(alternative))
                    { failed = 1; break; }
                if (!access(alternative, F_OK)) continue;
            }
            if (!d) {
                size_t n = strlen(entry->d_name) - 5U;
                if (snprintf(alternative, sizeof(alternative), "%s/%.*s/%s", folder,
                    (int)n, entry->d_name, entry->d_name) >= (int)sizeof(alternative)) { failed = 1; break; }
                if (!access(alternative, F_OK)) continue;
                if (snprintf(alternative, sizeof(alternative), "%s/%.*s/profile.json", folder,
                    (int)n, entry->d_name) >= (int)sizeof(alternative)) { failed = 1; break; }
                if (!access(alternative, F_OK)) continue;
            }
            if (store->device_count >= I2C_CFG_MAX_DEVICES || !read_file(path, &json)) { failed = 1; break; }
            int valid = bvstk_i2c_config_parse_json(json, &config) == BVSTK_OK;
            free(json);
            if (!valid || duplicate_name(store, config.name) ||
                !bvstk_device_profile_location_valid(relative, config.name, config.endpoint.parent))
                { failed = 1; break; }
            strcpy(config.file_name, relative);
            store->devices[store->device_count++] = config;
        }
        if (closedir(handle)) failed = 1;
    }
    free(dirs);
    return failed ? SIZE_MAX : store->device_count - initial;
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

        if (!file_name_safe(source->file_name) ||
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

static int write_all(int fd, const char *data, size_t length)
{
    size_t offset = 0U;

    while (offset < length) {
        ssize_t count = write(fd, data + offset, length - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return 0;
        }
        offset += (size_t)count;
    }
    return 1;
}

static int config_file_name(const i2c_device_config_t *config,
                            char *file_name,
                            size_t file_name_capacity)
{
    return bvstk_device_profile_save_path(config->name, config->file_name,
                                         file_name, file_name_capacity);
}

int bvstk_neutrino_i2c_config_store_save(
    const bvstk_neutrino_i2c_config_store_t *store,
    const i2c_device_config_t *config)
{
    char *json;
    size_t json_size = 0U;
    char file_name[I2C_CFG_FILE_NAME_MAX];
    char destination[512];
    char temporary[528];
    int fd = -1;
    int result = 0;

    if (store == NULL || store->initialized == 0U || config == NULL ||
        !ensure_directory_tree(store->primary_dir) ||
        !config_file_name(config, file_name, sizeof(file_name))) {
        return 0;
    }
    json = (char *)malloc(BVSTK_I2C_CONFIG_JSON_MAX);
    if (json == NULL) {
        return 0;
    }
    bvstk_status_t status = bvstk_device_profile_format_i2c(config, json,
                                      BVSTK_I2C_CONFIG_JSON_MAX, &json_size);
    if (status == BVSTK_ERR_UNSUPPORTED && !config->profile.version)
        status = bvstk_i2c_config_serialize_json(config, json,
                                      BVSTK_I2C_CONFIG_JSON_MAX, &json_size);
    int destination_length = snprintf(destination, sizeof(destination), "%s/%s",
                                      store->primary_dir, file_name);
    int temporary_length = snprintf(temporary, sizeof(temporary), "%s.tmp", destination);
    if (status != BVSTK_OK || destination_length <= 0 ||
        (size_t)destination_length >= sizeof(destination) || temporary_length <= 0 ||
        (size_t)temporary_length >= sizeof(temporary)) {
        free(json);
        return 0;
    }
    char parent[512];
    strcpy(parent, destination);
    char *slash = strrchr(parent, '/');
    if (!slash) { free(json); return 0; }
    *slash = 0;
    if (!ensure_directory_tree(parent)) { free(json); return 0; }
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0 && write_all(fd, json, json_size) && fsync(fd) == 0 &&
        close(fd) == 0) {
        fd = -1;
        if (rename(temporary, destination) == 0) {
            result = 1;
        }
    }
    if (fd >= 0) {
        close(fd);
    }
    if (!result) {
        unlink(temporary);
    }
    free(json);
    return result;
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
    loaded = load_directory(store, primary_dir);
    if (loaded == 0U) {
        loaded = load_directory(store, legacy_dir);
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
    if (migrate_to_primary && ensure_directory_tree(store->primary_dir)) {
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
