#ifndef BVSTK_DCP2_FS_H
#define BVSTK_DCP2_FS_H

#include "protocols/dcp2/bvstk_dcp2_codec.h"
#include "services/fs/bvstk_fs_storage.h"

#define BVSTK_DCP2_FS_VERSION 1U
#define BVSTK_DCP2_FS_BLOCK_MAX 3072U
#define BVSTK_DCP2_FS_HANDLES_MAX 2U
#define BVSTK_DCP2_FS_IDLE_MS 30000U

enum {
    BVSTK_DCP2_FS_INFO = 0,
    BVSTK_DCP2_FS_STAT = 1,
    BVSTK_DCP2_FS_OPEN = 2,
    BVSTK_DCP2_FS_READ = 3,
    BVSTK_DCP2_FS_CLOSE = 4,
    BVSTK_DCP2_FS_WRITE_INFO = 5,
    BVSTK_DCP2_FS_MKDIR = 6,
    BVSTK_DCP2_FS_REMOVE = 7,
    BVSTK_DCP2_FS_RENAME = 8,
    BVSTK_DCP2_FS_WRITE_BEGIN = 9,
    BVSTK_DCP2_FS_WRITE_BLOCK = 10,
    BVSTK_DCP2_FS_WRITE_COMMIT = 11,
    BVSTK_DCP2_FS_WRITE_ABORT = 12,
    BVSTK_DCP2_FS_EOF = 1
};

enum {
    BVSTK_DCP2_FS_CAP_MKDIR = 1U << 6,
    BVSTK_DCP2_FS_CAP_REMOVE = 1U << 7,
    BVSTK_DCP2_FS_CAP_RENAME = 1U << 8,
    BVSTK_DCP2_FS_CAP_UPLOAD = 1U << 9,
    BVSTK_DCP2_FS_REPLACE = 1U
};

enum {
    BVSTK_DCP2_FS_NOT_FOUND = 0x0100,
    BVSTK_DCP2_FS_NOT_READY = 0x0101,
    BVSTK_DCP2_FS_BAD_HANDLE = 0x0102,
    BVSTK_DCP2_FS_TYPE_ERROR = 0x0103,
    BVSTK_DCP2_FS_IO_ERROR = 0x0104,
    BVSTK_DCP2_FS_CHANGED = 0x0105,
    BVSTK_DCP2_FS_EXISTS = 0x0106
};

typedef struct {
    bvstk_fs_reader_t reader;
    uint32_t handle;
    uint32_t open_id;
    uint32_t last_activity;
    uint32_t last_block;
    uint16_t requested_block_size;
    uint16_t block_size;
    uint16_t cached_size;
    uint16_t terminal_status;
    bool has_block;
    bool eof;
    uint8_t cached[BVSTK_DCP2_FS_BLOCK_MAX];
} bvstk_dcp2_fs_handle_t;

/* Keep exact request bytes, including malformed arguments. Only mutations
 * replace this entry. DCP sequence numbers are independent of operation IDs. */
typedef struct {
    uint32_t id;
    uint16_t status;
    uint16_t request_size;
    uint16_t reply_size;
    uint8_t opcode;
    uint8_t request[BVSTK_DCP2_MAX_PAYLOAD - BVSTK_DCP2_MIN_PAYLOAD];
    uint8_t reply[8]; /* WRITE_BLOCK returns the largest mutation response. */
} bvstk_dcp2_fs_replay_t;

typedef struct {
    const bvstk_fs_backend_t *backend;
    const bvstk_fs_storage_t *storage; /* Optional, with the same reader. */
    uint32_t next_handle;
    bvstk_dcp2_fs_handle_t handles[BVSTK_DCP2_FS_HANDLES_MAX];
    bvstk_fs_upload_t upload;
    uint32_t upload_handle;
    uint32_t upload_activity;
    bvstk_dcp2_fs_replay_t replay;
} bvstk_dcp2_fs_session_t;

void bvstk_dcp2_fs_init(bvstk_dcp2_fs_session_t *session, const bvstk_fs_backend_t *backend);
void bvstk_dcp2_fs_destroy(bvstk_dcp2_fs_session_t *session);
void bvstk_dcp2_fs_expire(bvstk_dcp2_fs_session_t *session, uint32_t now_ms);
/* Returns wire status; output contains only the operation body, never status. */
uint16_t bvstk_dcp2_fs_request(bvstk_dcp2_fs_session_t *session, uint8_t opcode,
                              const uint8_t *body, size_t size, uint32_t now_ms,
                              uint8_t *output, size_t capacity, uint16_t *output_size);

#endif
