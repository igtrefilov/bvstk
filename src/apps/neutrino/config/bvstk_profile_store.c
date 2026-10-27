#define _POSIX_C_SOURCE 200809L
#include "apps/neutrino/config/bvstk_profile_store.h"
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
    if (stat(directory, &info)) return errno == ENOENT ? 0U : SIZE_MAX;
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

int bvstk_neutrino_profile_save(const char *directory, const char *relative,
    const char *json, size_t size)
{
    char destination[512], temporary[528], parent[512];
    if (!directory || !file_name_safe(relative) || !json || !size ||
        !bvstk_neutrino_profile_mkdir(directory)) return 0;
    int n = snprintf(destination, sizeof(destination), "%s/%s", directory, relative);
    if (n <= 0 || (size_t)n >= sizeof(destination)) return 0;
    n = snprintf(temporary, sizeof(temporary), "%s.tmp", destination);
    if (n <= 0 || (size_t)n >= sizeof(temporary)) return 0;
    strcpy(parent, destination);
    char *slash = strrchr(parent, '/');
    if (!slash) return 0;
    *slash = 0;
    if (!bvstk_neutrino_profile_mkdir(parent)) return 0;
    int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    int ok = write_all(fd, json, size) && fsync(fd) == 0;
    if (close(fd)) ok = 0;
    if (ok && rename(temporary, destination) == 0) return 1;
    unlink(temporary);
    return 0;
}
