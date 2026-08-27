#ifndef BVSTK_NEUTRINO_I2C_CLIENT_H
#define BVSTK_NEUTRINO_I2C_CLIENT_H

#include <stdio.h>

#include "shared/cli/bvstk_i2c_completion.h"

int bvstk_i2c_client_execute(int argument_count,
                             char *const arguments[],
                             FILE *output,
                             FILE *error_output);

/* Return the currently configured devices without printing the list. */
int bvstk_i2c_client_list_devices(
    bvstk_i2c_completion_device_t *devices,
    size_t device_capacity,
    size_t *out_device_count);

#endif /* BVSTK_NEUTRINO_I2C_CLIENT_H */
