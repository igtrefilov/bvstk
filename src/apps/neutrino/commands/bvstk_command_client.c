#include "apps/neutrino/commands/bvstk_command_client.h"
#include "apps/neutrino/commands/bvstk_command_ipc.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int command(bvstk_command_ipc_t *message, int argc, char *const argv[])
{
    size_t length = 0;
    for (int i = 0; i < argc; ++i) {
        if (!argv[i]) return 0;
        if (length + 3U >= sizeof(message->command)) return 0;
        if (i) message->command[length++] = ' ';
        message->command[length++] = '"';
        for (const char *p = argv[i]; *p; ++p) {
            if (length + 3U >= sizeof(message->command)) return 0;
            if (*p == '"' || *p == '\\') message->command[length++] = '\\';
            message->command[length++] = *p;
        }
        message->command[length++] = '"';
    }
    message->command[length] = '\0'; return 1;
}
int bvstk_command_client_execute(int argc, char *const argv[], FILE *output, FILE *errors)
{
    if (argc < 0 || (argc && !argv) || !output || !errors) return 2;
    bvstk_command_ipc_t *m = calloc(1, sizeof(*m));
    if (!m) return 1;
    m->version = BVSTK_COMMAND_VERSION;
    if (!command(m, argc, argv)) { fprintf(errors, "bvstk: command too long\n"); free(m); return 2; }
    char native[512];
    if (getcwd(native, sizeof(native))) {
        if (!strncmp(native, "/flash", 6U) && (!native[6] || native[6] == '/'))
            snprintf(m->cwd, sizeof(m->cwd), "flash:%s", native[6] ? native + 6U : "/");
        else if (!strncmp(native, "/sd", 3U) && (!native[3] || native[3] == '/'))
            snprintf(m->cwd, sizeof(m->cwd), "sd:%s", native[3] ? native + 3U : "/");
    }
    if (!*m->cwd) strcpy(m->cwd, "flash:/");
    int fd = open(BVSTK_COMMAND_PATH, O_RDWR);
    if (fd < 0) { fprintf(errors, "bvstk: %s: %s\n", BVSTK_COMMAND_PATH, strerror(errno)); free(m); return 1; }
    uint32_t offset = 0, transfer = 0, total = 0;
    int result = 1;
    for (;;) {
        int rc = devctl(fd, BVSTK_DCMD_COMMAND, m, sizeof(*m), NULL);
        if (rc != EOK) { fprintf(errors, "bvstk: command transport: %s\n", strerror(rc)); break; }
        if (m->version != BVSTK_COMMAND_VERSION || !m->transfer || m->offset != offset ||
            m->length > sizeof(m->data) || offset > m->total || m->length > m->total - offset ||
            (transfer && (transfer != m->transfer || total != m->total)) ||
            (!m->length && offset != m->total)) {
            fprintf(errors, "bvstk: invalid command response\n"); break;
        }
        transfer = m->transfer; total = m->total;
        if (m->length && fwrite(m->data, 1, m->length, output) != m->length) break;
        offset += m->length;
        if (offset == total) { result = m->result >= 0 && m->result <= 2 ? m->result : 1; break; }
        m->operation = BVSTK_COMMAND_READ; m->offset = offset;
    }
    close(fd); free(m); return result;
}

int bvstk_command_client_mdio_devices(bvstk_mdio_completion_device_t *devices,
    size_t capacity, size_t *count)
{
    *count = 0;
    FILE *out = tmpfile(), *errors = tmpfile();
    if (!out || !errors) { if (out) fclose(out); if (errors) fclose(errors); return 1; }
    char *arguments[] = {(char *)"mdio", (char *)"list"};
    int result = bvstk_command_client_execute(2, arguments, out, errors);
    if (!result) {
        rewind(out); char line[256];
        while (fgets(line, sizeof(line), out)) {
            char name[sizeof(devices[0].name)]; unsigned phy;
            if (*count < capacity && sscanf(line, " name=%31s phy=%u", name, &phy) == 2 && phy <= 31U) {
                strcpy(devices[*count].name, name); devices[(*count)++].phy_addr = (uint8_t)phy;
            }
        }
    }
    fclose(out); fclose(errors); return result;
}
