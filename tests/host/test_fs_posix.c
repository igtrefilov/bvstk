#define _POSIX_C_SOURCE 200809L
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "fs_storage_contract.h"

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static bvstk_fs_posix_t port;
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
    assert(!symlink(sd, link));
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
