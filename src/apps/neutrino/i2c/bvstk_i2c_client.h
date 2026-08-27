#ifndef BVSTK_NEUTRINO_I2C_CLIENT_H
#define BVSTK_NEUTRINO_I2C_CLIENT_H

#include <stdio.h>

int bvstk_i2c_client_execute(int argument_count,
                             char *const arguments[],
                             FILE *output,
                             FILE *error_output);

#endif /* BVSTK_NEUTRINO_I2C_CLIENT_H */
