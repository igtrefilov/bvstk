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

static bvstk_fs_result_t fs_fail_publish(void *context, const char *temporary,
    const char *destination, bool overwrite, bool *consumed)
{
    (void)context;
    (void)temporary;
    (void)destination;
    (void)overwrite;
    *consumed = false;
    return BVSTK_FS_IO;
}

static bvstk_fs_upload_t fs_next_upload;
static bvstk_fs_result_t fs_fail_after_publish(void *context, const char *temporary,
    const char *destination, bool overwrite, bool *consumed)
{
    assert(fs_fault_base->publish(context, temporary, destination, overwrite, consumed) == BVSTK_FS_OK);
    assert(*consumed);
    /* A second upload can take the released slot before the caller sees the
     * result of the first one, even if a late backup cleanup reports I/O. */
    assert(bvstk_fs_upload_begin(&fs_next_upload, fs_fault_base, "sd:/next", 1, false) == BVSTK_FS_OK);
    assert(!strcmp(fs_next_upload.temporary, temporary));
    assert(bvstk_fs_upload_write(&fs_next_upload, "x", 1) == BVSTK_FS_OK);
    return BVSTK_FS_IO;
}

static void fs_expect_private_stage(const bvstk_fs_backend_t *b, const char *stage)
{
    bvstk_fs_reader_t reader;
    uint8_t buffer[1025];
    static const uint8_t empty_tar[1024] = {0};
    bool eof;
    size_t count;
    assert(bvstk_fs_reader_open(&reader, b, "sd:/", BVSTK_FS_TREE, BVSTK_FS_RECURSIVE) == BVSTK_FS_OK);
    assert(bvstk_fs_reader_read(&reader, buffer, sizeof(buffer), &count, &eof) == BVSTK_FS_OK);
    assert(eof && count == 0);
    bvstk_fs_reader_close(&reader);
    assert(bvstk_fs_reader_open(&reader, b, "sd:/", BVSTK_FS_TAR, 0) == BVSTK_FS_OK);
    assert(bvstk_fs_reader_read(&reader, buffer, sizeof(buffer), &count, &eof) == BVSTK_FS_OK);
    assert(eof && count == sizeof(empty_tar) && !memcmp(buffer, empty_tar, count));
    bvstk_fs_reader_close(&reader);

    assert(bvstk_fs_reader_open(&reader, b, stage, BVSTK_FS_FILE, 0) == BVSTK_FS_DENIED);
    assert(!reader.file);
    void *handle = &reader;
    uint64_t size;
    assert(b->file_open(b->context, stage, &handle, &size) == BVSTK_FS_DENIED);
    assert(!handle);
}

static void fs_upload_contract(const bvstk_fs_storage_t *s)
{
    const char *path = "sd:/upload.txt";
    bvstk_fs_upload_t first = {0}, second = {0};
    bvstk_fs_info_t info;
    bvstk_fs_writer_t writer;
    assert(bvstk_fs_upload_supported(s));
    assert(bvstk_fs_upload_begin(&first, s, path, (uint64_t)UINT32_MAX + 1, false) == BVSTK_FS_RANGE);
    assert(bvstk_fs_upload_begin(&first, s, "sd:/missing/target", 0, false) == BVSTK_FS_NOT_FOUND);
    assert(bvstk_fs_upload_begin(&first, s, path, 3, false) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&first, "four", 4) == BVSTK_FS_RANGE);
    assert(bvstk_fs_upload_write(&first, "o", 1) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&first, "ne", 2) == BVSTK_FS_OK);
    assert(bvstk_fs_stat(s->reader, path, &info) == BVSTK_FS_NOT_FOUND);
    fs_expect_private_stage(s->reader, first.temporary);

    /* Both clients observed a missing target. Only the first commit wins. */
    assert(bvstk_fs_upload_begin(&second, s, path, 3, false) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&second, "two", 3) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&second) == BVSTK_FS_EXISTS);
    fs_expect(s->reader, path, "one", 3);

    assert(bvstk_fs_upload_begin(&first, s, path, 3, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&first, "X", 1) == BVSTK_FS_OK);
    fs_expect(s->reader, path, "one", 3);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_RANGE);
    fs_expect(s->reader, path, "one", 3);
    assert(bvstk_fs_upload_begin(&first, s, path, 2, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&first, "no", 2) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_abort(&first) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_abort(&first) == BVSTK_FS_OK);
    fs_expect(s->reader, path, "one", 3);

    bvstk_fs_storage_t faulty = *s;
    fs_fault_base = s;
    faulty.write = fs_fault_write;
    faulty.finish = fs_fault_finish;
    fs_write_short = 1;
    assert(bvstk_fs_upload_begin(&first, &faulty, path, 3, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&first, "bad", 3) == BVSTK_FS_IO);
    fs_write_short = 0;
    assert(bvstk_fs_upload_write(&first, "d", 1) == BVSTK_FS_IO);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_IO);
    fs_expect(s->reader, path, "one", 3);
    assert(bvstk_fs_upload_begin(&first, &faulty, path, 0, true) == BVSTK_FS_OK);
    fs_close_failure = 1;
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_IO);
    fs_close_failure = 0;
    fs_expect(s->reader, path, "one", 3);
    faulty.publish = fs_fail_publish;
    assert(bvstk_fs_upload_begin(&first, &faulty, path, 0, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_IO);
    fs_expect(s->reader, path, "one", 3);
    faulty.publish = fs_fail_after_publish;
    assert(bvstk_fs_upload_begin(&first, &faulty, path, 1, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&first, "y", 1) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_IO);
    fs_expect(s->reader, path, "y", 1);
    assert(bvstk_fs_upload_commit(&fs_next_upload) == BVSTK_FS_OK);
    fs_expect(s->reader, "sd:/next", "x", 1);
    assert(bvstk_fs_remove(s, "sd:/next") == BVSTK_FS_OK);
    assert(bvstk_fs_upload_begin(&first, s, path, 0, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_OK);
    fs_expect(s->reader, path, "", 0);
    assert(bvstk_fs_remove(s, path) == BVSTK_FS_OK);

    /* A normal writer may create the path between BEGIN and COMMIT. */
    assert(bvstk_fs_upload_begin(&first, s, path, 0, false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_open(&writer, s, path, false) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_write(&writer, "other", 5) == BVSTK_FS_OK);
    assert(bvstk_fs_writer_close(&writer) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_EXISTS);
    fs_expect(s->reader, path, "other", 5);

    const char *reserved[] = {"sd:/bVu00000.tMp", "sd:/BVU00015.oLd"};
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i) {
        assert(bvstk_fs_remove(s, reserved[i]) == BVSTK_FS_DENIED);
        assert(bvstk_fs_mkdir(s, reserved[i]) == BVSTK_FS_DENIED);
        assert(bvstk_fs_rename(s, path, reserved[i]) == BVSTK_FS_DENIED);
        assert(bvstk_fs_rename(s, reserved[i], path) == BVSTK_FS_DENIED);
        assert(bvstk_fs_writer_open(&writer, s, reserved[i], true) == BVSTK_FS_DENIED);
        assert(bvstk_fs_writer_append(&writer, s, reserved[i]) == BVSTK_FS_DENIED);
        assert(bvstk_fs_upload_begin(&first, s, reserved[i], 0, true) == BVSTK_FS_DENIED);
    }
    assert(bvstk_fs_remove(s, path) == BVSTK_FS_OK);

    /* Moving the parent during an upload cannot strand its staging file. */
    assert(bvstk_fs_mkdir(s, "sd:/parent") == BVSTK_FS_OK);
    assert(bvstk_fs_upload_begin(&first, s, "sd:/parent/new", 0, false) == BVSTK_FS_OK);
    assert(bvstk_fs_rename(s, "sd:/parent", "sd:/moved") == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&first) == BVSTK_FS_NOT_FOUND);
    assert(bvstk_fs_remove(s, "sd:/moved") == BVSTK_FS_OK);
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
    fs_upload_contract(s);
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
    assert(bvstk_fs_rename(s, "flash:/scope", "flash:/scope") == BVSTK_FS_EXISTS);
    assert(bvstk_fs_rename(s, "flash:/scope", "flash:/scope/nested/loop") == BVSTK_FS_INVALID);
    assert(bvstk_fs_mkdir(s, "flash:/occupied") == BVSTK_FS_OK);
    assert(bvstk_fs_rename(s, "flash:/scope", "flash:/occupied") == BVSTK_FS_EXISTS);
    assert(bvstk_fs_remove(s, "flash:/occupied") == BVSTK_FS_OK);
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
