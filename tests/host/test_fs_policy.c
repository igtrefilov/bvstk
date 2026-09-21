#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/freertos/storage/fs/fs_shared.h"

static FRESULT mount_result, close_result, write_result;
static unsigned mounts, formats, opens, closes, truncates, seeks, writes;
static int locked, media_ok;
static bool short_write;

int xil_printf(const char *format, ...) { (void)format; return 0; }
int xSemaphoreTake(SemaphoreHandle_t sem, unsigned ticks)
{
    (void)ticks; assert(sem && !locked); locked = 1; return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t sem) { assert(sem && locked); locked = 0; return pdTRUE; }
static int media_ready(void) { return media_ok; }

FRESULT f_mount(FATFS *fs, const char *path, BYTE immediate)
{
    assert(locked && path);
    if (!fs) { assert(!immediate); return FR_OK; }
    ++mounts; return mount_result;
}
FRESULT f_mkfs(const char *path, BYTE mode, unsigned au, void *work, UINT size)
{
    (void)mode; (void)au; assert(locked && path && work && size);
    ++formats; return FR_OK;
}
FRESULT f_open(FIL *fp, const char *path, BYTE mode)
{
    assert(locked && path && (mode & FA_WRITE)); ++opens; fp->size = 123; return FR_OK;
}
FRESULT f_truncate(FIL *fp) { assert(locked && fp); ++truncates; return FR_OK; }
FRESULT f_lseek(FIL *fp, FSIZE_t size) { assert(locked && size == fp->size); ++seeks; return FR_OK; }
FRESULT f_write(FIL *fp, const void *buf, UINT n, UINT *written)
{
    assert(locked && fp && buf); ++writes; *written = short_write ? n - 1 : n;
    return write_result;
}
FRESULT f_close(FIL *fp) { assert(locked && fp); ++closes; return close_result; }

int main(void)
{
    FATFS fs;
    volatile int ready = 0;
    SemaphoreHandle_t mutex = &fs;
    fs_shared_ctx_t ctx = { .fatfs = &fs, .ready = &ready, .mutex = &mutex,
        .root = "2:/", .preserve_media = true, .media_ready = media_ready };
    const FRESULT failures[] = {FR_NO_FILESYSTEM, FR_DISK_ERR, FR_NOT_READY};
    unsigned i, before;
    media_ok = 1;
    for (i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        ctx.mount_attempted = false; mount_result = failures[i]; before = mounts;
        assert(fs_shared_mount(&ctx, "SD-PL") != 0);
        assert(formats == 0 && mounts == before + 1 && !ready && !locked);
        mount_result = FR_OK;
        assert(fs_shared_mount(&ctx, "SD-PL") != 0);
        assert(mounts == before + 1); /* No automatic remount after failure. */
        assert(fs_shared_format(&ctx) == FR_DENIED && formats == 0);
    }
    ctx.mount_attempted = false;
    assert(fs_shared_mount(&ctx, "SD-PL") == 0 && fs_shared_is_ready(&ctx));
    before = mounts;
    assert(fs_shared_mount(&ctx, "SD-PL") == 0 && mounts == before);
    assert(fs_shared_format(&ctx) == FR_DENIED && formats == 0);
    assert(fs_shared_fs_write_text(&ctx, "2:/file", "hello", false) == FR_OK);
    assert(opens == 1 && closes == 1 && truncates == 1 && !seeks && writes == 1);
    assert(fs_shared_fs_write_text(&ctx, "2:/file", "world", true) == FR_OK);
    assert(opens == 2 && closes == 2 && truncates == 1 && seeks == 1);
    short_write = true;
    assert(fs_shared_fs_write_text(&ctx, "2:/file", "abc", false) == FR_DENIED);
    short_write = false; close_result = FR_DISK_ERR;
    assert(fs_shared_fs_write_text(&ctx, "2:/file", "abc", false) == FR_DISK_ERR);
    assert(fs_shared_fs_touch(&ctx, "2:/file") == FR_DISK_ERR);
    close_result = FR_OK; write_result = FR_DISK_ERR;
    assert(fs_shared_fs_write_text(&ctx, "2:/file", "abc", false) == FR_DISK_ERR);
    assert(opens == closes && !locked);
    media_ok = 0; before = opens;
    assert(!fs_shared_is_ready(&ctx));
    assert(fs_shared_fs_write_text(&ctx, "2:/file", "abc", false) == FR_NOT_READY);
    assert(opens == before);
    assert(fs_shared_mount(&ctx, "SD-PL") != 0 && mounts > 0 && !locked && !formats);
    ctx.mount_attempted = false;
    mount_result = FR_OK;
    assert(fs_shared_mount(&ctx, "SD-PL") != 0 && !ready && !formats);
    puts("FS no-format/mount-once/write-close policy tests passed");
    return 0;
}
