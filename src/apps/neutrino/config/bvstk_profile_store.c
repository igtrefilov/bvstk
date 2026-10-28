#define _POSIX_C_SOURCE 200809L
#include "apps/neutrino/config/bvstk_profile_store.h"
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "shared/config/bvstk_device_profile.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

int bvstk_neutrino_profile_mkdir(const char *path)
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
        struct stat info;
        if (lstat(current, &info) || !S_ISDIR(info.st_mode)) return 0;
        *cursor = '/';
    }
    if (mkdir(current, 0755) && errno != EEXIST) return 0;
    struct stat info;
    return lstat(current, &info) == 0 && S_ISDIR(info.st_mode);
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
        info.st_size <= 0 || (uint64_t)info.st_size > BVSTK_PROFILE_JSON_MAX) {
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

size_t bvstk_neutrino_profile_load(const char *directory,
    bvstk_neutrino_profile_load_fn accept, void *context)
{
    char (*dirs)[I2C_CFG_FILE_NAME_MAX];
    size_t loaded = 0, count = 1;
    int failed = 0;
    struct stat info;
    if (!directory || !accept) return SIZE_MAX;
    if (lstat(directory, &info)) return errno == ENOENT ? 0U : SIZE_MAX;
    if (!S_ISDIR(info.st_mode)) return SIZE_MAX;
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
            char file_name[I2C_CFG_FILE_NAME_MAX];
            strcpy(file_name, entry->d_name);
            size_t length = strlen(relative);
            /* A completed .bak recovers a missing canonical file, as in the
             * FreeRTOS loader. An existing invalid JSON still fails closed. */
            if (length > 9U && !strcasecmp(relative + length - 9U, ".json.bak")) {
                char canonical[512];
                relative[length - 4U] = 0;
                file_name[strlen(file_name) - 4U] = 0;
                if (snprintf(canonical, sizeof(canonical), "%s/%s", directory, relative) >= (int)sizeof(canonical))
                    { failed = 1; break; }
                if (!lstat(canonical, &info)) {
                    if (!S_ISREG(info.st_mode)) { failed = 1; break; }
                    continue;
                }
                if (errno != ENOENT) { failed = 1; break; }
            }
            if (!file_name_safe(relative) || bvstk_device_profile_is_register_map(file_name)) continue;
            const char *base = strrchr(dirs[d], '/');
            base = base ? base + 1 : dirs[d];
            if (d && !strcasecmp(file_name, "profile.json")) {
                if (snprintf(alternative, sizeof(alternative), "%s/%s.json", folder, base) >= (int)sizeof(alternative))
                    { failed = 1; break; }
                if (!access(alternative, F_OK)) continue;
            }
            if (!d) {
                size_t n = strlen(file_name) - 5U;
                if (snprintf(alternative, sizeof(alternative), "%s/%.*s/%s", folder,
                    (int)n, file_name, file_name) >= (int)sizeof(alternative)) { failed = 1; break; }
                if (!access(alternative, F_OK)) continue;
                if (snprintf(alternative, sizeof(alternative), "%s/%.*s/profile.json", folder,
                    (int)n, file_name) >= (int)sizeof(alternative)) { failed = 1; break; }
                if (!access(alternative, F_OK)) continue;
            }
            if (!read_file(path, &json)) { failed = 1; break; }
            int valid = accept(context, json, relative);
            free(json);
            if (!valid) { failed = 1; break; }
            ++loaded;
        }
        if (closedir(handle)) failed = 1;
    }
    free(dirs);
    return failed ? SIZE_MAX : loaded;
}

int bvstk_neutrino_profile_save(const char *directory, const char *relative,
    const char *json, size_t size)
{
    char path[BVSTK_FS_PATH_CAPACITY], parent[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_posix_t local;
    const bvstk_fs_storage_t *storage;
    bool custom = true;
    if (!directory || !file_name_safe(relative) || !json || !size) return 0;
    int n;
    /* Real device profiles use the mounted logical volume. Custom roots are
     * for explicit alternate stores/tests, never an unmounted /flash alias. */
    if (!strncmp(directory, "/flash", 6) && (!directory[6] || directory[6] == '/')) {
        storage = bvstk_fs_posix_default();
        custom = false;
        n = snprintf(path, sizeof(path), "flash:%s/%s", directory + 6, relative);
    } else {
        if (!bvstk_neutrino_profile_mkdir(directory)) return 0;
        const bvstk_fs_posix_volume_t v = {"flash", directory, false};
        if (bvstk_fs_posix_init(&local, &v, 1) != BVSTK_FS_OK) return 0;
        storage = bvstk_fs_posix_storage(&local);
        n = snprintf(path, sizeof(path), "flash:/%s", relative);
    }
    int ok = 0;
    if (storage && n > 0 && (size_t)n < sizeof(path)) {
        strcpy(parent, path);
        char *slash = strrchr(parent, '/');
        if (slash == strchr(parent, ':') + 1) slash[1] = 0;
        else *slash = 0;
        ok = bvstk_fs_mkdirs(storage, parent) == BVSTK_FS_OK &&
             bvstk_fs_save(storage, path, json, size) == BVSTK_FS_OK;
    }
    if (custom && bvstk_fs_posix_destroy(&local) != BVSTK_FS_OK) ok = 0;
    return ok;
}
