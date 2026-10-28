#include "apps/freertos/services/http/http_ui.h"
#include "apps/freertos/storage/fs/fs_dcp2_backend.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "shared/interfaces/bvstk_clock.h"
#include <stdio.h>
#include <string.h>

static unsigned fat_bits(const FATFS *fs)
{
    if (!fs) return 0;
    switch (fs->fs_type) {
    case FS_FAT12: return 12U;
    case FS_FAT16: return 16U;
    case FS_FAT32: return 32U;
    /* fat_bits is reserved for FAT12/16/32; exFAT is not FAT64. */
    case FS_EXFAT: return 0U;
    default: return 0U;
    }
}
void http_freertos_volumes(int fd)
{
    static struct { uint64_t total, free, at; unsigned fat_bits; int valid; } cached[3];
    static const char *const names[] = {"flash", "sd", "sd-pl"};
    uint64_t now = bvstk_platform_now_ms();
    http_ui_header(fd, "application/json", -1); http_ui_text(fd, "{\"volumes\":[");
    for (unsigned i = 0; i < 3U; ++i) {
        const fs_device_info_t *dev = fs_device_by_name(names[i]);
        int ready = dev && fs_shared_is_ready(dev->ctx);
        unsigned fat = cached[i].fat_bits;
        if (!ready) { cached[i].valid = 0; cached[i].fat_bits = 0; }
        if (ready && i < 2U && (!cached[i].valid || now - cached[i].at > 30000U) && http_ui_lock(dev->ctx)) {
            char drive[3] = {dev->ctx->root[0], ':', 0}; FATFS *fs = NULL; DWORD free_clusters;
            cached[i].valid = 0;
            cached[i].fat_bits = 0;
            if (f_getfree(drive, &free_clusters, &fs) == FR_OK && fs && fs->n_fatent >= 2U) {
                cached[i].total = (uint64_t)(fs->n_fatent - 2U) * fs->csize * 512U;
                cached[i].free = (uint64_t)free_clusters * fs->csize * 512U;
                cached[i].fat_bits = fat_bits(fs);
                cached[i].valid = 1; cached[i].at = now;
            }
            http_ui_unlock(dev->ctx);
        }
        if (i == 2U && ready) {
            const bvstk_sd_volume_t *v = bvstk_sd_pl_volume();
            if (v) { cached[i].total = (uint64_t)v->sectors * 512U; cached[i].fat_bits = v->fat_bits; fat = cached[i].fat_bits; }
        }
        fat = cached[i].fat_bits;
        http_ui_printf(fd, "%s{\"name\":\"%s\",\"ready\":%s,\"fat_bits\":%u,\"total_bytes\":",
            i ? "," : "", names[i], ready ? "true" : "false", fat);
        if (ready && (cached[i].valid || i == 2U)) http_ui_printf(fd, "%llu", (unsigned long long)cached[i].total);
        else http_ui_text(fd, "null");
        http_ui_text(fd, ",\"free_bytes\":");
        if (ready && cached[i].valid && i < 2U) http_ui_printf(fd, "%llu", (unsigned long long)cached[i].free);
        else http_ui_text(fd, "null");
        http_ui_text(fd, ",\"format_allowed\":false}");
    }
    http_ui_text(fd, "]}\n");
}
void http_freertos_sd_info(int fd)
{
    bvstk_sd_card_info_t info; bvstk_sd_service_t *s = bvstk_sd_master_freertos_service();
    bvstk_status_t st = bvstk_sd_service_info(s, &info);
    if (st != BVSTK_OK) { http_ui_status(fd, st); return; }
    http_ui_header(fd, "application/json", -1);
    http_ui_printf(fd, "{\"ready\":%s,\"mounted\":%s,\"filesystem_owned\":%s,\"ocr\":%lu,\"block_addressed\":%s,\"last_lba\":%lu,\"last_error\":",
        info.ready ? "true" : "false", bvstk_sd_pl_is_ready() ? "true" : "false",
        info.filesystem_owned ? "true" : "false", (unsigned long)info.ocr,
        info.block_addressed ? "true" : "false", (unsigned long)info.last.lba);
    http_ui_string(fd, bvstk_status_string(info.last.status));
    const bvstk_sd_volume_t *v = bvstk_sd_pl_volume();
    if (v) http_ui_printf(fd, ",\"fat_bits\":%u,\"volume_lba\":%lu,\"volume_sectors\":%lu",
        (unsigned)v->fat_bits, (unsigned long)v->first_lba, (unsigned long)v->sectors);
    http_ui_text(fd, ",\"format_allowed\":false}\n");
}
