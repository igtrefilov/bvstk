#ifndef BVSTK_TCP_I2C_SHELL_H
#define BVSTK_TCP_I2C_SHELL_H

#include <stdbool.h>
#include "console_common.h"

bool i2c_handle(char *tok, char **save, int fd);
void i2c_help(int fd);

#endif

