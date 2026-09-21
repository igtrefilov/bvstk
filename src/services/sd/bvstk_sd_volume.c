#include "services/sd/bvstk_sd_volume.h"

#include <string.h>

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool signature(const uint8_t *p)
{
    return p[510] == 0x55 && p[511] == 0xaa;
}

static bool bpb(const uint8_t *p, uint32_t start, uint32_t limit,
    bvstk_sd_volume_t *out)
{
    uint32_t total = le16(p + 19), fat = le16(p + 22);
    uint32_t reserved = le16(p + 14), roots = le16(p + 17);
    uint32_t spc = p[13], fats = p[16], clusters;
    uint64_t overhead, fat_bytes;
    uint8_t bits;
    if (!signature(p) || (p[0] != 0xeb && p[0] != 0xe9) ||
        le16(p + 11) != 512 || !reserved || !spc || spc > 128 ||
        (spc & (spc - 1)) || (fats != 1 && fats != 2) || roots % 16)
        return false;
    if (!total) total = le32(p + 32);
    if (!fat) fat = le32(p + 36);
    if (!total || !fat || (limit && total > limit) ||
        (uint64_t)start + total > UINT64_C(0x100000000)) return false;
    overhead = reserved + (uint64_t)fats * fat + roots / 16;
    if (overhead >= total) return false;
    clusters = (uint32_t)((total - overhead) / spc);
    if (!clusters || clusters > UINT32_C(0x0ffffff5) - 2) return false;
    bits = clusters < 4085 ? 12 : (clusters < 65525 ? 16 : 32);
    if (bits == 32) {
        uint32_t root = le32(p + 44);
        uint16_t fsinfo = le16(p + 48), backup = le16(p + 50);
        if (roots || le16(p + 22) || le16(p + 42) || root < 2 ||
            root >= clusters + 2 || (le16(p + 40) & 0x80) ||
            (fsinfo != 0xffff && (!fsinfo || fsinfo >= reserved)) ||
            (backup != 0xffff && backup && backup >= reserved)) return false;
    } else if (!roots || !le16(p + 22)) return false;
    fat_bytes = ((uint64_t)(clusters + 2) * bits + 7) / 8;
    if (fat_bytes > (uint64_t)fat * 512) return false;
    out->first_lba = start;
    out->sectors = total;
    out->fat_bits = bits;
    return true;
}

bvstk_status_t bvstk_sd_volume_probe(bvstk_sd_volume_read_t read, void *context,
    bvstk_sd_volume_t *volume)
{
    uint8_t sector[512], entries[64];
    bvstk_status_t status;
    unsigned i;
    if (!read || !volume) return BVSTK_ERR_MALFORMED;
    memset(volume, 0, sizeof(*volume));
    status = read(context, 0, sector);
    if (status != BVSTK_OK) return status;
    if (bpb(sector, 0, 0, volume)) return BVSTK_OK;
    if (!signature(sector)) return BVSTK_ERR_UNSUPPORTED;
    memcpy(entries, sector + 446, sizeof(entries));
    for (i = 0; i < 4; ++i) {
        const uint8_t *entry = entries + i * 16;
        uint32_t start = le32(entry + 8), count = le32(entry + 12);
        switch (entry[4]) {
        case 0x01: case 0x04: case 0x06: case 0x0b: case 0x0c: case 0x0e: break;
        default: continue;
        }
        if ((entry[0] != 0 && entry[0] != 0x80) || !start || !count ||
            (uint64_t)start + count > UINT64_C(0x100000000)) continue;
        status = read(context, start, sector);
        if (status != BVSTK_OK) return status;
        if (bpb(sector, start, count, volume)) return BVSTK_OK;
    }
    return BVSTK_ERR_UNSUPPORTED;
}

bool bvstk_sd_volume_map(const bvstk_sd_volume_t *volume, uint32_t sector,
    size_t count, uint32_t *physical_lba)
{
    if (!volume || !physical_lba || !count || count > SIZE_MAX / 512 ||
        sector >= volume->sectors || count > volume->sectors - sector ||
        (uint64_t)volume->first_lba + sector + count > UINT64_C(0x100000000))
        return false;
    *physical_lba = volume->first_lba + sector;
    return true;
}
