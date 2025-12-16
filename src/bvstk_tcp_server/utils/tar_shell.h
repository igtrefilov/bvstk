#ifndef TAR_SHELL_H
#define TAR_SHELL_H

#include <stdbool.h>

#include "console_common.h"

bool tar_handle(char *tok, char **save, int fd, console_session_t *session);
void tar_help(int fd);

#endif /* TAR_SHELL_H */

