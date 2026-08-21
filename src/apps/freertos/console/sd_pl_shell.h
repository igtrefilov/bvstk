#ifndef BVSTK_SD_PL_SHELL_H
#define BVSTK_SD_PL_SHELL_H

#include <stdbool.h>

#include "apps/freertos/console/console_common.h"

bool sd_pl_handle(char *tok, char **save, int fd);
void sd_pl_help(int fd);

#endif /* BVSTK_SD_PL_SHELL_H */
