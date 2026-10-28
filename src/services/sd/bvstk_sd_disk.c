#include "services/sd/bvstk_sd_disk.h"
static bvstk_status_t physical(void *context, uint32_t lba, uint8_t block[512])
{
    bvstk_sd_result_t result;
    return bvstk_sd_service_disk_read(context,lba,block,&result);
}
bvstk_status_t bvstk_sd_disk_open(bvstk_sd_disk_t *d, bvstk_sd_service_t *s)
{
    if (!d || !s) return BVSTK_ERR_MALFORMED;
    if (d->attempted) return d->ready ? BVSTK_OK : BVSTK_ERR_NOT_READY;
    d->attempted=true;d->service=s;
    bvstk_sd_result_t result;
    bvstk_status_t rc=bvstk_sd_service_disk_claim(s,&result);
    if (rc==BVSTK_OK) rc=bvstk_sd_volume_probe(physical,s,&d->volume);
    d->ready=rc==BVSTK_OK;
    return rc;
}
bvstk_status_t bvstk_sd_disk_read(bvstk_sd_disk_t *d,uint32_t sector,uint8_t *data,size_t n)
{
    uint32_t lba;
    if (!d || !d->ready) return BVSTK_ERR_NOT_READY;
    if (!data || !bvstk_sd_volume_map(&d->volume,sector,n,&lba)) return BVSTK_ERR_RANGE;
    for (size_t i=0;i<n;++i) {
        bvstk_status_t rc=physical(d->service,lba+(uint32_t)i,data+i*512);
        if (rc!=BVSTK_OK) { d->ready=false;return rc; }
    }
    return BVSTK_OK;
}
bvstk_status_t bvstk_sd_disk_write(bvstk_sd_disk_t *d,uint32_t sector,const uint8_t *data,size_t n)
{
    uint32_t lba;
    if (!d || !d->ready) return BVSTK_ERR_NOT_READY;
    if (!data || !bvstk_sd_volume_map(&d->volume,sector,n,&lba)) return BVSTK_ERR_RANGE;
    for (size_t i=0;i<n;++i) {
        bvstk_sd_result_t result;
        bvstk_status_t rc=bvstk_sd_service_disk_write(d->service,lba+(uint32_t)i,data+i*512,&result);
        if (rc!=BVSTK_OK) { d->ready=false;return rc; }
    }
    return BVSTK_OK;
}
