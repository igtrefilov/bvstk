#define _POSIX_C_SOURCE 200809L
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { int fd; } native_file_t;
typedef struct { DIR *dir; char path[BVSTK_FS_PATH_CAPACITY]; } native_dir_t;

static bvstk_fs_result_t error_result(int error)
{
    switch (error) {
    case ENOENT: return BVSTK_FS_NOT_FOUND;
    case ENOTDIR: case EISDIR: return BVSTK_FS_TYPE_ERROR;
    case EEXIST: return BVSTK_FS_EXISTS;
    case EACCES: case EPERM: case EROFS: case ENOTEMPTY: case ELOOP: return BVSTK_FS_DENIED;
    case ENAMETOOLONG: case EOVERFLOW: return BVSTK_FS_RANGE;
    case EINVAL: return BVSTK_FS_INVALID;
    case ENODEV: case ENXIO: return BVSTK_FS_NOT_READY;
    case ENOMEM: case EMFILE: case ENFILE: case EBUSY: case EAGAIN: return BVSTK_FS_BUSY;
    case ENOSYS: case ENOTSUP: return BVSTK_FS_UNSUPPORTED;
    default: return BVSTK_FS_IO;
    }
}

static int open_flags(int flags)
{
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    return flags | O_NONBLOCK;
}

/* Check every component, including the configured root. Logical paths reject
 * traversal; lstat prevents native symlinks from escaping a logical volume.
 * As with FatFs, external programs can still change files between operations;
 * readers detect changes where the filesystem's metadata permits it. */
static bvstk_fs_result_t inspect(char *path, bool missing_last)
{
    for (char *p = path + 1; ; ++p) {
        if (*p && *p != '/') continue;
        char saved = *p;
        struct stat info;
        *p = 0;
        int rc = lstat(path, &info), error = errno;
        *p = saved;
        if (rc) return error == ENOENT && !saved && missing_last ?
            BVSTK_FS_OK : error_result(error);
        if (S_ISLNK(info.st_mode)) return BVSTK_FS_DENIED;
        if (saved && !S_ISDIR(info.st_mode)) return BVSTK_FS_TYPE_ERROR;
        if (!saved) return BVSTK_FS_OK;
    }
}

static bvstk_fs_result_t root_ready(const bvstk_fs_posix_root_t *v)
{
    char path[BVSTK_FS_PATH_CAPACITY];
    struct stat root, parent;
    if (!v->root[0]) return BVSTK_FS_UNSUPPORTED;
    strcpy(path, v->root);
    bvstk_fs_result_t rc = inspect(path, false);
    if (rc == BVSTK_FS_NOT_FOUND) return BVSTK_FS_NOT_READY;
    if (rc != BVSTK_FS_OK) return rc;
    if (lstat(path, &root)) return error_result(errno);
    if (!S_ISDIR(root.st_mode)) return BVSTK_FS_NOT_READY;
    if (!v->require_mount) return BVSTK_FS_OK;
    char *slash = strrchr(path, '/');
    if (slash == path) slash[1] = 0;
    else *slash = 0;
    if (stat(path, &parent)) return error_result(errno);
    return root.st_dev != parent.st_dev ? BVSTK_FS_OK : BVSTK_FS_NOT_READY;
}

static bvstk_fs_result_t resolve(bvstk_fs_posix_t *p, const char *path,
    char native[BVSTK_FS_PATH_CAPACITY], bool mutation, bool missing_last)
{
    char canonical[BVSTK_FS_PATH_CAPACITY];
    if (!p || !p->initialized || !path) return BVSTK_FS_NOT_READY;
    bvstk_fs_result_t rc = bvstk_fs_validate_path((const uint8_t *)path, strlen(path),
                                                 canonical, sizeof(canonical));
    if (rc != BVSTK_FS_OK) return rc;
    const char *colon = strchr(canonical, ':');
    size_t length = (size_t)(colon - canonical);
    if (mutation && !colon[2]) return BVSTK_FS_INVALID;
    const bvstk_fs_posix_root_t *v = NULL;
    for (size_t i = 0; i < p->volume_count; ++i)
        if (strlen(p->volumes[i].name) == length && !strncmp(canonical, p->volumes[i].name, length))
            v = &p->volumes[i];
    if (!v) return BVSTK_FS_NOT_FOUND;
    rc = root_ready(v);
    if (rc != BVSTK_FS_OK) return rc;
    int n = snprintf(native, BVSTK_FS_PATH_CAPACITY, "%s%s%s", v->root,
        colon[2] && strcmp(v->root, "/") ? "/" : "", colon + 2);
    if (n < 0 || n >= (int)BVSTK_FS_PATH_CAPACITY) return BVSTK_FS_RANGE;
    return inspect(native, missing_last);
}

static bvstk_fs_result_t convert(const struct stat *s, bvstk_fs_info_t *info)
{
    memset(info, 0, sizeof(*info));
    if (!S_ISREG(s->st_mode) && !S_ISDIR(s->st_mode)) return BVSTK_FS_UNSUPPORTED;
    if (s->st_size < 0) return BVSTK_FS_RANGE;
    info->type = S_ISDIR(s->st_mode) ? BVSTK_FS_ENTRY_DIR : BVSTK_FS_ENTRY_FILE;
    info->size = info->type == BVSTK_FS_ENTRY_FILE ? (uint64_t)s->st_size : 0;
    info->stamp = (uint32_t)s->st_mtime ^ ((uint32_t)s->st_ctime << 1) ^ (uint32_t)s->st_ino;
    if (!(s->st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) info->attributes = 1;
    return BVSTK_FS_OK;
}

static size_t volume_count(void *context) { return ((bvstk_fs_posix_t *)context)->volume_count; }
static bvstk_fs_result_t volume(void *context, size_t index, bvstk_fs_volume_t *out)
{
    bvstk_fs_posix_t *p = context;
    if (!out || index >= p->volume_count) return BVSTK_FS_RANGE;
    strcpy(out->name, p->volumes[index].name);
    bvstk_fs_result_t rc = root_ready(&p->volumes[index]);
    out->state = rc == BVSTK_FS_UNSUPPORTED ? 2 : (uint8_t)(rc == BVSTK_FS_OK);
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t stat_path(void *context, const char *path, bvstk_fs_info_t *out)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    if (!out) return BVSTK_FS_INVALID;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, false, false);
    if (rc == BVSTK_FS_OK) rc = lstat(native, &info) ? error_result(errno) : convert(&info, out);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t file_open(void *context, const char *path, void **out, uint64_t *size)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    if (!out || !size) return BVSTK_FS_INVALID;
    *out = NULL;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, false, false);
    native_file_t *file = NULL;
    if (rc == BVSTK_FS_OK && !(file = malloc(sizeof(*file)))) rc = BVSTK_FS_BUSY;
    if (rc == BVSTK_FS_OK) {
        file->fd = open(native, open_flags(O_RDONLY));
        if (file->fd < 0) rc = error_result(errno);
        else {
            if (fstat(file->fd, &info)) rc = error_result(errno);
            else if (!S_ISREG(info.st_mode)) rc = BVSTK_FS_TYPE_ERROR;
            else if (info.st_size < 0) rc = BVSTK_FS_RANGE;
            if (rc != BVSTK_FS_OK) close(file->fd);
        }
    }
    if (rc == BVSTK_FS_OK) { *out = file; *size = (uint64_t)info.st_size; ++p->files; }
    else free(file);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t file_read(void *context, void *handle, uint8_t *data, size_t capacity, size_t *size)
{
    native_file_t *file = handle;
    (void)context;
    if (!file || !size || (!data && capacity)) return BVSTK_FS_INVALID;
    ssize_t n;
    do { n = read(file->fd, data, capacity); } while (n < 0 && errno == EINTR);
    *size = n < 0 ? 0U : (size_t)n;
    return n < 0 ? error_result(errno) : BVSTK_FS_OK;
}

static void file_close(void *context, void *handle)
{
    bvstk_fs_posix_t *p = context;
    native_file_t *file = handle;
    if (!file) return;
    (void)close(file->fd);
    free(file);
    pthread_mutex_lock(&p->mutex);
    --p->files;
    pthread_mutex_unlock(&p->mutex);
}

static bvstk_fs_result_t dir_open(void *context, const char *path, void **out)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    if (!out) return BVSTK_FS_INVALID;
    *out = NULL;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, false, false);
    native_dir_t *dir = NULL;
    if (rc == BVSTK_FS_OK && !(dir = malloc(sizeof(*dir)))) rc = BVSTK_FS_BUSY;
    if (rc == BVSTK_FS_OK && !(dir->dir = opendir(native))) rc = error_result(errno);
    if (rc == BVSTK_FS_OK) { strcpy(dir->path, native); *out = dir; ++p->directories; }
    else free(dir);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t dir_read(void *context, void *handle, char *name, size_t capacity, bvstk_fs_info_t *out)
{
    native_dir_t *dir = handle;
    struct dirent *entry;
    struct stat info;
    char path[BVSTK_FS_PATH_CAPACITY], checked_name[BVSTK_FS_PATH_CAPACITY];
    (void)context;
    if (!dir || !name || !capacity || !out) return BVSTK_FS_INVALID;
    do {
        errno = 0;
        entry = readdir(dir->dir);
    } while (entry && (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")));
    if (!entry) {
        if (errno) return error_result(errno);
        name[0] = 0; memset(out, 0, sizeof(*out)); return BVSTK_FS_OK;
    }
    /* Validate the actual entry, including UTF-8, without changing its name. */
    int n = snprintf(checked_name, sizeof(checked_name), "v:/%s", entry->d_name);
    if (n < 0 || (size_t)n >= sizeof(checked_name)) return BVSTK_FS_RANGE;
    bvstk_fs_result_t rc = bvstk_fs_validate_path((const uint8_t *)checked_name, (size_t)n, path, sizeof(path));
    if (rc != BVSTK_FS_OK) return rc;
    if (strlen(entry->d_name) >= capacity) return BVSTK_FS_RANGE;
    n = snprintf(path, sizeof(path), "%s/%s", dir->path, entry->d_name);
    if (n < 0 || (size_t)n >= sizeof(path)) return BVSTK_FS_RANGE;
    if (lstat(path, &info)) return error_result(errno);
    if (S_ISLNK(info.st_mode)) return BVSTK_FS_DENIED;
    rc = convert(&info, out);
    if (rc == BVSTK_FS_OK) strcpy(name, entry->d_name);
    return rc;
}

static void dir_close(void *context, void *handle)
{
    bvstk_fs_posix_t *p = context;
    native_dir_t *dir = handle;
    if (!dir) return;
    (void)closedir(dir->dir);
    free(dir);
    pthread_mutex_lock(&p->mutex);
    --p->directories;
    pthread_mutex_unlock(&p->mutex);
}

static bvstk_fs_result_t create(void *context, const char *path, bool overwrite, void **out)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    if (!out) return BVSTK_FS_INVALID;
    *out = NULL;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, true, true);
    native_file_t *file = NULL;
    if (rc == BVSTK_FS_OK && !lstat(native, &info) && !S_ISREG(info.st_mode)) rc = BVSTK_FS_TYPE_ERROR;
    if (rc == BVSTK_FS_OK && !(file = malloc(sizeof(*file)))) rc = BVSTK_FS_BUSY;
    if (rc == BVSTK_FS_OK) {
        file->fd = open(native, open_flags(O_WRONLY | O_CREAT | (overwrite ? O_TRUNC : O_EXCL)), 0644);
        if (file->fd < 0) rc = error_result(errno);
    }
    if (rc == BVSTK_FS_OK) { *out = file; ++p->files; }
    else free(file);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t append_file(void *context, const char *path, void **out)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    if (!out) return BVSTK_FS_INVALID;
    *out = NULL;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, true, true);
    native_file_t *file = NULL;
    if (rc == BVSTK_FS_OK && !lstat(native, &info) && !S_ISREG(info.st_mode)) rc = BVSTK_FS_TYPE_ERROR;
    if (rc == BVSTK_FS_OK && !(file = malloc(sizeof(*file)))) rc = BVSTK_FS_BUSY;
    if (rc == BVSTK_FS_OK) {
        file->fd = open(native, open_flags(O_WRONLY | O_CREAT | O_APPEND), 0644);
        if (file->fd < 0) rc = error_result(errno);
    }
    if (rc == BVSTK_FS_OK) { *out = file; ++p->files; }
    else free(file);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t write_file(void *context, void *handle, const uint8_t *data, size_t size, size_t *written)
{
    native_file_t *file = handle;
    (void)context;
    *written = 0;
    while (*written < size) {
        ssize_t n = write(file->fd, data + *written, size - *written);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return error_result(errno);
        if (!n) return BVSTK_FS_IO;
        *written += (size_t)n;
    }
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t finish_file(void *context, void *handle, bool sync)
{
    bvstk_fs_posix_t *p = context;
    native_file_t *file = handle;
    bvstk_fs_result_t rc = sync && fsync(file->fd) ? error_result(errno) : BVSTK_FS_OK;
    if (close(file->fd) && rc == BVSTK_FS_OK) rc = error_result(errno);
    free(file);
    pthread_mutex_lock(&p->mutex);
    --p->files;
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t make_dir(void *context, const char *path)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, true, true);
    if (rc == BVSTK_FS_OK && mkdir(native, 0755)) rc = error_result(errno);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t remove_path(void *context, const char *path)
{
    bvstk_fs_posix_t *p = context;
    char native[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, path, native, true, false);
    if (rc == BVSTK_FS_OK && lstat(native, &info)) rc = error_result(errno);
    if (rc == BVSTK_FS_OK) {
        if (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode)) rc = BVSTK_FS_TYPE_ERROR;
        else if ((S_ISDIR(info.st_mode) ? rmdir(native) : unlink(native))) rc = error_result(errno);
    }
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t rename_path(void *context, const char *from, const char *to)
{
    bvstk_fs_posix_t *p = context;
    char a[BVSTK_FS_PATH_CAPACITY], b[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, from, a, true, false);
    if (rc == BVSTK_FS_OK) rc = resolve(p, to, b, true, true);
    if (rc == BVSTK_FS_OK) {
        if (!lstat(b, &info)) rc = BVSTK_FS_EXISTS;
        else if (errno != ENOENT) rc = error_result(errno);
        else if (rename(a, b)) rc = error_result(errno);
    }
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

static bvstk_fs_result_t backup_file(const char *from, const char *to)
{
    char temporary[BVSTK_FS_PATH_CAPACITY];
    uint8_t buffer[1024];
    struct stat before, after;
    int out = -1, in = open(from, open_flags(O_RDONLY));
    if (in < 0) return error_result(errno);
    bvstk_fs_result_t rc = fstat(in, &before) ? error_result(errno) : BVSTK_FS_OK;
    for (unsigned i = 0; rc == BVSTK_FS_OK; ++i) {
        int n = snprintf(temporary, sizeof(temporary), "%s.tmp%u", to, i);
        if (n < 0 || (size_t)n >= sizeof(temporary)) { rc = BVSTK_FS_RANGE; break; }
        out = open(temporary, open_flags(O_WRONLY | O_CREAT | O_EXCL), 0644);
        if (out >= 0) break;
        if (errno != EEXIST) { rc = error_result(errno); break; }
        if (i == 15U) { rc = BVSTK_FS_BUSY; break; }
    }
    while (rc == BVSTK_FS_OK) {
        ssize_t n = read(in, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { rc = error_result(errno); break; }
        if (!n) break;
        native_file_t file = {out};
        size_t written;
        rc = write_file(NULL, &file, buffer, (size_t)n, &written);
    }
    if (rc == BVSTK_FS_OK && fstat(in, &after)) rc = error_result(errno);
    if (rc == BVSTK_FS_OK && (before.st_size != after.st_size || before.st_mtime != after.st_mtime))
        rc = BVSTK_FS_CHANGED;
    if (out >= 0) {
        if (rc == BVSTK_FS_OK && fsync(out)) rc = error_result(errno);
        if (close(out) && rc == BVSTK_FS_OK) rc = error_result(errno);
    }
    if (close(in) && rc == BVSTK_FS_OK) rc = error_result(errno);
    if (rc == BVSTK_FS_OK && rename(temporary, to)) rc = error_result(errno);
    if (rc != BVSTK_FS_OK && out >= 0) (void)unlink(temporary);
    return rc;
}

static bvstk_fs_result_t install(void *context, const char *temporary, const char *path, const char *backup)
{
    bvstk_fs_posix_t *p = context;
    char a[BVSTK_FS_PATH_CAPACITY], b[BVSTK_FS_PATH_CAPACITY], c[BVSTK_FS_PATH_CAPACITY];
    struct stat info;
    pthread_mutex_lock(&p->mutex);
    bvstk_fs_result_t rc = resolve(p, temporary, a, true, false);
    if (rc == BVSTK_FS_OK) rc = resolve(p, path, b, true, true);
    if (rc == BVSTK_FS_OK) rc = resolve(p, backup, c, true, true);
    if (rc == BVSTK_FS_OK) {
        if (!lstat(b, &info)) {
            if (!S_ISREG(info.st_mode)) rc = BVSTK_FS_TYPE_ERROR;
            else if (!lstat(c, &info) && !S_ISREG(info.st_mode)) rc = BVSTK_FS_TYPE_ERROR;
            else rc = backup_file(b, c);
        } else if (errno != ENOENT) rc = error_result(errno);
    }
    if (rc == BVSTK_FS_OK && rename(a, b)) rc = error_result(errno);
    pthread_mutex_unlock(&p->mutex);
    return rc;
}

bvstk_fs_result_t bvstk_fs_posix_init(bvstk_fs_posix_t *p, const bvstk_fs_posix_volume_t *volumes, size_t count)
{
    if (!p || !volumes || !count || count > BVSTK_FS_POSIX_VOLUMES_MAX) return BVSTK_FS_INVALID;
    memset(p, 0, sizeof(*p));
    for (size_t i = 0; i < count; ++i) {
        char logical[BVSTK_FS_PATH_CAPACITY];
        if (bvstk_fs_path(volumes[i].name, "/", logical, sizeof(logical)) != BVSTK_FS_OK) return BVSTK_FS_INVALID;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(volumes[i].name, p->volumes[j].name)) return BVSTK_FS_INVALID;
        strcpy(p->volumes[i].name, volumes[i].name);
        if (volumes[i].root) {
            size_t n = strlen(volumes[i].root);
            if (!n || n >= sizeof(p->volumes[i].root) || volumes[i].root[0] != '/') return BVSTK_FS_INVALID;
            strcpy(p->volumes[i].root, volumes[i].root);
            while (n > 1 && p->volumes[i].root[n - 1] == '/') p->volumes[i].root[--n] = 0;
            /* A native root is configuration, but still cannot contain dot
             * components that would bypass component-by-component checks. */
            char *root = p->volumes[i].root;
            if (strstr(root, "//") || strstr(root, "/./") || strstr(root, "/../") ||
                (n >= 2 && !strcmp(root + n - 2, "/.")) ||
                (n >= 3 && !strcmp(root + n - 3, "/.."))) return BVSTK_FS_INVALID;
        }
        p->volumes[i].require_mount = volumes[i].require_mount;
    }
    if (pthread_mutex_init(&p->mutex, NULL)) return BVSTK_FS_BUSY;
    p->volume_count = count;
    p->reader = (bvstk_fs_backend_t){p, volume_count, volume, stat_path, file_open,
        file_read, file_close, dir_open, dir_read, dir_close};
    p->storage = (bvstk_fs_storage_t){&p->reader, create, write_file, finish_file,
        make_dir, remove_path, rename_path, install, append_file};
    p->initialized = true;
    return BVSTK_FS_OK;
}

bvstk_fs_result_t bvstk_fs_posix_destroy(bvstk_fs_posix_t *p)
{
    if (!p || !p->initialized) return BVSTK_FS_NOT_READY;
    pthread_mutex_lock(&p->mutex);
    bool busy = p->files || p->directories;
    pthread_mutex_unlock(&p->mutex);
    if (busy) return BVSTK_FS_BUSY;
    if (pthread_mutex_destroy(&p->mutex)) return BVSTK_FS_BUSY;
    p->initialized = false;
    return BVSTK_FS_OK;
}

const bvstk_fs_storage_t *bvstk_fs_posix_storage(bvstk_fs_posix_t *p)
{
    return p && p->initialized ? &p->storage : NULL;
}

static bvstk_fs_posix_t default_port;
static pthread_once_t default_once = PTHREAD_ONCE_INIT;
static void init_default(void)
{
    const bvstk_fs_posix_volume_t volumes[] = {
        {"sd", "/sd", true}, {"flash", "/flash", true}, {"sd-pl", NULL, false}
    };
    (void)bvstk_fs_posix_init(&default_port, volumes, 3);
}

const bvstk_fs_storage_t *bvstk_fs_posix_default(void)
{
    if (pthread_once(&default_once, init_default)) return NULL;
    return bvstk_fs_posix_storage(&default_port);
}
