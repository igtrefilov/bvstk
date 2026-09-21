#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "services/sd/bvstk_sd_volume.h"

static uint8_t mbr[512], boot[512];
static uint32_t boot_lba;
static unsigned reads;
static bool fail_read;

static void put16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
static void put32(uint8_t *p, uint32_t n) { put16(p, (uint16_t)n); put16(p + 2, (uint16_t)(n >> 16)); }

static bvstk_status_t read_sector(void *context, uint32_t lba, uint8_t out[512])
{
    (void)context;
    ++reads;
    if (fail_read) return BVSTK_ERR_IO;
    if (lba == boot_lba) memcpy(out, boot, 512);
    else if (!lba) memcpy(out, mbr, 512);
    else return BVSTK_ERR_IO;
    return BVSTK_OK;
}

static void setup(unsigned bits, uint32_t start)
{
    memset(mbr, 0, sizeof(mbr));
    memset(boot, 0, sizeof(boot));
    boot_lba = start; reads = 0; fail_read = false;
    boot[0] = 0xeb; put16(boot + 11, 512); boot[16] = 1;
    boot[510] = mbr[510] = 0x55; boot[511] = mbr[511] = 0xaa;
    if (bits == 32) {
        boot[13] = 8; put16(boot + 14, 45);
        put32(boot + 32, 739328); put32(boot + 36, 723);
        put32(boot + 44, 2); put16(boot + 48, 1); put16(boot + 50, 6);
    } else {
        boot[13] = 1; put16(boot + 14, 1); put16(boot + 17, 512);
        put16(boot + 19, bits == 16 ? 16384 : 2048);
        put16(boot + 22, bits == 16 ? 64 : 6);
    }
    if (start) {
        mbr[450] = bits == 32 ? 0x0c : 0x06;
        put32(mbr + 454, start); put32(mbr + 458, 739328);
    }
}

int main(void)
{
    bvstk_sd_volume_t v;
    uint32_t lba;
    unsigned i;
    const unsigned bits[] = {12, 16, 32};
    for (i = 0; i < 3; ++i) {
        setup(bits[i], 0);
        assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_OK);
        assert(v.first_lba == 0 && v.fat_bits == bits[i] && reads == 1);
        setup(bits[i], 2048);
        assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_OK);
        assert(v.first_lba == 2048 && v.fat_bits == bits[i] && reads == 2);
        assert(bvstk_sd_volume_map(&v, 0, 1, &lba) && lba == 2048);
        assert(bvstk_sd_volume_map(&v, v.sectors - 2, 2, &lba));
        assert(!bvstk_sd_volume_map(&v, v.sectors - 2, 3, &lba));
        assert(!bvstk_sd_volume_map(&v, v.sectors, 1, &lba));
        assert(!bvstk_sd_volume_map(&v, UINT32_MAX, 1, &lba));
        assert(!bvstk_sd_volume_map(&v, 0, 0, &lba));
        assert(!bvstk_sd_volume_map(&v, 0, SIZE_MAX, &lba));
    }
    setup(32, 2048); put32(mbr + 458, 100); /* BPB exceeds partition. */
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    assert(!v.sectors);
    setup(32, UINT32_MAX - 100); /* Partition wraps 32-bit LBA space. */
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    assert(reads == 1);
    setup(32, 2048); mbr[511] = 0;
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 2048); mbr[450] = 0xee; /* GPT unsupported, no guessing. */
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); put16(boot + 11, 4096);
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); boot[13] = 3;
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); put32(boot + 36, 1); /* FAT cannot hold all clusters. */
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); put32(boot + 36, UINT32_MAX); /* Metadata arithmetic overflow. */
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); put32(boot + 44, 1);
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); put16(boot + 48, 50);
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    setup(32, 0); fail_read = true;
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_IO && !v.sectors);
    setup(32, 0); memset(boot, 0, sizeof(boot));
    assert(bvstk_sd_volume_probe(read_sector, NULL, &v) == BVSTK_ERR_UNSUPPORTED);
    assert(bvstk_sd_volume_probe(NULL, NULL, &v) == BVSTK_ERR_MALFORMED);
    puts("SD existing-volume discovery/bounds tests passed");
    return 0;
}
