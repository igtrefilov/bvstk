#ifndef BVSTK_FS_POSIX_H
#define BVSTK_FS_POSIX_H

#include <pthread.h>
#include "services/fs/bvstk_fs_storage.h"

#define BVSTK_FS_POSIX_VOLUMES_MAX 3U

typedef struct {
    const char *name;
    /* NULL declares a volume unsupported by this image. */
    const char *root;
    /* Mounted roots must have a different st_dev from their parent. This
     * prevents an empty /flash or /sd mountpoint being used as a RAM volume. */
    bool require_mount;
} bvstk_fs_posix_volume_t;

typedef struct {
    char name[16];
    char root[BVSTK_FS_PATH_CAPACITY];
    bool require_mount;
} bvstk_fs_posix_root_t;

typedef struct {
    bvstk_fs_backend_t reader;
    bvstk_fs_storage_t storage;
    bvstk_fs_posix_root_t volumes[BVSTK_FS_POSIX_VOLUMES_MAX];
    size_t volume_count, files, directories;
    pthread_mutex_t mutex;
    bool initialized;
} bvstk_fs_posix_t;

bvstk_fs_result_t bvstk_fs_posix_init(bvstk_fs_posix_t *,
    const bvstk_fs_posix_volume_t *, size_t);
/* All handles must be closed before destruction. */
bvstk_fs_result_t bvstk_fs_posix_destroy(bvstk_fs_posix_t *);
const bvstk_fs_storage_t *bvstk_fs_posix_storage(bvstk_fs_posix_t *);
/* AX7020 native mappings: sd:/ -> /sd, flash:/ -> /flash; sd-pl unsupported. */
const bvstk_fs_storage_t *bvstk_fs_posix_default(void);

#endif
