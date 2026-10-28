/* Run under Neutrino against the actual bvstkd resource manager.
 * Both paths must refer to a new file in a task-owned temporary directory. */
#define _POSIX_C_SOURCE 200809L
#include "apps/neutrino/commands/bvstk_command_ipc.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void start(bvstk_command_ipc_t *m, const char *command)
{
    memset(m, 0, sizeof(*m)); m->version = BVSTK_COMMAND_VERSION;
    assert(strlen(command) < sizeof(m->command)); strcpy(m->command, command);
    strcpy(m->cwd, "flash:/");
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    uint8_t expected[10017], received[10017];
    for (size_t i = 0; i < sizeof(expected); ++i) expected[i] = (uint8_t)(i * 17U);
    int file = open(argv[1], O_CREAT | O_EXCL | O_WRONLY, 0600); assert(file >= 0);
    assert(write(file, expected, sizeof(expected)) == sizeof(expected)); assert(!close(file));
    int a = open(BVSTK_COMMAND_PATH, O_RDWR), b = open(BVSTK_COMMAND_PATH, O_RDWR);
    assert(a >= 0 && b >= 0);
    bvstk_command_ipc_t *m = calloc(1, sizeof(*m)), *other = calloc(1, sizeof(*other)); assert(m && other);
    m->version = 1; m->operation = BVSTK_COMMAND_READ;
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EINVAL);
    char command[2048]; assert(snprintf(command, sizeof(command), "fs cat \"%s\"", argv[2]) < (int)sizeof(command));
    start(m, command);
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EOK);
    assert(!m->result && m->total == sizeof(expected) && m->length == BVSTK_COMMAND_CHUNK);
    uint32_t transfer = m->transfer;
    memcpy(received, m->data, m->length);
    /* A different open descriptor must not replace a live result. */
    start(other, "help"); assert(devctl(b, BVSTK_DCMD_COMMAND, other, sizeof(*other), NULL) == EOK);
    assert(!other->result && other->total && !memcmp(other->data, "BVSTK", 5));
    m->operation = BVSTK_COMMAND_READ; m->offset = 0;
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EOK);
    assert(!memcmp(m->data, expected, m->length));
    uint32_t offset = m->length;
    while (offset < sizeof(received)) {
        m->operation = BVSTK_COMMAND_READ; m->offset = offset;
        assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EOK);
        assert(m->transfer == transfer && m->total == sizeof(expected) && m->length && !m->result);
        assert(m->offset == offset && m->length <= sizeof(received) - offset);
        memcpy(received + offset, m->data, m->length); offset += m->length;
    }
    assert(!memcmp(received, expected, sizeof(expected)));
    m->offset = m->total + 1U;
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EINVAL);
    m->offset = 0; ++m->transfer;
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EINVAL);
    /* Retrying READ retrieves the prior result without repeating the append. */
    assert(snprintf(command, sizeof(command), "fs append \"%s\" X", argv[2]) < (int)sizeof(command));
    start(m, command); assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EOK);
    assert(!m->result);
    m->operation = BVSTK_COMMAND_READ; m->offset = 0;
    for (unsigned n = 0; n < 3; ++n) assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EOK);
    struct stat info; assert(!stat(argv[1], &info) && info.st_size == (off_t)(sizeof(expected) + 1U));
    start(m, "help"); m->version = 2;
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EINVAL);
    m->version = 1; memset(m->command, 'x', sizeof(m->command));
    assert(devctl(a, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL) == EINVAL);
    assert(!close(a) && !close(b)); free(m); free(other);
    assert(!unlink(argv[1]));
    puts("Command IPC: binary paging/retries, per-open isolation, single execution, malformed requests and close passed");
    return 0;
}
