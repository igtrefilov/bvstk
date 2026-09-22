#ifndef BVSTK_TCP_MDIO_SHELL_H
#define BVSTK_TCP_MDIO_SHELL_H

#include <stdbool.h>
#include "apps/freertos/console/console_common.h"
#include "shared/cli/bvstk_mdio_completion.h"

bool mdio_handle(char *tok, char **save, int fd);
void mdio_help(int fd);
int mdio_complete_line(const char *line, size_t length, size_t cursor,
                      bvstk_mdio_completion_result_t *result);

#endif
