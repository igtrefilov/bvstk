#ifndef BVSTK_TCP_FS_SHELL_H
#define BVSTK_TCP_FS_SHELL_H

#include <stdbool.h>
#include "apps/freertos/console/console_common.h"

bool fs_handle(char *tok, char **save, int fd, console_session_t *session);
void fs_help(int fd);

#endif
