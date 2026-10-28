#ifndef TEST_FS_STORAGE_CONTRACT_H
#define TEST_FS_STORAGE_CONTRACT_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "services/fs/bvstk_fs_storage.h"

static void fs_expect(const bvstk_fs_backend_t *b, const char *path, const void *expected, size_t size)
{
    bvstk_fs_reader_t reader;
    uint8_t buffer[271];
    size_t offset = 0, count;
    bool eof = false;
    assert(bvstk_fs_reader_open(&reader, b, path, BVSTK_FS_FILE, 0) == BVSTK_FS_OK);
    while (!eof) {
        assert(bvstk_fs_reader_read(&reader, buffer, sizeof(buffer), &count, &eof) == BVSTK_FS_OK);
        assert(count <= size - offset);
        assert(!memcmp(buffer, (const uint8_t *)expected + offset, count));
        offset += count;
    }
    assert(offset == size);
    bvstk_fs_reader_close(&reader);
}

static const bvstk_fs_storage_t *fs_fault_base;
static int fs_write_short, fs_close_failure, fs_install_failure;
static bvstk_fs_result_t fs_fault_write(void *c, void *h, const uint8_t *p, size_t n, size_t *done)
{
    return fs_fault_base->write(c, h, p, fs_write_short && n ? n - 1U : n, done);
}
static bvstk_fs_result_t fs_fault_finish(void *c, void *h, bool sync)
{
    bvstk_fs_result_t rc = fs_fault_base->finish(c, h, sync);
    return fs_close_failure ? BVSTK_FS_IO : rc;
}
static bvstk_fs_result_t fs_fault_install(void *c, const char *a, const char *b, const char *backup)
{
    return fs_install_failure ? BVSTK_FS_IO : fs_fault_base->install(c, a, b, backup);
}

/* Identical end-to-end expectations for the actual POSIX and FatFs adapters.
 * Backend fault injection verifies that errors do not become successful saves. */
static void fs_storage_contract(const bvstk_fs_storage_t *s)
{
    uint8_t data[4097];
    char path[BVSTK_FS_PATH_CAPACITY];
    bvstk_fs_writer_t w;
    bvstk_fs_info_t info;
    uint64_t copied;
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (uint8_t)(i * 17U);
    assert(bvstk_fs_path("flash", "/scope/", path, sizeof(path)) == BVSTK_FS_OK);
    assert(!strcmp(path, "flash:/scope"));
    const char *invalid[] = {"flash:/../outside", "flash:/scope/./x", "flash://x",
        "flash:/scope/trailing.", "flash:/scope/trailing ", "flash:/x\\y", "flash:/x:y",
        "flash:/x\001", "flash:/bad\xC0\x80", "flash:relative", "/flash/absolute"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(bvstk_fs_writer_open(&w, s, invalid[i], true) == BVSTK_FS_INVALID);
        assert(bvstk_fs_remove(s, invalid[i]) == BVSTK_FS_INVALID);
    }
    assert(bvstk_fs_remove(s, "flash:/") == BVSTK_FS_INVALID);
    assert(bvstk_fs_mkdir(s, "flash:/") == BVSTK_FS_INVALID);
    assert(bvstk_fs_writer_open(&w, s, "flash:/", true) == BVSTK_FS_INVALID);
    assert(bvstk_fs_mkdirs(s, "flash:/scope/nested") == BVSTK_FS_OK);
    assert(bvstk_fs_mkdirs(s, "flash:/scope/nested") == BVSTK_FS_OK);
    assert(bvstk_fs_writer_open(&w, s, "flash:/scope/nested/данные.bin", false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, data, 1) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, data + 1, sizeof(data) - 1U) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK && w.bytes == sizeof(data));
    fs_expect(s->reader, "flash:/scope/nested/данные.bin", data, sizeof(data));
    assert(bvstk_fs_writer_open(&w, s, "flash:/scope/nested/данные.bin", false) == BVSTK_FS_EXISTS);
    fs_expect(s->reader, "flash:/scope/nested/данные.bin", data, sizeof(data));
    assert(bvstk_fs_copy(s, "flash:/scope/nested/данные.bin", "sd:/copied.bin", &copied) == BVSTK_FS_OK);
    assert(copied == sizeof(data));
    fs_expect(s->reader, "sd:/copied.bin", data, sizeof(data));
    assert(bvstk_fs_copy(s, "flash:/scope/nested/данные.bin", "sd:/copied.bin", NULL) == BVSTK_FS_EXISTS);
    assert(bvstk_fs_copy(s, "flash:/scope", "sd:/directory", NULL) == BVSTK_FS_TYPE_ERROR);
    assert(bvstk_fs_rename(s, "sd:/copied.bin", "flash:/cross-volume") == BVSTK_FS_INVALID);
    assert(bvstk_fs_rename(s, "sd:/copied.bin", "sd:/renamed.bin") == BVSTK_FS_OK);
    assert(bvstk_fs_stat(s->reader, "sd:/copied.bin", &info) == BVSTK_FS_NOT_FOUND);
    fs_expect(s->reader, "sd:/renamed.bin", data, sizeof(data));
    assert(bvstk_fs_remove(s, "flash:/scope") == BVSTK_FS_DENIED);
    assert(bvstk_fs_writer_open(&w, s, "flash:/empty", false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK);
    fs_expect(s->reader, "flash:/empty", "", 0);
    assert(bvstk_fs_writer_open(&w, s, "sd:/renamed.bin", true) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, "short", 5) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK);
    fs_expect(s->reader, "sd:/renamed.bin", "short", 5);
    assert(bvstk_fs_writer_append(&w, s, "sd:/renamed.bin") == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, "+tail", 5) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK);
    fs_expect(s->reader, "sd:/renamed.bin", "short+tail", 10);
    assert(bvstk_fs_writer_append(&w, s, "sd:/renamed.bin") == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK);
    fs_expect(s->reader, "sd:/renamed.bin", "short+tail", 10);
    assert(bvstk_fs_writer_append(&w, s, "sd:/new-append") == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, "created", 7) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK);
    fs_expect(s->reader, "sd:/new-append", "created", 7);
    assert(bvstk_fs_remove(s, "sd:/new-append") == BVSTK_FS_OK);
    assert(bvstk_fs_writer_open(&w, s, "sd:/interrupted", false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, data, 17) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_abort(&w) == BVSTK_FS_OK);
    fs_expect(s->reader, "sd:/interrupted", data, 17);
    assert(bvstk_fs_save(s, "flash:/scope/profile.json", "old", 3) == BVSTK_FS_OK);
    assert(bvstk_fs_save(s, "flash:/scope/profile.json", "new", 3) == BVSTK_FS_OK);
    fs_expect(s->reader, "flash:/scope/profile.json", "new", 3);
    fs_expect(s->reader, "flash:/scope/profile.json.bak", "old", 3);
    /* Simulate a leftover unrelated staging file; save uses its own new slot. */
    assert(bvstk_fs_writer_open(&w, s, "flash:/scope/profile.json.tmp0", false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&w, "other", 5) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_OK);
    assert(bvstk_fs_save(s, "flash:/scope/profile.json", "last", 4) == BVSTK_FS_OK);
    fs_expect(s->reader, "flash:/scope/profile.json.tmp0", "other", 5);
    fs_expect(s->reader, "flash:/scope/profile.json.bak", "new", 3);
    bvstk_fs_storage_t faulty = *s;
    fs_fault_base = s;
    faulty.write = fs_fault_write;
    faulty.finish = fs_fault_finish;
    faulty.install = fs_fault_install;
    for (unsigned fault = 0; fault < 3; ++fault) {
        fs_write_short = fault == 0;
        fs_close_failure = fault == 1;
        fs_install_failure = fault == 2;
        assert(bvstk_fs_save(&faulty, "flash:/scope/profile.json", data, sizeof(data)) == BVSTK_FS_IO);
        fs_expect(s->reader, "flash:/scope/profile.json", "last", 4);
        fs_expect(s->reader, "flash:/scope/profile.json.bak", "new", 3);
    }
    fs_write_short = fs_close_failure = fs_install_failure = 0;
    /* Writer overflow is checked before handing bytes to the backend. */
    assert(bvstk_fs_writer_open(&w, s, "sd:/limit", false) == BVSTK_FS_OK);
    w.bytes = UINT32_MAX;
    assert(bvstk_fs_writer_write(&w, "x", 1) == BVSTK_FS_RANGE);
    assert(bvstk_fs_writer_close(&w) == BVSTK_FS_RANGE && !w.handle);
    assert(bvstk_fs_remove(s, "sd:/limit") == BVSTK_FS_OK);
    /* Exercise the existing directory/TAR stream with the new adapters. */
    bvstk_fs_reader_t tree;
    uint8_t buffer[257];
    size_t total = 0, count;
    bool eof = false;
    assert(bvstk_fs_reader_open(&tree, s->reader, "flash:/scope", BVSTK_FS_TREE, BVSTK_FS_RECURSIVE) == BVSTK_FS_OK);
    while (!eof) {
        assert(bvstk_fs_reader_read(&tree, buffer, sizeof(buffer), &count, &eof) == BVSTK_FS_OK);
        total += count;
    }
    assert(total > 0);
    bvstk_fs_reader_close(&tree);
    eof = false; total = 0;
    assert(bvstk_fs_reader_open(&tree, s->reader, "flash:/scope", BVSTK_FS_TAR, 0) == BVSTK_FS_OK);
    while (!eof) {
        assert(bvstk_fs_reader_read(&tree, buffer, sizeof(buffer), &count, &eof) == BVSTK_FS_OK);
        total += count;
    }
    assert(total > sizeof(data) && !(total % 512U));
    bvstk_fs_reader_close(&tree);
    assert(bvstk_fs_remove(s, "sd:/interrupted") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "sd:/renamed.bin") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/scope/nested/данные.bin") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/scope/nested") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/scope/profile.json") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/scope/profile.json.bak") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/scope/profile.json.tmp0") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/scope") == BVSTK_FS_OK);
    assert(bvstk_fs_remove(s, "flash:/empty") == BVSTK_FS_OK);
}
#endif
