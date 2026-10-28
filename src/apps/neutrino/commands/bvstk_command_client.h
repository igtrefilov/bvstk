#ifndef BVSTK_NEUTRINO_COMMAND_CLIENT_H
#define BVSTK_NEUTRINO_COMMAND_CLIENT_H
#include <stdio.h>
#include "shared/cli/bvstk_mdio_completion.h"
int bvstk_command_client_execute(int argc, char *const argv[], FILE *, FILE *);
int bvstk_command_client_mdio_devices(bvstk_mdio_completion_device_t *, size_t, size_t *);
#endif
