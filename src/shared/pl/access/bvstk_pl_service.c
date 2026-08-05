#include "shared/pl/access/bvstk_pl_service.h"

#include <errno.h>
#include <string.h>

static bvstk_status_t ensure_mapped(bvstk_pl_service_t *service,
                                    bvstk_pl_region_id_t id)
{
    const bvstk_pl_region_desc_t *desc;

    if (service == NULL || (unsigned int)id >= (unsigned int)BVSTK_PL_REGION_COUNT) {
        return BVSTK_ERR_MALFORMED;
    }
    if (service->mapped[id]) {
        return BVSTK_OK;
    }

    desc = bvstk_pl_region_get(id);
    if (desc == NULL) {
        return BVSTK_ERR_NOT_FOUND;
    }
    if (bvstk_mmio_region_open(&service->mappings[id],
                               desc->physical_base,
                               desc->size) != 0) {
        return errno == EACCES || errno == EPERM ? BVSTK_ERR_DENIED : BVSTK_ERR_IO;
    }
    service->mapped[id] = 1U;
    return BVSTK_OK;
}

void bvstk_pl_service_init(bvstk_pl_service_t *service)
{
    if (service != NULL) {
        memset(service, 0, sizeof(*service));
    }
}

void bvstk_pl_service_shutdown(bvstk_pl_service_t *service)
{
    size_t i;

    if (service == NULL) {
        return;
    }
    for (i = 0; i < BVSTK_PL_REGION_COUNT; ++i) {
        if (service->mapped[i]) {
            bvstk_mmio_region_close(&service->mappings[i]);
        }
    }
    memset(service, 0, sizeof(*service));
}

bvstk_status_t bvstk_pl_service_read32(bvstk_pl_service_t *service,
                                       bvstk_pl_region_id_t region,
                                       size_t offset,
                                       uint32_t *value)
{
    bvstk_status_t status = ensure_mapped(service, region);

    if (status != BVSTK_OK) {
        return status;
    }
    if (value == NULL) {
        return BVSTK_ERR_MALFORMED;
    }
    if (bvstk_mmio_read32(&service->mappings[region], offset, value) != 0) {
        return errno == EINVAL ? BVSTK_ERR_RANGE : BVSTK_ERR_IO;
    }
    return BVSTK_OK;
}

bvstk_status_t bvstk_pl_service_write32(bvstk_pl_service_t *service,
                                        bvstk_pl_region_id_t region,
                                        size_t offset,
                                        uint32_t value)
{
    bvstk_status_t status = ensure_mapped(service, region);

    if (status != BVSTK_OK) {
        return status;
    }
    if (bvstk_mmio_write32(&service->mappings[region], offset, value) != 0) {
        return errno == EINVAL ? BVSTK_ERR_RANGE : BVSTK_ERR_IO;
    }
    return BVSTK_OK;
}
