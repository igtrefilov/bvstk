#include "apps/neutrino/commands/bvstk_command_client.h"
#include "apps/neutrino/observe/bvstk_observe_client.h"
#include "shared/interfaces/bvstk_platform.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
int main(int argc, char **argv)
{
    const char *name = strrchr(argv[0], '/'); name = name ? name + 1 : argv[0];
    if (strcmp(name, "bvstkctl")) {
        argv[0] = (char *)name;
        return bvstk_command_client_execute(argc, argv, stdout, stderr);
    }
    if (argc >= 2 && !strcmp(argv[1], "observe"))
        return bvstk_observe_client_execute(argc - 2, argv + 2);
    if (argc == 2 && !strcmp(argv[1], "version")) {
        printf("bvstkctl 0.3.0\nplatform=%s\npl_contract=%" PRIu32 "\n",
            bvstk_platform_name(), BVSTK_PL_CONTRACT_VERSION);
        return 0;
    }
    if ((argc == 5 || argc == 6) && !strcmp(argv[1], "i2c") &&
        (!strcmp(argv[2], "read") || !strcmp(argv[2], "write"))) {
        char *temporary = argv[2]; argv[2] = argv[3]; argv[3] = temporary;
    }
    if (argc < 2) {
        char *help[] = {(char *)"help"};
        return bvstk_command_client_execute(1, help, stdout, stderr);
    }
    return bvstk_command_client_execute(argc - 1, argv + 1, stdout, stderr);
}
