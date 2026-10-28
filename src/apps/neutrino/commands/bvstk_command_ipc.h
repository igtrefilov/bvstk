#ifndef BVSTK_NEUTRINO_COMMAND_IPC_H
#define BVSTK_NEUTRINO_COMMAND_IPC_H
#include <stdint.h>
#include <devctl.h>
#define BVSTK_COMMAND_PATH "/dev/bvstk-control"
#define BVSTK_COMMAND_VERSION 1U
#define BVSTK_COMMAND_START 0U
#define BVSTK_COMMAND_READ 1U
#define BVSTK_COMMAND_CHUNK 4096U
/* Below the devctl encoded message-size limit. Large outputs are paged on the
 * same open descriptor, and never execute the command again during a READ. */
typedef struct {
    uint32_t version, operation, transfer, offset, total, length;
    int32_t result;
    char command[2048];
    char cwd[512];
    uint8_t data[BVSTK_COMMAND_CHUNK];
} bvstk_command_ipc_t;
#define BVSTK_DCMD_COMMAND __DIOTF(0x80, 3, bvstk_command_ipc_t)
#endif
