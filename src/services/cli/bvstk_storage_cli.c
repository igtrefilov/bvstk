#include "services/cli/bvstk_storage_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include "hardware/pl/spi/bvstk_spi_regs.h"

static bool eq(const char *a, const char *b) { return a && !strcasecmp(a, b); }
void bvstk_cli_fs_result(bvstk_cli_t *cli, bvstk_fs_result_t st)
{
    if (st == BVSTK_FS_OK) bvstk_cli_text(cli, "OK\r\n");
    else {
        cli->result = st == BVSTK_FS_INVALID || st == BVSTK_FS_RANGE ? 2 : 1;
        bvstk_cli_writef(cli, "ERR fs %s\r\n", bvstk_fs_result_name(st));
    }
}
bvstk_fs_result_t bvstk_cli_path(const char *cwd, const char *input,
    char out[BVSTK_FS_PATH_CAPACITY])
{
    char path[BVSTK_FS_PATH_CAPACITY];
    if (!cwd || !strstr(cwd, ":/")) cwd = "flash:/";
    if (!input || !*input) input = cwd;
    if (strchr(input, ':')) {
        if (strlen(input) >= sizeof(path)) return BVSTK_FS_RANGE;
        strcpy(path, input);
    } else if (input[0] == '/') {
        const char *colon = strchr(cwd, ':');
        if (!colon || (size_t)(colon - cwd) + strlen(input) + 2U > sizeof(path)) return BVSTK_FS_RANGE;
        snprintf(path, sizeof(path), "%.*s:%s", (int)(colon - cwd), cwd, input);
    } else {
        int n = snprintf(path, sizeof(path), "%s%s%s", cwd,
            cwd[strlen(cwd) - 1U] == '/' ? "" : "/", input);
        if (n < 0 || (size_t)n >= sizeof(path)) return BVSTK_FS_RANGE;
    }
    /* Normalize relative navigation inside the selected logical volume. */
    const char *colon = strstr(path, ":/");
    if (!colon || (size_t)(colon - path) > 15U) return BVSTK_FS_INVALID;
    size_t root = (size_t)(colon - path) + 2U, length = root;
    memcpy(out, path, root); out[root] = '\0';
    char *part = path + root;
    while (*part) {
        char *end = strchr(part, '/');
        if (end) *end = '\0';
        if (!strcmp(part, "..")) {
            if (length == root) return BVSTK_FS_INVALID;
            while (length > root && out[length - 1U] != '/') --length;
            if (length > root) --length;
            out[length] = '\0';
        } else if (*part && strcmp(part, ".")) {
            size_t n = strlen(part);
            if (length + n + 2U > BVSTK_FS_PATH_CAPACITY) return BVSTK_FS_RANGE;
            if (length != root) out[length++] = '/';
            memcpy(out + length, part, n + 1U); length += n;
        }
        if (!end) break;
        part = end + 1U;
    }
    char canonical[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_result_t st = bvstk_fs_validate_path((const uint8_t *)out,
        strlen(out), canonical, sizeof(canonical));
    if (st == BVSTK_FS_OK) strcpy(out, canonical);
    return st;
}
static bvstk_fs_result_t list(bvstk_cli_t *cli, const bvstk_fs_backend_t *fs, const char *path)
{
    void *dir = NULL;
    bvstk_fs_result_t st = fs->dir_open(fs->context, path, &dir);
    if (st != BVSTK_FS_OK) return st;
    for (;;) {
        char name[BVSTK_FS_PATH_CAPACITY]; bvstk_fs_info_t info;
        st = fs->dir_read(fs->context, dir, name, sizeof(name), &info);
        if (st != BVSTK_FS_OK || !*name) break;
        bvstk_cli_writef(cli, "%c %10" PRIu64 " %s\r\n",
            info.type == BVSTK_FS_ENTRY_DIR ? 'd' : '-', info.size, name);
        if (cli->result) { st = BVSTK_FS_IO; break; }
    }
    fs->dir_close(fs->context, dir);
    return st;
}
static bool nested(const char *from, const char *to)
{
    size_t n = strlen(from);
    return !strncmp(from, to, n) && (!to[n] || from[n - 1U] == '/' || to[n] == '/');
}
typedef struct { char from[BVSTK_FS_PATH_CAPACITY], to[BVSTK_FS_PATH_CAPACITY], name[BVSTK_FS_PATH_CAPACITY]; } tree_level_t;
static bvstk_fs_result_t tree(const bvstk_fs_storage_t *s, tree_level_t *levels,
    unsigned depth, bool copy)
{
    if (depth >= BVSTK_FS_MAX_DEPTH) return BVSTK_FS_RANGE;
    tree_level_t *current = &levels[depth];
    bvstk_fs_info_t info;
    bvstk_fs_result_t st = bvstk_fs_stat(s->reader, current->from, &info);
    if (st != BVSTK_FS_OK) return st;
    if (info.type == BVSTK_FS_ENTRY_FILE) return copy ?
        bvstk_fs_copy(s, current->from, current->to, NULL) : bvstk_fs_remove(s, current->from);
    if (copy) {
        st = bvstk_fs_mkdir(s, current->to);
        if (st != BVSTK_FS_OK) return st;
    }
    void *dir = NULL;
    const bvstk_fs_backend_t *fs = s->reader;
    st = fs->dir_open(fs->context, current->from, &dir);
    if (st != BVSTK_FS_OK) return st;
    for (;;) {
        char *name = current->name;
        st = fs->dir_read(fs->context, dir, name, sizeof(current->name), &info);
        if (st != BVSTK_FS_OK || !*name) break;
        if (depth + 1U >= BVSTK_FS_MAX_DEPTH) { st = BVSTK_FS_RANGE; break; }
        tree_level_t *next = &levels[depth + 1U];
        st = bvstk_cli_path(current->from, name, next->from);
        if (st == BVSTK_FS_OK && copy) st = bvstk_cli_path(current->to, name, next->to);
        if (st == BVSTK_FS_OK) st = tree(s, levels, depth + 1U, copy);
        if (st != BVSTK_FS_OK) break;
    }
    fs->dir_close(fs->context, dir);
    return st != BVSTK_FS_OK || copy ? st : bvstk_fs_remove(s, current->from);
}
int bvstk_storage_cli(bvstk_cli_t *cli, const bvstk_fs_storage_t *s,
    const char *cwd, int argc, char **argv)
{
    if (!argc || eq(argv[0], "-h") || eq(argv[0], "--help")) {
        bvstk_cli_text(cli, "fs volumes | ls [path] | stat <path> | cat <path>\r\n"
            "fs mkdir <path> | touch <path> | write|append <path> <text>\r\n"
            "fs cp [-r] <src> <dst> | mv <src> <dst> | rm [-r] <path>\r\n"
            "Paths use flash:/, sd:/ and sd-pl:/; relative paths use the current volume.\r\n");
        return cli->result;
    }
    if (!s || !s->reader) { bvstk_cli_fs_result(cli, BVSTK_FS_NOT_READY); return cli->result; }
    const bvstk_fs_backend_t *fs = s->reader;
    bvstk_fs_result_t st = BVSTK_FS_INVALID;
    if (argc == 1 && eq(argv[0], "volumes")) {
        st = BVSTK_FS_OK;
        for (size_t i = 0; i < fs->volume_count(fs->context); ++i) {
            bvstk_fs_volume_t volume;
            st = fs->volume(fs->context, i, &volume);
            if (st != BVSTK_FS_OK) break;
            bvstk_cli_writef(cli, "%s:/ %s\r\n", volume.name,
                volume.state == 1 ? "ready" : volume.state == 2 ? "unsupported" : "not_ready");
        }
        if (st != BVSTK_FS_OK) bvstk_cli_fs_result(cli, st);
        return cli->result;
    }
    int first = 1;
    bool recursive = argc > 1 && (eq(argv[1], "-r") || eq(argv[1], "-R"));
    if (recursive) ++first;
    char path[BVSTK_FS_PATH_CAPACITY], destination[BVSTK_FS_PATH_CAPACITY];
    st = bvstk_cli_path(cwd, argc > first ? argv[first] : NULL, path);
    if (st != BVSTK_FS_OK) { bvstk_cli_fs_result(cli, st); return cli->result; }
    if (eq(argv[0], "ls") && argc <= 2) st = list(cli, fs, path);
    else if (eq(argv[0], "stat") && argc == 2) {
        bvstk_fs_info_t info; st = bvstk_fs_stat(fs, path, &info);
        if (st == BVSTK_FS_OK) bvstk_cli_writef(cli, "%s type=%s size=%" PRIu64 "\r\n", path,
            info.type == BVSTK_FS_ENTRY_DIR ? "directory" : "file", info.size);
    } else if (eq(argv[0], "cat") && argc == 2) {
        bvstk_fs_reader_t *reader = calloc(1, sizeof(*reader));
        if (!reader) st = BVSTK_FS_BUSY;
        else {
            st = bvstk_fs_reader_open(reader, fs, path, BVSTK_FS_FILE, 0);
            bool eof = false;
            while (st == BVSTK_FS_OK && !eof) {
                uint8_t data[1024]; size_t size;
                st = bvstk_fs_reader_read(reader, data, sizeof(data), &size, &eof);
                if (st == BVSTK_FS_OK && size &&
                    bvstk_output_write(&cli->output, data, size) != (int)size) st = BVSTK_FS_IO;
            }
            bvstk_fs_reader_close(reader); free(reader);
        }
    } else if (eq(argv[0], "mkdir") && argc == 2) st = bvstk_fs_mkdir(s, path);
    else if ((eq(argv[0], "write") || eq(argv[0], "append") || eq(argv[0], "touch")) &&
        ((argc >= 3 && !eq(argv[0], "touch")) || (argc == 2 && eq(argv[0], "touch")))) {
        bvstk_fs_writer_t writer;
        st = eq(argv[0], "write") ? bvstk_fs_writer_open(&writer, s, path, true) :
            bvstk_fs_writer_append(&writer, s, path);
        if (st == BVSTK_FS_OK) {
            for (int i = 2; i < argc && st == BVSTK_FS_OK; ++i) {
                if (i > 2) st = bvstk_fs_writer_write(&writer, " ", 1);
                if (st == BVSTK_FS_OK) st = bvstk_fs_writer_write(&writer, argv[i], strlen(argv[i]));
            }
            bvstk_fs_result_t closed = bvstk_fs_writer_close(&writer);
            if (st == BVSTK_FS_OK) st = closed;
        }
    } else if ((eq(argv[0], "cp") || eq(argv[0], "mv")) && argc == first + 2) {
        st = bvstk_cli_path(cwd, argv[first + 1], destination);
        if (st == BVSTK_FS_OK && eq(argv[0], "mv") && !recursive) st = bvstk_fs_rename(s, path, destination);
        else if (st == BVSTK_FS_OK && eq(argv[0], "cp") && !recursive) st = bvstk_fs_copy(s, path, destination, NULL);
        else if (st == BVSTK_FS_OK && eq(argv[0], "cp")) {
            tree_level_t *levels = calloc(BVSTK_FS_MAX_DEPTH, sizeof(*levels));
            if (!levels) st = BVSTK_FS_BUSY;
            else {
                strcpy(levels[0].from, path); strcpy(levels[0].to, destination);
                st = nested(path, destination) ? BVSTK_FS_INVALID : tree(s, levels, 0, true);
                free(levels);
            }
        } else st = BVSTK_FS_INVALID;
    } else if (eq(argv[0], "rm") && argc == first + 1) {
        if (!recursive) st = bvstk_fs_remove(s, path);
        else if (!strcmp(strstr(path, ":/"), ":/")) st = BVSTK_FS_INVALID;
        else {
            tree_level_t *levels = calloc(BVSTK_FS_MAX_DEPTH, sizeof(*levels));
            if (!levels) st = BVSTK_FS_BUSY;
            else { strcpy(levels[0].from, path); st = tree(s, levels, 0, false); free(levels); }
        }
    } else st = BVSTK_FS_INVALID;
    if (st != BVSTK_FS_OK || !(eq(argv[0], "ls") || eq(argv[0], "cat") || eq(argv[0], "stat")))
        bvstk_cli_fs_result(cli, st);
    return cli->result;
}
int bvstk_memory_cli(bvstk_cli_t *cli, const bvstk_control_api_t *control, int n, char **a)
{
    if (!n || eq(a[0], "-h") || eq(a[0], "--help")) {
        bvstk_cli_text(cli, "mem r <address> | mem w <address> <value>\r\n"
            "Aligned addresses use 32 bits; unaligned addresses use 8 bits when supported.\r\n");
        return cli->result;
    }
    unsigned long address, value = 0;
    bvstk_status_t st = n >= 2 ? bvstk_cli_number(a[1], UINT32_MAX, &address) : BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK && eq(a[0], "r") && n == 2) {
        uint64_t data;
        st = bvstk_control_mem_read(control, (uint32_t)address, address & 3U ? 8U : 32U, &data);
        if (st == BVSTK_OK) bvstk_cli_writef(cli, "OK 0x%0*" PRIX64 " %" PRIu64 "\r\n",
            address & 3U ? 2 : 8, data, data);
    } else if (st == BVSTK_OK && eq(a[0], "w") && n == 3) {
        st = bvstk_cli_number(a[2], address & 3U ? UINT8_MAX : UINT32_MAX, &value);
        if (st == BVSTK_OK) st = bvstk_control_mem_write(control,
            (uint32_t)address, address & 3U ? 8U : 32U, value);
        bvstk_cli_result(cli, "mem", st); return cli->result;
    } else if (st == BVSTK_OK) st = BVSTK_ERR_MALFORMED;
    if (st != BVSTK_OK) bvstk_cli_result(cli, "mem", st);
    return cli->result;
}
int bvstk_pl_cli(bvstk_cli_t *cli, bvstk_control_api_t *control, int n, char **a)
{
    if (!n || eq(a[0], "-h") || eq(a[0], "--help")) {
        bvstk_cli_text(cli, "pl list | probe | read <region> <offset> [words] | write <region> <offset> <value>\r\n");
        return cli->result;
    }
    if (n == 1 && eq(a[0], "list")) {
        for (size_t i = 0; i < bvstk_pl_region_count(); ++i) {
            const bvstk_pl_region_desc_t *r = bvstk_pl_region_get((bvstk_pl_region_id_t)i);
            bvstk_cli_writef(cli, "%-12s base=0x%08" PRIXPTR " size=0x%zx kind=%s\r\n",
                r->name, r->physical_base, r->size, r->kind == BVSTK_PL_REGION_BRAM ? "bram" : "control");
        }
        return cli->result;
    }
    if (n == 1 && eq(a[0], "probe")) {
        for (size_t i = 0; i < bvstk_pl_region_count(); ++i) {
            const bvstk_pl_region_desc_t *r = bvstk_pl_region_get((bvstk_pl_region_id_t)i);
            if (r->kind != BVSTK_PL_REGION_CONTROL) continue;
            uint32_t value; size_t offset = r->id == BVSTK_PL_SPI_MASTER ? BVSTK_SPI_SIGNATURE_OFFSET : 0;
            bvstk_status_t st = bvstk_control_pl_read32(control, r->id, offset, &value);
            if (st != BVSTK_OK) { bvstk_cli_result(cli, "pl", st); return cli->result; }
            bvstk_cli_writef(cli, "%s+0x%04zx = 0x%08" PRIX32 "\r\n", r->name, offset, value);
        }
        return cli->result;
    }
    const bvstk_pl_region_desc_t *r = n > 1 ? bvstk_pl_region_find(a[1]) : NULL;
    unsigned long offset = 0, count = 1, value = 0;
    bvstk_status_t st = r && n >= 3 ? bvstk_cli_number(a[2], UINT32_MAX, &offset) : BVSTK_ERR_MALFORMED;
    bool writing = eq(a[0], "write");
    if (st == BVSTK_OK && writing && n == 4) st = bvstk_cli_number(a[3], UINT32_MAX, &value);
    else if (st == BVSTK_OK && eq(a[0], "read") && (n == 3 || n == 4)) {
        if (n == 4) st = bvstk_cli_number(a[3], 64U, &count);
    } else if (st == BVSTK_OK) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK && (!count || offset & 3U || offset > r->size || count * 4U > r->size - offset)) st = BVSTK_ERR_RANGE;
    for (size_t i = 0; st == BVSTK_OK && i < count; ++i) {
        uint32_t data;
        st = writing ? bvstk_control_pl_write32(control, r->id, offset, (uint32_t)value) :
            bvstk_control_pl_read32(control, r->id, offset + i * 4U, &data);
        if (st == BVSTK_OK && !writing) bvstk_cli_writef(cli, "%s+0x%04zx = 0x%08" PRIX32 "\r\n",
            r->name, (size_t)offset + i * 4U, data);
    }
    if (st != BVSTK_OK || writing) bvstk_cli_result(cli, "pl", st);
    return cli->result;
}
