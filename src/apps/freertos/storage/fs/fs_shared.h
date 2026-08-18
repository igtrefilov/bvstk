#ifndef FS_SHARED_H
#define FS_SHARED_H

#include <stdbool.h>
#include "ports/freertos-xilinx/fs-fatfs/xilffs_config.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FS_PATH_MAX 128
#define FS_NAME_MAX 64

typedef struct {
    FATFS *fatfs;
    volatile int *ready;
    SemaphoreHandle_t *mutex;
    const char *root;
} fs_shared_ctx_t;

typedef struct {
    FRESULT unmount;
    FRESULT mkfs;
    FRESULT mount;
} fs_shared_format_diag_t;

int fs_shared_mount(fs_shared_ctx_t *ctx, const char *label);
FRESULT fs_shared_format(fs_shared_ctx_t *ctx);
FRESULT fs_shared_format_ex(fs_shared_ctx_t *ctx,
                            fs_shared_format_diag_t *diag);
int fs_shared_is_ready(const fs_shared_ctx_t *ctx);

int fs_shared_fs_ls(const fs_shared_ctx_t *ctx, const char *path, int fd);
int fs_shared_fs_cat(const fs_shared_ctx_t *ctx, const char *path, int fd);
FRESULT fs_shared_fs_touch(const fs_shared_ctx_t *ctx, const char *path);
FRESULT fs_shared_fs_mkdir(const fs_shared_ctx_t *ctx, const char *path);
FRESULT fs_shared_fs_rm(const fs_shared_ctx_t *ctx, const char *path);
FRESULT fs_shared_fs_rm_recursive(const fs_shared_ctx_t *ctx, const char *path);
int fs_shared_fs_is_dir(const fs_shared_ctx_t *ctx, const char *path);
int fs_shared_fs_complete(const fs_shared_ctx_t *ctx, const char *dir, const char *prefix,
                          char results[][FS_NAME_MAX], int max_results, int *out_count);
int fs_shared_fs_cp(const fs_shared_ctx_t *ctx, const char *src, const char *dst, bool recursive);
int fs_shared_fs_cp_between(const fs_shared_ctx_t *src_ctx, const fs_shared_ctx_t *dst_ctx,
                            const char *src, const char *dst, bool recursive);
int fs_shared_fs_mv(const fs_shared_ctx_t *ctx, const char *src, const char *dst);
int fs_shared_fs_mv_between(const fs_shared_ctx_t *src_ctx, const fs_shared_ctx_t *dst_ctx,
                            const char *src, const char *dst, bool recursive);

/* Locked file-operation primitives used by binary transports such as SCP. */
FRESULT fs_shared_file_stat(const fs_shared_ctx_t *ctx, const char *path, FILINFO *info);
FRESULT fs_shared_file_open_read(const fs_shared_ctx_t *ctx, const char *path,
                                 FIL *file, uint32_t *size);
FRESULT fs_shared_file_open_write(const fs_shared_ctx_t *ctx, const char *path,
                                  FIL *file);
FRESULT fs_shared_file_read(const fs_shared_ctx_t *ctx, FIL *file,
                            void *buf, uint32_t capacity, uint32_t *read_bytes);
FRESULT fs_shared_file_write(const fs_shared_ctx_t *ctx, FIL *file,
                             const void *buf, uint32_t size, uint32_t *written_bytes);
FRESULT fs_shared_file_close(const fs_shared_ctx_t *ctx, FIL *file);

/* Locked directory-operation primitives used by recursive binary transports. */
FRESULT fs_shared_dir_open(const fs_shared_ctx_t *ctx, const char *path, DIR *dir);
FRESULT fs_shared_dir_read(const fs_shared_ctx_t *ctx, DIR *dir, FILINFO *info);
FRESULT fs_shared_dir_close(const fs_shared_ctx_t *ctx, DIR *dir);

#ifdef __cplusplus
}
#endif

#endif // FS_SHARED_H
