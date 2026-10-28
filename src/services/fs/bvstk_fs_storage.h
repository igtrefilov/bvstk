#ifndef BVSTK_FS_STORAGE_H
#define BVSTK_FS_STORAGE_H

#include "services/fs/bvstk_fs_reader.h"

/* The read-only backend remains usable by DCP2 FS. Mutations are a separate
 * capability; none of these operations mounts or formats a volume. */
typedef struct {
    const bvstk_fs_backend_t *reader;
    bvstk_fs_result_t (*create)(void *, const char *, bool overwrite, void **);
    bvstk_fs_result_t (*write)(void *, void *, const uint8_t *, size_t, size_t *);
    /* Always releases the handle, including when synchronization fails. */
    bvstk_fs_result_t (*finish)(void *, void *, bool sync);
    bvstk_fs_result_t (*mkdir)(void *, const char *);
    bvstk_fs_result_t (*remove)(void *, const char *);
    /* Destination must not exist; renames stay within a logical volume. */
    bvstk_fs_result_t (*rename)(void *, const char *, const char *);
    /* Install a closed, synced temporary file. Preserve the previous file at
     * backup, and restore it if installation fails. POSIX can replace in one
     * rename; FatFs uses a recoverable backup/rename sequence. */
    bvstk_fs_result_t (*install)(void *, const char *, const char *, const char *);
    /* Optional append-open; creates a missing file, never truncates it. */
    bvstk_fs_result_t (*append)(void *, const char *, void **);
} bvstk_fs_storage_t;

typedef struct {
    const bvstk_fs_storage_t *storage;
    void *handle;
    uint64_t bytes;
    bvstk_fs_result_t result;
} bvstk_fs_writer_t;

bvstk_fs_result_t bvstk_fs_path(const char *volume, const char *relative,
                               char *out, size_t capacity);
bvstk_fs_result_t bvstk_fs_stat(const bvstk_fs_backend_t *, const char *, bvstk_fs_info_t *);
bvstk_fs_result_t bvstk_fs_mkdir(const bvstk_fs_storage_t *, const char *);
bvstk_fs_result_t bvstk_fs_mkdirs(const bvstk_fs_storage_t *, const char *);
bvstk_fs_result_t bvstk_fs_remove(const bvstk_fs_storage_t *, const char *);
bvstk_fs_result_t bvstk_fs_rename(const bvstk_fs_storage_t *, const char *, const char *);

/* Writes follow the existing HTTP contract: create-new by default; an explicit
 * overwrite truncates; partial files survive errors and aborted transfers.
 * Success requires a successful final sync AND close. The portable write
 * limit is UINT32_MAX, matching the deployed FatFs/HTTP upload contract. */
bvstk_fs_result_t bvstk_fs_writer_open(bvstk_fs_writer_t *,
    const bvstk_fs_storage_t *, const char *, bool overwrite);
bvstk_fs_result_t bvstk_fs_writer_append(bvstk_fs_writer_t *,
    const bvstk_fs_storage_t *, const char *);
bvstk_fs_result_t bvstk_fs_writer_write(bvstk_fs_writer_t *, const void *, size_t);
bvstk_fs_result_t bvstk_fs_writer_close(bvstk_fs_writer_t *);
bvstk_fs_result_t bvstk_fs_writer_abort(bvstk_fs_writer_t *);
bvstk_fs_result_t bvstk_fs_copy(const bvstk_fs_storage_t *, const char *, const char *, uint64_t *);
/* Explicit profile save: stage, sync, close, retain .bak, then install. A
 * failed write never truncates the destination. There is no automatic retry. */
bvstk_fs_result_t bvstk_fs_save(const bvstk_fs_storage_t *, const char *, const void *, size_t);
const char *bvstk_fs_result_name(bvstk_fs_result_t);

#endif
