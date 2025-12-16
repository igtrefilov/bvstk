#ifndef IP_SHELL_H
#define IP_SHELL_H

#include <stdbool.h>

#include "console_common.h"

bool ip_handle(char *tok, char **save, int fd, console_session_t *session);
void ip_help(int fd);

#endif /* IP_SHELL_H */

