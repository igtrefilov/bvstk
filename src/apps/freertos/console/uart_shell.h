#ifndef BVSTK_TCP_UART_SHELL_H
#define BVSTK_TCP_UART_SHELL_H

#include <stdbool.h>

#include "apps/freertos/console/console_common.h"

bool uart_handle(char *tok, char **save, int fd);
void uart_help(int fd);

#endif
