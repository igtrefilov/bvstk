#define _POSIX_C_SOURCE 200809L
#include "services/cli/bvstk_storage_cli.h"
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static bvstk_fs_posix_t port;
static const bvstk_fs_storage_t *storage;
static char output[65536];
static int command(const char *input)
{
    char line[2048], *argv[64];
    assert(strlen(input) < sizeof(line)); strcpy(line, input);
    int n = bvstk_cli_tokens(line, argv, 64); assert(n > 0);
    bvstk_cli_buffer_t buffer = {output, sizeof(output), 0, false}; output[0] = '\0';
    bvstk_cli_t cli = {0}; cli.output = (bvstk_output_t){bvstk_cli_buffer_write, &buffer};
    int result = !strcmp(argv[0], "tar") ? bvstk_tar_cli(&cli, storage, "flash:/", n - 1, argv + 1) :
        bvstk_storage_cli(&cli, storage, "flash:/", n - 1, argv + 1);
    return result;
}
static void import_archive(const char *directory, const char *name)
{
    char path[1024];
    assert(snprintf(path, sizeof(path), "%s/%s.tar", directory, name) < (int)sizeof(path));
    FILE *input = fopen(path, "rb"); assert(input);
    assert(snprintf(path, sizeof(path), "flash:/%s.tar", name) < (int)sizeof(path));
    bvstk_fs_writer_t writer;
    assert(bvstk_fs_writer_open(&writer, storage, path, false) == BVSTK_FS_OK);
    unsigned char data[511]; size_t size;
    while ((size = fread(data, 1, sizeof(data), input)))
        assert(bvstk_fs_writer_write(&writer, data, size) == BVSTK_FS_OK);
    assert(!ferror(input) && !fclose(input));
    assert(bvstk_fs_writer_close(&writer) == BVSTK_FS_OK);
}
static void contents(const char *path, const char *expected)
{
    bvstk_fs_reader_t reader;
    assert(bvstk_fs_reader_open(&reader, storage->reader, path, BVSTK_FS_FILE, 0) == BVSTK_FS_OK);
    char text[512]; size_t size; bool eof;
    assert(bvstk_fs_reader_read(&reader, (uint8_t *)text, sizeof(text), &size, &eof) == BVSTK_FS_OK);
    assert(eof && size == strlen(expected) && !memcmp(text, expected, size));
    bvstk_fs_reader_close(&reader);
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    char flash[512], sd[512];
    assert(snprintf(flash, sizeof(flash), "%s/flash", argv[1]) < (int)sizeof(flash));
    assert(snprintf(sd, sizeof(sd), "%s/sd", argv[1]) < (int)sizeof(sd));
    assert(!mkdir(argv[1], 0700) && !mkdir(flash, 0700) && !mkdir(sd, 0700));
    const bvstk_fs_posix_volume_t volumes[] = {{"flash", flash, false}, {"sd", sd, false}, {"sd-pl", NULL, false}};
    assert(bvstk_fs_posix_init(&port, volumes, 3) == BVSTK_FS_OK);
    storage = bvstk_fs_posix_storage(&port);
    assert(!command("fs volumes") && strstr(output, "sd-pl:/ unsupported"));
    assert(!command("fs mkdir source"));
    assert(!command("fs write \"source/данные с пробелом.txt\" \"two  spaces\""));
    assert(!command("fs append \"source/данные с пробелом.txt\" \" + more\""));
    contents("flash:/source/данные с пробелом.txt", "two  spaces + more");
    assert(!command("fs touch source/empty"));
    assert(!command("fs touch \"source/данные с пробелом.txt\""));
    contents("flash:/source/данные с пробелом.txt", "two  spaces + more");
    assert(!command("fs ls source") && strstr(output, "данные с пробелом.txt"));
    assert(!command("fs cp -r source sd:/copy"));
    contents("sd:/copy/данные с пробелом.txt", "two  spaces + more");
    assert(command("fs cp -r source source/inside"));
    bvstk_fs_info_t info;
    assert(bvstk_fs_stat(storage->reader, "flash:/source/inside", &info) == BVSTK_FS_NOT_FOUND);
    assert(command("fs cp source/empty sd:/copy/empty"));
    assert(!command("fs mv sd:/copy/empty sd:/copy/renamed"));
    assert(command("fs mv sd:/copy/renamed flash:/cross-volume"));
    assert(!command("tar c source sd:/snapshot.tar"));
    assert(!command("tar t sd:/snapshot.tar") && strstr(output, "данные с пробелом.txt"));
    assert(!command("tar x sd:/snapshot.tar flash:/restored"));
    contents("flash:/restored/данные с пробелом.txt", "two  spaces + more");
    assert(command("tar c source source/inside.tar"));
    assert(command("fs rm -r flash:/"));
    assert(command("fs write ../escape bad"));
    assert(command("fs write sd-pl:/unsupported bad"));
    char parsed[] = "fs write \"a b\" \"quote\\\" backslash\\\\\"", *tokens[8];
    assert(bvstk_cli_tokens(parsed, tokens, 8) == 4);
    assert(!strcmp(tokens[2], "a b") && !strcmp(tokens[3], "quote\" backslash\\"));
    char invalid[] = "fs write \"unterminated";
    assert(bvstk_cli_tokens(invalid, tokens, 8) < 0);
    /* Truncated archives cannot be acknowledged as complete extraction. */
    assert(!command("fs write broken.tar invalid"));
    assert(command("tar x broken.tar flash:/broken"));
    /* Accept standard ustar/PAX, reject paths/links/checksums before extraction. */
    const char *fixtures[] = {"ustar", "pax", "escape", "absolute", "link", "checksum", "truncated"};
    char cmd[256];
    for (unsigned i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        import_archive(argv[2], fixtures[i]);
        assert(snprintf(cmd, sizeof(cmd), "tar x %s.tar flash:/external", fixtures[i]) < (int)sizeof(cmd));
        int result = command(cmd);
        if ((result == 0) != (i < 2)) fprintf(stderr, "%s: result=%d %s", cmd, result, output);
        assert((result == 0) == (i < 2));
        assert(!port.files && !port.directories);
        assert(snprintf(cmd, sizeof(cmd), "fs rm %s.tar", fixtures[i]) < (int)sizeof(cmd));
        assert(!command(cmd));
    }
    contents("flash:/external/nested/plain.txt", "external");
    assert(!command("fs ls external/nested") && strstr(output, "длинное имя"));
    assert(bvstk_fs_stat(storage->reader, "flash:/outside", &info) == BVSTK_FS_NOT_FOUND);
    assert(!command("fs rm -r external"));
    assert(!command("fs rm -r source"));
    assert(!command("fs rm -r restored"));
    assert(!command("fs rm -r broken"));
    assert(!command("fs rm broken.tar"));
    assert(!command("fs rm -r sd:/copy"));
    assert(!command("fs rm sd:/snapshot.tar"));
    assert(!port.files && !port.directories);
    assert(bvstk_fs_posix_destroy(&port) == BVSTK_FS_OK);
    assert(!rmdir(flash) && !rmdir(sd) && !rmdir(argv[1]));
    puts("Shared storage CLI: quoting, Unicode, append, recursive operations, TAR, volume bounds and handle cleanup passed");
    return 0;
}
