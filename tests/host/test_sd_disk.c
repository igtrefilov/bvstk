#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"

static bvstk_sd_service_t service;
static uint8_t boot[512], mbr[512];
static unsigned claims, reads, writes;
static uint32_t last_lba;
static bool fail_init, fail_read, fail_write;

int xil_printf(const char *format, ...) { (void)format; return 0; }
bvstk_sd_service_t *bvstk_sd_master_freertos_service(void) { return &service; }
bvstk_status_t bvstk_sd_service_disk_claim(bvstk_sd_service_t *s, bvstk_sd_result_t *r)
{
    assert(s == &service); ++claims; memset(r, 0, sizeof(*r));
    return fail_init ? BVSTK_ERR_TIMEOUT : BVSTK_OK;
}
bvstk_status_t bvstk_sd_service_disk_read(bvstk_sd_service_t *s, uint32_t lba,
    uint8_t out[512], bvstk_sd_result_t *r)
{
    assert(s == &service); ++reads; last_lba = lba; memset(r, 0, sizeof(*r));
    if (fail_read) return BVSTK_ERR_IO;
    memcpy(out, lba == 0 ? mbr : boot, 512);
    return BVSTK_OK;
}
bvstk_status_t bvstk_sd_service_disk_write(bvstk_sd_service_t *s, uint32_t lba,
    const uint8_t in[512], bvstk_sd_result_t *r)
{
    assert(s == &service && in); ++writes; last_lba = lba; memset(r, 0, sizeof(*r));
    return fail_write ? BVSTK_ERR_IO : BVSTK_OK;
}

int main(int argc, char **argv)
{
    uint8_t data[1536] = {0};
    uint32_t count = 123;
    unsigned before;
    assert(argc == 2);
    /* FAT16 BPB at physical LBA 2048, inside a 16384-sector partition. */
    boot[0] = 0xeb; boot[12] = 2; boot[13] = 1; boot[14] = 1;
    boot[16] = 1; boot[18] = 2; boot[20] = 0x40; boot[22] = 64;
    mbr[450] = 0x06; mbr[455] = 8; mbr[459] = 0x40;
    boot[510] = mbr[510] = 0x55; boot[511] = mbr[511] = 0xaa;
    fail_init = !strcmp(argv[1], "init-error");
    if (!strcmp(argv[1], "no-filesystem")) memset(boot, 0, sizeof(boot));
    if (fail_init || !strcmp(argv[1], "no-filesystem")) {
        assert(bvstk_sd_pl_initialize() != BVSTK_OK);
        assert(!bvstk_sd_pl_is_ready() && !bvstk_sd_pl_volume() && writes == 0);
        fail_init = false;
        assert(bvstk_sd_pl_initialize() == BVSTK_ERR_NOT_READY && claims == 1);
        assert(bvstk_sd_pl_write(0, data, 1) == BVSTK_ERR_NOT_READY && !writes);
        return 0;
    }
    assert(bvstk_sd_pl_initialize() == BVSTK_OK && claims == 1 && reads == 2 && !writes);
    assert(bvstk_sd_pl_volume()->first_lba == 2048 && bvstk_sd_pl_volume()->sectors == 16384);
    assert(bvstk_sd_pl_initialize() == BVSTK_OK && claims == 1 && reads == 2);
    assert(bvstk_sd_pl_get_sector_count(&count) == BVSTK_ERR_UNSUPPORTED && count == 123);
    assert(bvstk_sd_pl_read(5, data, 3) == BVSTK_OK && last_lba == 2055 && reads == 5);
    assert(bvstk_sd_pl_write(5, data, 3) == BVSTK_OK && last_lba == 2055 && writes == 3);
    before = reads + writes;
    assert(bvstk_sd_pl_read(16383, data, 2) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_pl_write(UINT32_MAX, data, 3) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_pl_write(0, NULL, 1) == BVSTK_ERR_RANGE);
    assert(bvstk_sd_pl_write(0, data, 0) == BVSTK_ERR_RANGE);
    assert(reads + writes == before);
    if (!strcmp(argv[1], "write-error")) {
        fail_write = true;
        assert(bvstk_sd_pl_write(0, data, 3) == BVSTK_ERR_IO && writes == 4);
    } else {
        fail_read = true;
        assert(bvstk_sd_pl_read(0, data, 3) == BVSTK_ERR_IO && reads == 6);
    }
    before = reads + writes;
    assert(!bvstk_sd_pl_is_ready() && !bvstk_sd_pl_volume());
    assert(bvstk_sd_pl_initialize() == BVSTK_ERR_NOT_READY && claims == 1);
    assert(bvstk_sd_pl_read(0, data, 1) == BVSTK_ERR_NOT_READY);
    assert(bvstk_sd_pl_write(0, data, 1) == BVSTK_ERR_NOT_READY);
    assert(reads + writes == before); /* No retry, no new-card/stale-handle access. */
    puts("SD disk adapter mapping/error-latch tests passed");
    return 0;
}
