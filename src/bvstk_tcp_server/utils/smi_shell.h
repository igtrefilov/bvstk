#ifndef BVSTK_TCP_SMI_SHELL_H
#define BVSTK_TCP_SMI_SHELL_H

#include <stdbool.h>
#include "console_common.h"

bool smi_handle(char *tok, char **save, int fd);
void smi_help(int fd);

#endif
