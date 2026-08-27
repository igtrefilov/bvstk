#include <stdio.h>

#include "apps/neutrino/i2c/bvstk_i2c_client.h"

int main(int argc, char **argv)
{
    return bvstk_i2c_client_execute(argc - 1,
                                    argv + 1,
                                    stdout,
                                    stderr);
}
