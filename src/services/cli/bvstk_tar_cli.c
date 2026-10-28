#include "services/cli/bvstk_storage_cli.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

typedef struct {
    bvstk_fs_reader_t file;
    int (*read)(void *,void *,size_t);
    void *context;
} tar_source_t;
static bvstk_fs_result_t exact(tar_source_t *reader, void *data, size_t size)
{
    size_t done = 0;
    while (done < size) {
        size_t count = 0; bool eof = false;
        bvstk_fs_result_t st;
        if(reader->read) {
            int n=reader->read(reader->context,(uint8_t *)data+done,size-done);
            if(n<0||(size_t)n>size-done)return BVSTK_FS_IO;
            count=(size_t)n;eof=!n;st=BVSTK_FS_OK;
        } else st=bvstk_fs_reader_read(&reader->file,(uint8_t *)data+done,size-done,&count,&eof);
        if (st != BVSTK_FS_OK) return st;
        done += count;
        if (done < size && (!count || eof)) return BVSTK_FS_IO;
    }
    return BVSTK_FS_OK;
}
static bool octal(const uint8_t *p, size_t size, uint64_t *out)
{
    uint64_t value = 0;
    size_t i = 0;
    while (i < size && (p[i] == ' ' || p[i] == '\0')) ++i;
    for (; i < size && p[i] >= '0' && p[i] <= '7'; ++i) {
        if (value > (UINT64_MAX - 7U) / 8U) return false;
        value = value * 8U + p[i] - '0';
    }
    for (; i < size; ++i) if (p[i] && p[i] != ' ') return false;
    *out = value; return true;
}
static bool field(char *out, size_t capacity, const uint8_t *p, size_t size)
{
    size_t n = 0;
    while (n < size && p[n]) ++n;
    if (n >= capacity) return false;
    memcpy(out, p, n); out[n] = '\0'; return true;
}
static bvstk_fs_result_t parent_dirs(const bvstk_fs_storage_t *s, char *path)
{
    char *slash = strrchr(path, '/');
    if (!slash || slash[-1] == ':') return BVSTK_FS_OK;
    *slash = '\0';
    bvstk_fs_result_t st = bvstk_fs_mkdirs(s, path);
    *slash = '/'; return st;
}
typedef struct {
    tar_source_t reader;
    uint8_t block[512], metadata[2048];
    char pax_path[BVSTK_FS_PATH_CAPACITY], relative[BVSTK_FS_PATH_CAPACITY];
    char checked[BVSTK_FS_PATH_CAPACITY], canonical[BVSTK_FS_PATH_CAPACITY];
    char path[BVSTK_FS_PATH_CAPACITY], name[101], prefix[156];
} archive_workspace_t;

static bvstk_fs_result_t archive(bvstk_cli_t *cli, const bvstk_fs_storage_t *s,
    const char *source, const char *destination, bool extract, int (*read)(void *,void *,size_t),void *context)
{
    archive_workspace_t *work = calloc(1, sizeof(*work));
    if (!work) return BVSTK_FS_BUSY;
    tar_source_t *reader = &work->reader;
    reader->read=read;reader->context=context;
    uint8_t *block = work->block, *metadata = work->metadata;
    char *pax_path = work->pax_path, *relative = work->relative;
    char *name = work->name, *prefix = work->prefix;
    char *checked = work->checked, *canonical = work->canonical, *path = work->path;
    bvstk_fs_result_t st = read?BVSTK_FS_OK:bvstk_fs_reader_open(&reader->file,s->reader,source,BVSTK_FS_FILE,0);
    while (st == BVSTK_FS_OK) {
        st = exact(reader, block, sizeof(work->block));
        if (st != BVSTK_FS_OK) break;
        bool zero = true;
        for (size_t i = 0; i < sizeof(work->block); ++i) if (block[i]) { zero = false; break; }
        if (zero) {
            st = exact(reader, block, sizeof(work->block));
            for (size_t i = 0; st == BVSTK_FS_OK && i < sizeof(work->block); ++i)
                if (block[i]) st = BVSTK_FS_INVALID;
            break;
        }
        uint64_t stored, size;
        unsigned sum = 0;
        for (unsigned i = 0; i < 512U; ++i) sum += i >= 148U && i < 156U ? ' ' : block[i];
        if (!octal(block + 148U, 8U, &stored) || sum != stored ||
            !octal(block + 124U, 12U, &size) || size > UINT32_MAX) { st = BVSTK_FS_INVALID; break; }
        if (!field(name, sizeof(work->name), block, 100U) ||
            !field(prefix, sizeof(work->prefix), block + 345U, 155U)) { st = BVSTK_FS_RANGE; break; }
        unsigned type = block[156];
        if (type == 'x') {
            /* Bounded PAX metadata emitted by the common TAR reader. */
            if (!size || size >= sizeof(work->metadata)) { st = BVSTK_FS_RANGE; break; }
            st = exact(reader, metadata, (size_t)size);
            for (size_t offset = 0; st == BVSTK_FS_OK && offset < size;) {
                size_t start = offset, length = 0;
                while (offset < size && metadata[offset] >= '0' && metadata[offset] <= '9') {
                    unsigned digit = metadata[offset++] - '0';
                    if (length > (sizeof(work->metadata) - digit) / 10U) { st = BVSTK_FS_INVALID; break; }
                    length = length * 10U + digit;
                }
                if (st != BVSTK_FS_OK) break;
                if (!length || length > size - start || offset >= size || metadata[offset++] != ' ' ||
                    offset >= start + length || metadata[start + length - 1U] != '\n') { st = BVSTK_FS_INVALID; break; }
                size_t content = start + length - 1U - offset;
                if (content >= 5U && !memcmp(metadata + offset, "path=", 5U)) {
                    content -= 5U;
                    if (!content || content >= sizeof(work->pax_path)) { st = BVSTK_FS_RANGE; break; }
                    if (memchr(metadata + offset + 5U, '\0', content)) { st = BVSTK_FS_INVALID; break; }
                    memcpy(pax_path, metadata + offset + 5U, content); pax_path[content] = '\0';
                }
                offset = start + length;
            }
            if (st == BVSTK_FS_OK && size % 512U) st = exact(reader, block, 512U - (size_t)(size % 512U));
            continue;
        }
        if (type != 0 && type != '0' && type != '5') { st = BVSTK_FS_UNSUPPORTED; break; }
        if (*pax_path) { strcpy(relative, pax_path); pax_path[0] = '\0'; }
        else {
            int n = snprintf(relative, sizeof(work->relative), "%s%s%s", prefix, *prefix ? "/" : "", name);
            if (n < 0 || (size_t)n >= sizeof(work->relative)) { st = BVSTK_FS_RANGE; break; }
        }
        while (!strncmp(relative, "./", 2U)) memmove(relative, relative + 2U, strlen(relative + 2U) + 1U);
        if (type == '5' && !size && (!*relative || !strcmp(relative, "."))) continue;
        size_t n = strlen(relative);
        while (n && relative[n - 1U] == '/') relative[--n] = '\0';
        /* Archive member names must already be relative and canonical. */
        if (n + 7U >= sizeof(work->checked)) { st = BVSTK_FS_RANGE; break; }
        memcpy(checked, "flash:/", 7U); memcpy(checked + 7U, relative, n + 1U);
        st = !n || relative[0] == '/' || strchr(relative, ':') ? BVSTK_FS_INVALID :
            bvstk_fs_validate_path((const uint8_t *)checked, n + 7U, canonical, sizeof(work->canonical));
        if (st != BVSTK_FS_OK) break;
        if (extract) {
            st = bvstk_cli_path(destination, relative, path);
            if (st != BVSTK_FS_OK) break;
        }
        bvstk_fs_writer_t writer = {0};
        if (type == '5') {
            if (size) { st = BVSTK_FS_INVALID; break; }
            if (extract) st = bvstk_fs_mkdirs(s, path);
        } else if (extract) {
            st = parent_dirs(s, path);
            if (st == BVSTK_FS_OK) st = bvstk_fs_writer_open(&writer, s, path, true);
        }
        if (!extract) bvstk_cli_writef(cli, "%s%s\r\n", relative, type == '5' ? "/" : "");
        uint64_t remaining = size;
        while (st == BVSTK_FS_OK && remaining) {
            size_t chunk = remaining < sizeof(work->block) ? (size_t)remaining : sizeof(work->block);
            st = exact(reader, block, chunk);
            if (st == BVSTK_FS_OK && writer.handle) st = bvstk_fs_writer_write(&writer, block, chunk);
            remaining -= chunk;
        }
        if (writer.handle) {
            bvstk_fs_result_t closed = bvstk_fs_writer_close(&writer);
            if (st == BVSTK_FS_OK) st = closed;
        }
        if (st == BVSTK_FS_OK && size % 512U) st = exact(reader, block, 512U - (size_t)(size % 512U));
        if (cli->result) st = BVSTK_FS_IO;
    }
    if(!read)bvstk_fs_reader_close(&reader->file);
    free(work);
    return st;
}
int bvstk_tar_cli(bvstk_cli_t *cli, const bvstk_fs_storage_t *s, const char *cwd,
    int argc, char **argv)
{
    if (!argc || !strcasecmp(argv[0], "-h") || !strcasecmp(argv[0], "--help")) {
        bvstk_cli_text(cli, "tar c <directory> <archive> | x <archive> <directory> | t <archive>\r\n");
        return cli->result;
    }
    bool create = !strcasecmp(argv[0], "c"), extract = !strcasecmp(argv[0], "x");
    bvstk_fs_result_t st = BVSTK_FS_INVALID;
    char source[BVSTK_FS_PATH_CAPACITY], destination[BVSTK_FS_PATH_CAPACITY];
    if (!s || !s->reader) st = BVSTK_FS_NOT_READY;
    else if (((create || extract) && argc == 3) || (!strcasecmp(argv[0], "t") && argc == 2)) {
        st = bvstk_cli_path(cwd, argv[1], source);
        if (st == BVSTK_FS_OK && argc == 3) st = bvstk_cli_path(cwd, argv[2], destination);
        if (st == BVSTK_FS_OK && create) {
            size_t n = strlen(source);
            if (!strncmp(source, destination, n) && (!destination[n] || source[n - 1U] == '/' || destination[n] == '/'))
                st = BVSTK_FS_INVALID;
            else {
                bvstk_fs_reader_t *reader = calloc(1, sizeof(*reader));
                if (!reader) st = BVSTK_FS_BUSY;
                else {
                    st = bvstk_fs_reader_open(reader, s->reader, source, BVSTK_FS_TAR, 0);
                    bvstk_fs_writer_t writer = {0};
                    if (st == BVSTK_FS_OK) st = bvstk_fs_writer_open(&writer, s, destination, true);
                    bool eof = false;
                    while (st == BVSTK_FS_OK && !eof) {
                        uint8_t data[1024]; size_t size;
                        st = bvstk_fs_reader_read(reader, data, sizeof(data), &size, &eof);
                        if (st == BVSTK_FS_OK) st = bvstk_fs_writer_write(&writer, data, size);
                    }
                    if (writer.handle) {
                        bvstk_fs_result_t closed = bvstk_fs_writer_close(&writer);
                        if (st == BVSTK_FS_OK) st = closed;
                    }
                    bvstk_fs_reader_close(reader); free(reader);
                }
            }
        } else if (st == BVSTK_FS_OK) {
            if (extract) st = bvstk_fs_mkdirs(s, destination);
            if (st == BVSTK_FS_OK) st = archive(cli, s, source, extract ? destination : NULL, extract,NULL,NULL);
        }
    }
    if (st != BVSTK_FS_OK || create || extract) bvstk_cli_fs_result(cli, st);
    return cli->result;
}

bvstk_fs_result_t bvstk_tar_extract_stream(const bvstk_fs_storage_t *s,const char *destination,
    int (*read)(void *,void *,size_t),void *context)
{
    if(!s||!read||!destination)return BVSTK_FS_INVALID;
    char checked[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_result_t st=bvstk_fs_validate_path((const uint8_t *)destination,strlen(destination),checked,sizeof(checked));
    if(st==BVSTK_FS_OK)st=bvstk_fs_mkdirs(s,checked);
    bvstk_cli_t cli={0};
    return st==BVSTK_FS_OK?archive(&cli,s,NULL,checked,true,read,context):st;
}
