#ifndef FS_SHARED_H
#define FS_SHARED_H

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

int fs_shared_mount(fs_shared_ctx_t *ctx, const char *label);
int fs_shared_is_ready(const fs_shared_ctx_t *ctx);

int fs_shared_fs_ls(const fs_shared_ctx_t *ctx, const char *path, int fd);
int fs_shared_fs_cat(const fs_shared_ctx_t *ctx, const char *path, int fd);
int fs_shared_fs_touch(const fs_shared_ctx_t *ctx, const char *path);
int fs_shared_fs_mkdir(const fs_shared_ctx_t *ctx, const char *path);
int fs_shared_fs_rm(const fs_shared_ctx_t *ctx, const char *path);
int fs_shared_fs_is_dir(const fs_shared_ctx_t *ctx, const char *path);
int fs_shared_fs_complete(const fs_shared_ctx_t *ctx, const char *dir, const char *prefix,
                          char results[][FS_NAME_MAX], int max_results, int *out_count);

#ifdef __cplusplus
}
#endif

#endif // FS_SHARED_H
