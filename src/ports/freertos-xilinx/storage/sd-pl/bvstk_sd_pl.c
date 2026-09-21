#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "xil_printf.h"

/* Called through FatFs under its volume lock, after the port startup task.
 * Present only the selected existing FAT volume, at logical sector zero.
 * Never expose the MBR/other partitions to filesystem writes. */
static bvstk_sd_volume_t s_volume;
static bool s_attempted;
static volatile int s_ready;

static bvstk_status_t report_error(bvstk_status_t status, const bvstk_sd_result_t *r)
{
    if (status != BVSTK_OK) {
        s_ready = 0;
        xil_printf("SD-PL: I/O stopped: %s LBA=%lu stage=%s; restart required\r\n",
            bvstk_status_string(status), (unsigned long)r->lba,
            r->hardware.stage ? r->hardware.stage : "unknown");
    }
    return status;
}

static bvstk_status_t read_physical(void *context, uint32_t lba, uint8_t sector[512])
{
    bvstk_sd_result_t result;
    bvstk_status_t status = bvstk_sd_service_disk_read(context, lba, sector, &result);
    return report_error(status, &result);
}

bvstk_status_t bvstk_sd_pl_initialize(void)
{
    bvstk_sd_service_t *service = bvstk_sd_master_freertos_service();
    bvstk_sd_result_t result;
    bvstk_status_t status;
    if (s_attempted) return s_ready ? BVSTK_OK : BVSTK_ERR_NOT_READY;
    s_attempted = true;
    status = bvstk_sd_service_disk_claim(service, &result);
    if (report_error(status, &result) != BVSTK_OK) return status;
    status = bvstk_sd_volume_probe(read_physical, service, &s_volume);
    if (status != BVSTK_OK) {
        xil_printf("SD-PL: no usable FAT12/16/32 volume (%s); media left unchanged\r\n",
            bvstk_status_string(status));
        return status;
    }
    s_ready = 1;
    xil_printf("SD-PL: existing FAT%u volume LBA=%lu sectors=%lu; no formatting\r\n",
        s_volume.fat_bits, (unsigned long)s_volume.first_lba,
        (unsigned long)s_volume.sectors);
    return BVSTK_OK;
}

void bvstk_sd_pl_shutdown(void)
{
    /* Do not release ownership or permit silent reinitialization. */
    s_ready = 0;
    s_attempted = true;
}

int bvstk_sd_pl_is_ready(void) { return s_ready; }

const bvstk_sd_volume_t *bvstk_sd_pl_volume(void)
{
    return s_ready ? &s_volume : NULL;
}

bvstk_status_t bvstk_sd_pl_get_sector_count(uint32_t *sector_count)
{
    /* Physical capacity is not available in the qualified black-box API.
     * This ioctl is for mkfs, not normal FatFs I/O. Do not invent capacity. */
    if (!sector_count) return BVSTK_ERR_MALFORMED;
    return BVSTK_ERR_UNSUPPORTED;
}

bvstk_status_t bvstk_sd_pl_read(uint32_t first_sector, uint8_t *buffer, size_t count)
{
    uint32_t lba;
    size_t i;
    if (!s_ready) return BVSTK_ERR_NOT_READY;
    if (!buffer || !bvstk_sd_volume_map(&s_volume, first_sector, count, &lba))
        return BVSTK_ERR_RANGE;
    for (i = 0; i < count; ++i) {
        bvstk_status_t status = read_physical(bvstk_sd_master_freertos_service(),
            lba + (uint32_t)i, buffer + i * 512);
        if (status != BVSTK_OK) return status;
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_sd_pl_write(uint32_t first_sector, const uint8_t *buffer, size_t count)
{
    uint32_t lba;
    size_t i;
    if (!s_ready) return BVSTK_ERR_NOT_READY;
    if (!buffer || !bvstk_sd_volume_map(&s_volume, first_sector, count, &lba))
        return BVSTK_ERR_RANGE;
    for (i = 0; i < count; ++i) {
        bvstk_sd_result_t result;
        bvstk_status_t status = bvstk_sd_service_disk_write(
            bvstk_sd_master_freertos_service(), lba + (uint32_t)i,
            buffer + i * 512, &result);
        if (report_error(status, &result) != BVSTK_OK) return status;
    }
    /* Synchronous verified writes: CTRL_SYNC needs no additional command. */
    return BVSTK_OK;
}
