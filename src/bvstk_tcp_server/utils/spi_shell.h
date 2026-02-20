#ifndef BVSTK_TCP_SPI_SHELL_H
#define BVSTK_TCP_SPI_SHELL_H

#include <stdbool.h>
#include "console_common.h"

bool spi_handle(char *tok, char **save, int fd);
void spi_help(int fd);

#endif
