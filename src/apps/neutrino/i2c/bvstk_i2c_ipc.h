#ifndef BVSTK_NEUTRINO_I2C_IPC_H
#define BVSTK_NEUTRINO_I2C_IPC_H

#include <devctl.h>
#include <stdint.h>

#define BVSTK_I2C_DEVICE_PATH "/dev/bvstk-i2c"
#define BVSTK_I2C_IPC_VERSION UINT32_C(1)
#define BVSTK_I2C_IPC_COMMAND_MAX 512U
/* Keep the fixed devctl payload below the Neutrino message-size limit. */
#define BVSTK_I2C_IPC_RESPONSE_MAX 8192U

typedef struct {
    uint32_t version;
    int32_t result;
    uint32_t response_length;
    char command[BVSTK_I2C_IPC_COMMAND_MAX];
    char response[BVSTK_I2C_IPC_RESPONSE_MAX];
} bvstk_i2c_ipc_message_t;

/* 0x80 is a project-local devctl class used only by this resource manager. */
#define BVSTK_DCMD_I2C_EXECUTE \
    __DIOTF(0x80, 1, bvstk_i2c_ipc_message_t)

#endif /* BVSTK_NEUTRINO_I2C_IPC_H */
