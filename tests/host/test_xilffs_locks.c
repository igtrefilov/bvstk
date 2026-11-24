/* Built by run_xilffs.sh with the actual patched xilffs R0.13b. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "apps/freertos/storage/fs/fs_dcp2_backend.h"
#include "apps/freertos/storage/fs/fs_devices.h"
#include "ports/freertos-xilinx/fs-fatfs/bvstk_fatfs.h"
#include "diskio.h"

_Static_assert(sizeof(DWORD) == 4 && sizeof(LONG) == 4, "FAT on-disk types");
_Static_assert(FF_FS_LOCK == 64 && FF_FS_REENTRANT, "target locking configuration");

enum { VOL_COUNT = 2, SECTORS = 8192, SECTOR_SIZE = 512 };
static BYTE images[VOL_COUNT][SECTORS][SECTOR_SIZE];
static FATFS volumes[VOL_COUNT];
static int fail_writes;
static pthread_mutex_t scheduler = PTHREAD_MUTEX_INITIALIZER;

void vTaskSuspendAll(void) { assert(!pthread_mutex_lock(&scheduler)); }
int xTaskResumeAll(void) { return pthread_mutex_unlock(&scheduler); }
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    pthread_mutexattr_t attr;
    SemaphoreHandle_t mutex = malloc(sizeof(*mutex));
    assert(mutex && !pthread_mutexattr_init(&attr));
    assert(!pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
    assert(!pthread_mutex_init(mutex, &attr) && !pthread_mutexattr_destroy(&attr));
    return mutex;
}
int xSemaphoreTakeRecursive(SemaphoreHandle_t mutex, unsigned ticks)
{ (void)ticks; return !pthread_mutex_lock(mutex); }
int xSemaphoreGiveRecursive(SemaphoreHandle_t mutex) { return !pthread_mutex_unlock(mutex); }

DSTATUS disk_initialize(BYTE drive) { return drive < VOL_COUNT ? 0 : STA_NOINIT; }
DSTATUS disk_status(BYTE drive) { return disk_initialize(drive); }
DRESULT disk_read(BYTE drive, BYTE *to, DWORD sector, UINT count)
{
    assert(drive < VOL_COUNT && count <= SECTORS && sector <= SECTORS - count);
    memcpy(to, images[drive][sector], SECTOR_SIZE * count);
    return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *from, DWORD sector, UINT count)
{
    assert(drive < VOL_COUNT && count <= SECTORS && sector <= SECTORS - count);
    if (fail_writes) return RES_ERROR;
    memcpy(images[drive][sector], from, SECTOR_SIZE * count);
    return RES_OK;
}
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer)
{
    assert(drive < VOL_COUNT);
    if (cmd == GET_BLOCK_SIZE) *(DWORD *)buffer = 1;
    else if (cmd == GET_SECTOR_COUNT) *(DWORD *)buffer = SECTORS;
    else if (cmd != CTRL_SYNC) return RES_PARERR;
    return RES_OK;
}
DWORD get_fattime(void) { return 0x58210000; }

static int ready = 1;
static SemaphoreHandle_t gate;
static fs_shared_ctx_t ctx = {.ready = &ready, .root = "0:/", .mutex = &gate};
static const fs_device_info_t device = {"sd", "test", &ctx};
int fs_device_count(void) { return 1; }
const fs_device_info_t *fs_device_at(int i) { return i ? NULL : &device; }
const fs_device_info_t *fs_device_by_name(const char *name)
{ return strcmp(name, "sd") ? NULL : &device; }

static void check_contents(const char *path, const char *expected)
{
    FIL file;
    char buffer[16];
    UINT count;
    assert(f_open(&file, path, FA_READ) == FR_OK);
    assert(f_read(&file, buffer, sizeof(buffer), &count) == FR_OK);
    assert(count == strlen(expected) && !memcmp(buffer, expected, count));
    assert(f_close(&file) == FR_OK);
}

static void *mount_and_write(void *arg)
{
    int index = *(int *)arg;
    char root[] = "0:", path[] = "0:/report.txt", renamed[] = "0:/renamed.txt";
    root[0] = path[0] = renamed[0] = (char)('0' + index);
    for (unsigned i = 0; i < 300; ++i) {
        FIL file, other;
        UINT count;
        assert(bvstk_fatfs_mount(&volumes[index], root, 1) == FR_OK);
        assert(f_open(&file, path, FA_CREATE_NEW | FA_WRITE) == FR_OK);
        assert(f_unlink(path) == FR_LOCKED && f_rename(path, renamed) == FR_LOCKED);
        assert(f_write(&file, "stable", 6, &count) == FR_OK && count == 6);
        assert(f_open(&other, path, FA_WRITE) == FR_LOCKED);
        assert(f_close(&file) == FR_OK);
        assert(f_open(&file, path, FA_READ) == FR_OK);
        check_contents(path, "stable");
        assert(f_unlink(path) == FR_LOCKED && f_close(&file) == FR_OK);
        assert(f_rename(path, renamed) == FR_OK && f_unlink(renamed) == FR_OK);
        assert(bvstk_fatfs_mount(NULL, root, 0) == FR_OK);
    }
    return NULL;
}

static void close_after_failed_sync(void)
{
    for (unsigned i = 0; i < 2 * FF_FS_LOCK; ++i) {
        FIL file;
        UINT count;
        assert(f_open(&file, "0:/bad", FA_CREATE_NEW | FA_WRITE) == FR_OK);
        assert(f_write(&file, "X", 1, &count) == FR_OK && count == 1);
        fail_writes = 1;
        assert(f_close(&file) == FR_DISK_ERR);
        fail_writes = 0;
        assert(f_unlink("0:/bad") == FR_OK);
    }
}

static void storage_conflicts(void)
{
    const bvstk_fs_storage_t *storage = fs_storage_backend();
    bvstk_fs_upload_t upload = {0};
    bvstk_fs_writer_t writer;
    FILINFO dirinfo;
    char alias[100];
    DIR dir;
    gate = xSemaphoreCreateRecursiveMutex();
    assert(bvstk_fs_mkdir(storage, "sd:/Long Directory Name") == BVSTK_FS_OK);
    assert(bvstk_fs_mkdir(storage, "sd:/Long Directory Name/child") == BVSTK_FS_OK);
    assert(f_stat("0:/Long Directory Name", &dirinfo) == FR_OK && dirinfo.altname[0]);
    snprintf(alias, sizeof(alias), "sd:/%s/child/loop", dirinfo.altname);
    assert(bvstk_fs_rename(storage, "sd:/Long Directory Name", alias) == BVSTK_FS_INVALID);
    assert(bvstk_fs_remove(storage, "sd:/Long Directory Name/child") == BVSTK_FS_OK);
    assert(f_opendir(&dir, "0:/Long Directory Name") == FR_OK);
    assert(bvstk_fs_rename(storage, "sd:/Long Directory Name", "sd:/renamed") == BVSTK_FS_BUSY);
    assert(bvstk_fs_remove(storage, "sd:/Long Directory Name") == BVSTK_FS_BUSY);
    assert(f_closedir(&dir) == FR_OK);
    assert(bvstk_fs_remove(storage, "sd:/Long Directory Name") == BVSTK_FS_OK);

    assert(bvstk_fs_writer_open(&writer, storage, "sd:/open", false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&writer, "one", 3) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_begin(&upload, storage, "sd:/open", 3, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&upload, "two", 3) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&upload) == BVSTK_FS_BUSY);
    assert(bvstk_fs_remove(storage, "sd:/open") == BVSTK_FS_BUSY);
    assert(bvstk_fs_rename(storage, "sd:/open", "sd:/moved") == BVSTK_FS_BUSY);
    assert(bvstk_fs_writer_close(&writer) == BVSTK_FS_OK);
    check_contents("0:/open", "one");
    assert(bvstk_fs_upload_begin(&upload, storage, "sd:/open", 3, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&upload, "two", 3) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&upload) == BVSTK_FS_OK);
    check_contents("0:/open", "two");
    assert(bvstk_fs_remove(storage, "sd:/open") == BVSTK_FS_OK);
}

int main(void)
{
    int indices[] = {0, 1};
    pthread_t workers[VOL_COUNT];
    BYTE scratch[SECTOR_SIZE];
    for (int index = 0; index < VOL_COUNT; ++index) {
        char root[] = "0:";
        root[0] = (char)('0' + index);
        assert(f_mkfs(root, FM_FAT | FM_SFD, 0, scratch, sizeof(scratch)) == FR_OK);
        assert(!pthread_create(&workers[index], NULL, mount_and_write, &indices[index]));
    }
    for (int index = 0; index < VOL_COUNT; ++index) assert(!pthread_join(workers[index], NULL));
    assert(bvstk_fatfs_mount(&volumes[0], "0:", 1) == FR_OK);
    storage_conflicts();
    close_after_failed_sync();
    assert(bvstk_fatfs_mount(NULL, "0:", 0) == FR_OK);
    puts("Native xilffs tests passed: two volumes, open-object conflicts, directory aliases, commit and close failures");
    return 0;
}
