#ifndef BVSTK_TAR_H
#define BVSTK_TAR_H

#include <stddef.h>

#include "../fs/fs_shared.h"

typedef int (*tar_read_cb)(void *user, void *buf, size_t len);
typedef int (*tar_write_cb)(void *user, const void *buf, size_t len);

/*
 * Minimal ustar tar streaming/extraction for FatFs trees.
 * - Supports regular files and directories.
 * - No compression; intended for streaming over sockets or console.
 * - Paths are treated as relative inside the tar stream.
 */

int tar_stream_dir_fatfs(const fs_shared_ctx_t *ctx, const char *dir_path, tar_write_cb write_cb, void *user);
int tar_extract_into_dir_fatfs(const fs_shared_ctx_t *ctx, const char *dst_dir_path,
                               tar_read_cb read_cb, void *user);
int tar_list(tar_read_cb read_cb, void *user, tar_write_cb write_cb, void *write_user);

#endif /* BVSTK_TAR_H */
