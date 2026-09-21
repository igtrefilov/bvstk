#ifndef BVSTK_CONSOLE_COMPLETION_H
#define BVSTK_CONSOLE_COMPLETION_H

#include <stddef.h>

#include "apps/freertos/console/console_common.h"
#include "shared/cli/bvstk_line_editor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Complete a command line for any console transport sharing a session. */
int bvstk_console_complete(
    const console_session_t *session,
    const char *line,
    size_t line_length,
    size_t cursor,
    bvstk_line_editor_completion_t *result);

#ifdef __cplusplus
}
#endif

#endif
