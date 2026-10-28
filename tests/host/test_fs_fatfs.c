/* Actual FreeRTOS adapter and fs_shared primitives over an API-only FatFs shim.
 * This checks composition/locking/error propagation, not on-media FAT layout. */
#define _DEFAULT_SOURCE
#define DIR NativeDir
#include <dirent.h>
#undef DIR
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "apps/freertos/storage/fs/fs_dcp2_backend.h"
#include "apps/freertos/storage/fs/fs_devices.h"
#include "fs_storage_contract.h"

static char root[512];
static int ready[3] = {1, 1, 1}, allocated, opened, fail_sync, fail_close, fail_install;
static pthread_mutex_t mutexes[3] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
static SemaphoreHandle_t handles[3] = {&mutexes[0], &mutexes[1], &mutexes[2]};
static fs_shared_ctx_t contexts[] = {
    {.ready=&ready[0], .mutex=&handles[0], .root="0:/"},
    {.ready=&ready[1], .mutex=&handles[1], .root="1:/"},
    {.ready=&ready[2], .mutex=&handles[2], .root="2:/"}
};
static const fs_device_info_t devices[] = {
    {"flash", "flash", &contexts[0]}, {"sd", "sd", &contexts[1]}, {"sd-pl", "sd-pl", &contexts[2]}
};
void *pvPortMalloc(size_t n) { void *p = malloc(n); if (p) ++allocated; return p; }
void vPortFree(void *p) { assert(p && allocated); --allocated; free(p); }
int xSemaphoreTake(SemaphoreHandle_t s, unsigned ticks) { (void)ticks; return !pthread_mutex_lock(s); }
int xSemaphoreGive(SemaphoreHandle_t s) { return !pthread_mutex_unlock(s); }
int fs_device_count(void) { return 3; }
const fs_device_info_t *fs_device_at(int i) { return i >= 0 && i < 3 ? &devices[i] : NULL; }
const fs_device_info_t *fs_device_by_name(const char *name)
{
    for (unsigned i = 0; i < 3; ++i) if (!strcmp(name, devices[i].name)) return &devices[i];
    return NULL;
}
static void native_path(const char *path, char out[1024])
{
    assert(path[0] >= '0' && path[0] <= '2' && path[1] == ':' && path[2] == '/');
    assert(!strstr(path, "/../") && !strstr(path, "/./"));
    assert(snprintf(out, 1024, "%s/%c%s", root, path[0], path + 2) < 1024);
}
static FRESULT failure(void)
{
    return errno == ENOENT ? FR_NO_FILE : errno == EEXIST ? FR_EXIST :
        errno == ENOMEM ? FR_NOT_ENOUGH_CORE : FR_DENIED;
}
FRESULT f_open(FIL *f, const char *path, BYTE flags)
{
    char p[1024]; struct stat info;
    native_path(path, p);
    bool exists = !stat(p, &info);
    if (exists && S_ISDIR(info.st_mode)) return FR_DENIED;
    if ((flags & FA_CREATE_NEW) && exists) return FR_EXIST;
    const char *mode = flags & FA_WRITE ?
        ((flags & (FA_CREATE_ALWAYS | FA_CREATE_NEW)) || !exists ? "w+b" : "r+b") : "rb";
    f->native = fopen(p, mode);
    if (!f->native) return failure();
    f->size = exists && !(flags & FA_CREATE_ALWAYS) ? (FSIZE_t)info.st_size : 0;
    ++opened;
    return FR_OK;
}
FRESULT f_read(FIL *f, void *p, UINT n, UINT *count)
{
    *count = (UINT)fread(p, 1, n, f->native);
    return ferror(f->native) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_write(FIL *f, const void *p, UINT n, UINT *count)
{
    *count = (UINT)fwrite(p, 1, n, f->native);
    f->size = (FSIZE_t)ftell(f->native);
    return *count == n ? FR_OK : FR_DISK_ERR;
}
FRESULT f_close(FIL *f)
{
    int rc = fclose(f->native);
    f->native = NULL; --opened;
    if (fail_close) { fail_close = 0; return FR_DISK_ERR; }
    return rc ? FR_DISK_ERR : FR_OK;
}
FRESULT f_sync(FIL *f) { return fail_sync || fflush(f->native) ? FR_DISK_ERR : FR_OK; }
FRESULT f_truncate(FIL *f) { f->size = 0; return ftruncate(fileno(f->native), 0) ? FR_DISK_ERR : FR_OK; }
FRESULT f_lseek(FIL *f, FSIZE_t offset) { return fseek(f->native, (long)offset, SEEK_SET) ? FR_DISK_ERR : FR_OK; }
FRESULT f_stat(const char *path, FILINFO *info)
{
    char p[1024]; struct stat s;
    native_path(path, p);
    if (stat(p, &s)) return failure();
    memset(info, 0, sizeof(*info));
    info->fsize = (FSIZE_t)s.st_size;
    info->fattrib = S_ISDIR(s.st_mode) ? AM_DIR : 0;
    const char *name = strrchr(path, '/') + 1;
    if (strlen(name) >= sizeof(info->fname)) return FR_INVALID_NAME;
    strcpy(info->fname, name);
    return FR_OK;
}
FRESULT f_opendir(DIR *dir, const char *path)
{
    char p[1024]; native_path(path, p);
    if (strlen(p) >= sizeof(dir->path)) return FR_INVALID_NAME;
    strcpy(dir->path, p);
    dir->native = opendir(p);
    return dir->native ? FR_OK : failure();
}
FRESULT f_readdir(DIR *dir, FILINFO *info)
{
    errno = 0;
    struct dirent *entry = readdir(dir->native);
    memset(info, 0, sizeof(*info));
    if (!entry) return errno ? FR_DISK_ERR : FR_OK;
    char p[1024]; struct stat s;
    if (snprintf(p, sizeof(p), "%s/%s", dir->path, entry->d_name) >= (int)sizeof(p)) return FR_INVALID_NAME;
    if (stat(p, &s)) return failure();
    strcpy(info->fname, entry->d_name);
    info->fattrib = S_ISDIR(s.st_mode) ? AM_DIR : 0;
    info->fsize = (FSIZE_t)s.st_size;
    return FR_OK;
}
FRESULT f_closedir(DIR *dir) { return closedir(dir->native) ? FR_DISK_ERR : FR_OK; }
FRESULT f_mkdir(const char *path) { char p[1024]; native_path(path, p); return mkdir(p, 0700) ? failure() : FR_OK; }
FRESULT f_unlink(const char *path) { char p[1024]; native_path(path, p); return remove(p) ? failure() : FR_OK; }
FRESULT f_rename(const char *from, const char *to)
{
    char a[1024], b[1024]; struct stat info;
    native_path(from, a); native_path(to, b);
    if (fail_install && strstr(from, ".tmp")) { fail_install = 0; return FR_DISK_ERR; }
    if (!stat(b, &info)) return FR_EXIST;
    return rename(a, b) ? failure() : FR_OK;
}

int main(int argc, char **argv)
{
    assert(argc == 2 && strlen(argv[1]) < sizeof(root));
    strcpy(root, argv[1]);
    assert(!mkdir(root, 0700));
    for (unsigned i = 0; i < 3; ++i) {
        char p[1024]; assert(snprintf(p, sizeof(p), "%s/%u", root, i) < (int)sizeof(p));
        assert(!mkdir(p, 0700));
    }
    const bvstk_fs_storage_t *s = fs_storage_backend();
    fs_storage_contract(s);
    ready[1] = 0;
    bvstk_fs_writer_t w;
    assert(bvstk_fs_writer_open(&w, s, "sd:/not-mounted", false) == BVSTK_FS_NOT_READY);
    ready[1] = 1;
    assert(bvstk_fs_save(s, "flash:/recover.json", "original", 8) == BVSTK_FS_OK);
    fail_install = 1;
    assert(bvstk_fs_save(s, "flash:/recover.json", "replacement", 11) == BVSTK_FS_IO);
    fs_expect(s->reader, "flash:/recover.json", "original", 8);
    fail_sync = 1;
    assert(bvstk_fs_save(s, "flash:/recover.json", "bad", 3) == BVSTK_FS_IO);
    fail_sync = 0;
    fs_expect(s->reader, "flash:/recover.json", "original", 8);
    fail_close = 1;
    assert(bvstk_fs_save(s, "flash:/recover.json", "bad", 3) == BVSTK_FS_IO);
    fs_expect(s->reader, "flash:/recover.json", "original", 8);
    assert(bvstk_fs_remove(s, "flash:/recover.json") == BVSTK_FS_OK);
    assert(!opened && !allocated);
    for (unsigned i = 0; i < 3; ++i) {
        char p[1024]; assert(snprintf(p, sizeof(p), "%s/%u", root, i) < (int)sizeof(p));
        assert(!rmdir(p));
    }
    assert(!rmdir(root));
    puts("FatFs adapter: common filesystem lifecycle, real volume locks, failed sync/close and install rollback passed");
    return 0;
}
