/* Run on Neutrino against the real mounted QSPI and PS SD volumes.
 * Every mutation is confined to newly-created, process-specific directories. */
#define _POSIX_C_SOURCE 200809L
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "host/fs_storage_contract.h"
#include <unistd.h>

int main(void)
{
    const bvstk_fs_storage_t *native = bvstk_fs_posix_default();
    assert(native);
    for (size_t i = 0; i < native->reader->volume_count(native->reader->context); ++i) {
        bvstk_fs_volume_t v;
        assert(native->reader->volume(native->reader->context, i, &v) == BVSTK_FS_OK);
        printf("volume %s state=%u\n", v.name, (unsigned)v.state);
        assert(v.state == (!strcmp(v.name, "sd-pl") ? 2 : 1));
    }
    char flash[128], sd[128], flash_path[128], sd_path[128];
    assert(snprintf(flash, sizeof(flash), "/flash/.bvstk-stage5-%lu", (unsigned long)getpid()) < (int)sizeof(flash));
    assert(snprintf(sd, sizeof(sd), "/sd/.bvstk-stage5-%lu", (unsigned long)getpid()) < (int)sizeof(sd));
    assert(snprintf(flash_path, sizeof(flash_path), "flash:%s", flash + 6) < (int)sizeof(flash_path));
    assert(snprintf(sd_path, sizeof(sd_path), "sd:%s", sd + 3) < (int)sizeof(sd_path));
    printf("isolated roots: %s %s\n", flash, sd);
    fflush(stdout);
    assert(bvstk_fs_mkdir(native, flash_path) == BVSTK_FS_OK);
    printf("created %s\n", flash); fflush(stdout);
    assert(bvstk_fs_mkdir(native, sd_path) == BVSTK_FS_OK);
    printf("created %s\n", sd); fflush(stdout);
    static bvstk_fs_posix_t port;
    const bvstk_fs_posix_volume_t roots[] = {
        {"flash", flash, false}, {"sd", sd, false}, {"sd-pl", NULL, false}
    };
    assert(bvstk_fs_posix_init(&port, roots, 3) == BVSTK_FS_OK);
    fs_storage_contract(bvstk_fs_posix_storage(&port));
    assert(!port.files && !port.directories);
    assert(bvstk_fs_posix_destroy(&port) == BVSTK_FS_OK);
    assert(bvstk_fs_remove(native, flash_path) == BVSTK_FS_OK);
    assert(bvstk_fs_remove(native, sd_path) == BVSTK_FS_OK);
    puts("PASS: QSPI/PS-SD filesystem contract, UTF-8, cross-volume copy, streams, partial writes, backups and cleanup");
    return 0;
}
