#ifndef BVSTK_TCP_CONSOLE_COMMON_H
#define BVSTK_TCP_CONSOLE_COMMON_H

#include <stdint.h>
#include <stdbool.h>

#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/storage/fs/fs_devices.h"
#include "apps/freertos/storage/fs/fs_shared.h"
#include "shared/base/bvstk_parse.h"

#define CONSOLE_CWD_LEN 64
typedef struct {
    char cwd[CONSOLE_CWD_LEN];
    const fs_shared_ctx_t *fs_ctx;
    const char *fs_label;
} console_session_t;

void console_session_init(console_session_t *s);
void console_print_banner(int fd);
void console_print_prompt(int fd, const console_session_t *s);

void write_str(int fd, const char *s);

const fs_shared_ctx_t *console_session_get_fs(const console_session_t *s);
const char *console_session_get_root(const console_session_t *s);
const char *console_session_get_label(const console_session_t *s);
void console_session_set_fs(console_session_t *s, const fs_shared_ctx_t *ctx, const char *label);

#endif
