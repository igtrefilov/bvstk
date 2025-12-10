#ifndef BVSTK_TCP_AXP_SHELL_H
#define BVSTK_TCP_AXP_SHELL_H

#include <stdbool.h>
#include "console_common.h"

bool axp_handle(char *tok, char **save, int fd);
void axp_help(int fd);

#endif
