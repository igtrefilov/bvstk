#define _POSIX_C_SOURCE 200809L
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "fs_storage_contract.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static bvstk_fs_posix_t port;
static bool no_hard_links, fail_rename, competing_create;
int __real_link(const char *, const char *);
int __real_rename(const char *, const char *);

int __wrap_link(const char *from, const char *to)
{
    if (!no_hard_links) return __real_link(from, to);
    errno = EXDEV; /* fs-dos reports this even within one FAT volume. */
    return -1;
}

int __wrap_rename(const char *from, const char *to)
{
    if (competing_create) {
        /* Simulate a native application claiming the name at publication. */
        int fd = open(to, O_WRONLY | O_CREAT | O_EXCL, 0644);
        assert(fd < 0 && errno == EEXIST);
        competing_create = false;
    }
    if (fail_rename) { errno = EIO; return -1; }
    return __real_rename(from, to);
}

static void check_publication(const bvstk_fs_storage_t *s, const char *root)
{
    const char *path = "flash:/publish.bin";
    char native[BVSTK_FS_PATH_CAPACITY];
    assert(snprintf(native, sizeof(native), "%s/publish.bin", root) < (int)sizeof(native));
    bvstk_fs_upload_t upload = {0};
    no_hard_links = true;
    assert(bvstk_fs_upload_begin(&upload, s, path, 0, false) == BVSTK_FS_OK);
    fail_rename = true;
    assert(bvstk_fs_upload_commit(&upload) == BVSTK_FS_IO);
    fail_rename = false;
    assert(access(native, F_OK) < 0 && errno == ENOENT);

    assert(bvstk_fs_upload_begin(&upload, s, path, 3, false) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&upload, "new", 3) == BVSTK_FS_OK);
    competing_create = true;
    assert(bvstk_fs_upload_commit(&upload) == BVSTK_FS_OK && !competing_create);
    fs_expect(s->reader, path, "new", 3);
    no_hard_links = false;

    assert(!chmod(native, 04750));
    struct stat before, after;
    assert(!stat(native, &before));
    assert(bvstk_fs_upload_begin(&upload, s, path, 4, true) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_write(&upload, "edit", 4) == BVSTK_FS_OK);
    assert(bvstk_fs_upload_commit(&upload) == BVSTK_FS_OK);
    assert(!stat(native, &after));
    assert((after.st_mode & 07777) == 0750 && after.st_uid == before.st_uid &&
           after.st_gid == before.st_gid);
    fs_expect(s->reader, path, "edit", 4);
    assert(bvstk_fs_remove(s, path) == BVSTK_FS_OK);

    assert(bvstk_fs_mkdir(s, "flash:/old-dir") == BVSTK_FS_OK);
    fail_rename = true;
    assert(bvstk_fs_rename(s, "flash:/old-dir", "flash:/new-dir") == BVSTK_FS_IO);
    fail_rename = false;
    bvstk_fs_info_t info;
    assert(bvstk_fs_stat(s->reader, "flash:/new-dir", &info) == BVSTK_FS_NOT_FOUND);
    competing_create = true;
    assert(bvstk_fs_rename(s, "flash:/old-dir", "flash:/new-dir") == BVSTK_FS_OK && !competing_create);
    assert(bvstk_fs_remove(s, "flash:/new-dir") == BVSTK_FS_OK);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char flash[512], sd[512], absent[512], link[512];
    assert(snprintf(flash, sizeof(flash), "%s/flash", argv[1]) < (int)sizeof(flash));
    assert(snprintf(sd, sizeof(sd), "%s/sd", argv[1]) < (int)sizeof(sd));
    assert(snprintf(absent, sizeof(absent), "%s/absent", argv[1]) < (int)sizeof(absent));
    assert(!mkdir(argv[1], 0700));
    assert(!mkdir(flash, 0700));
    assert(!mkdir(sd, 0700));
    const bvstk_fs_posix_volume_t volumes[] = {
        {"flash", flash, false}, {"sd", sd, false}, {"sd-pl", NULL, false}
    };
    assert(bvstk_fs_posix_init(&port, volumes, 3) == BVSTK_FS_OK);
    const bvstk_fs_storage_t *s = bvstk_fs_posix_storage(&port);
    fs_storage_contract(s);
    check_publication(s, flash);
    bvstk_fs_volume_t volume;
    assert(s->reader->volume(s->reader->context, 2, &volume) == BVSTK_FS_OK && volume.state == 2);
    bvstk_fs_info_t info;
    assert(bvstk_fs_stat(s->reader, "sd-pl:/file", &info) == BVSTK_FS_UNSUPPORTED);
    assert(bvstk_fs_stat(s->reader, "unknown:/file", &info) == BVSTK_FS_NOT_FOUND);
    /* A dangling/missing root does not become a newly-created RAM filesystem. */
    strcpy(port.volumes[1].root, absent);
    assert(s->reader->volume(s->reader->context, 1, &volume) == BVSTK_FS_OK && !volume.state);
    assert(bvstk_fs_mkdir(s, "sd:/x") == BVSTK_FS_NOT_READY);
    assert(access(absent, F_OK));
    strcpy(port.volumes[1].root, sd);
    port.volumes[1].require_mount = true;
    assert(bvstk_fs_stat(s->reader, "sd:/", &info) == BVSTK_FS_NOT_READY);
    port.volumes[1].require_mount = false;
    /* Symlinks to directories or files outside the volume are rejected. */
    assert(snprintf(link, sizeof(link), "%s/link", flash) < (int)sizeof(link));
    bvstk_fs_upload_t upload = {0};
    mode_t mask = umask(0);
    assert(bvstk_fs_upload_begin(&upload, s, "flash:/link", 0, true) == BVSTK_FS_OK);
    umask(mask);
    char stage[512];
    assert(snprintf(stage, sizeof(stage), "%s%s", flash, strchr(upload.temporary, '/')) < (int)sizeof(stage));
    struct stat stage_info;
    assert(!lstat(stage, &stage_info) && (stage_info.st_mode & 0777) == 0600);
    assert(!symlink(sd, link));
    assert(bvstk_fs_upload_commit(&upload) == BVSTK_FS_DENIED);
    struct stat link_info;
    assert(!lstat(link, &link_info) && S_ISLNK(link_info.st_mode));
    assert(bvstk_fs_stat(s->reader, "flash:/link", &info) == BVSTK_FS_DENIED);
    assert(bvstk_fs_mkdir(s, "flash:/link/escape") == BVSTK_FS_DENIED);
    assert(bvstk_fs_remove(s, "flash:/link") == BVSTK_FS_DENIED);
    assert(!unlink(link));
    void *dir;
    assert(s->reader->dir_open(s->reader->context, "flash:/", &dir) == BVSTK_FS_OK);
    assert(bvstk_fs_posix_destroy(&port) == BVSTK_FS_BUSY);
    s->reader->dir_close(s->reader->context, dir);
    assert(!port.files && !port.directories);
    assert(bvstk_fs_posix_destroy(&port) == BVSTK_FS_OK);
    assert(!rmdir(flash) && !rmdir(sd) && !rmdir(argv[1]));
    puts("POSIX filesystem: common lifecycle, UTF-8, streams, failed saves, volume readiness and bounds passed");
    return 0;
}
