#ifndef BVSTK_TCP_MEM_SHELL_H
#define BVSTK_TCP_MEM_SHELL_H

#include <stdbool.h>
#include "console_common.h"

bool mem_handle(char *tok, char **save, int fd);
void mem_help(int fd);

#endif
