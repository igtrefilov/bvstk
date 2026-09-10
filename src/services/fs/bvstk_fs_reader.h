#ifndef BVSTK_FS_READER_H
#define BVSTK_FS_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BVSTK_FS_PATH_CAPACITY 512U
#define BVSTK_FS_MAX_DEPTH 16U

typedef enum {
    BVSTK_FS_OK = 0,
    BVSTK_FS_INVALID,
    BVSTK_FS_RANGE,
    BVSTK_FS_NOT_FOUND,
    BVSTK_FS_NOT_READY,
    BVSTK_FS_TYPE_ERROR,
    BVSTK_FS_IO,
    BVSTK_FS_CHANGED,
    BVSTK_FS_BUSY,
    BVSTK_FS_UNSUPPORTED
} bvstk_fs_result_t;

enum {
    BVSTK_FS_FILE = 1,
    BVSTK_FS_TREE = 2,
    BVSTK_FS_TAR = 3,
    BVSTK_FS_RECURSIVE = 1,
    BVSTK_FS_ENTRY_FILE = 1,
    BVSTK_FS_ENTRY_DIR = 2
};

typedef struct {
    uint64_t size;
    uint32_t stamp; /* Backend change hint; does not guarantee a snapshot. */
    uint8_t type;
    uint8_t attributes; /* bit0 readonly, bit1 hidden, bit2 system, bit3 archive */
} bvstk_fs_info_t;

typedef struct {
    char name[16];
    uint8_t state; /* 0 not ready, 1 ready, 2 unsupported by this image */
} bvstk_fs_volume_t;

/* Paths and entry names at this boundary are UTF-8. Handles belong to backend. */
typedef struct {
    void *context;
    size_t (*volume_count)(void *context);
    bvstk_fs_result_t (*volume)(void *context, size_t index, bvstk_fs_volume_t *out);
    bvstk_fs_result_t (*stat)(void *context, const char *path, bvstk_fs_info_t *out);
    bvstk_fs_result_t (*file_open)(void *context, const char *path, void **file, uint64_t *size);
    bvstk_fs_result_t (*file_read)(void *context, void *file, uint8_t *data, size_t capacity, size_t *size);
    void (*file_close)(void *context, void *file);
    bvstk_fs_result_t (*dir_open)(void *context, const char *path, void **dir);
    bvstk_fs_result_t (*dir_read)(void *context, void *dir, char *name, size_t capacity, bvstk_fs_info_t *info);
    void (*dir_close)(void *context, void *dir);
} bvstk_fs_backend_t;

typedef struct {
    const bvstk_fs_backend_t *backend;
    char root[BVSTK_FS_PATH_CAPACITY];
    char path[BVSTK_FS_PATH_CAPACITY];
    struct {
        void *handle;
        size_t path_len;
    } dirs[BVSTK_FS_MAX_DEPTH];
    size_t depth;
    size_t relative_offset;
    void *file;
    bvstk_fs_info_t info;
    uint64_t remaining;
    uint64_t total_size;
    size_t padding;
    uint8_t stage[2048]; /* PAX header, padded metadata and regular header. */
    size_t stage_size;
    size_t stage_offset;
    uint8_t kind;
    uint8_t flags;
    uint8_t phase;
} bvstk_fs_reader_t;

bool bvstk_fs_utf8_decode(const uint8_t *data, size_t size, size_t *used, uint32_t *codepoint);
bvstk_fs_result_t bvstk_fs_validate_path(const uint8_t *data, size_t size, char *out, size_t capacity);
bvstk_fs_result_t bvstk_fs_reader_open(bvstk_fs_reader_t *reader, const bvstk_fs_backend_t *backend,
                                      const char *path, uint8_t kind, uint8_t flags);
bvstk_fs_result_t bvstk_fs_reader_read(bvstk_fs_reader_t *reader, uint8_t *data, size_t capacity,
                                      size_t *size, bool *eof);
void bvstk_fs_reader_close(bvstk_fs_reader_t *reader);

#endif
