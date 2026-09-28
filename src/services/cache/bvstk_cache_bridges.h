#ifndef BVSTK_CACHE_BRIDGES_H
#define BVSTK_CACHE_BRIDGES_H
#include "services/cache/bvstk_cache.h"
#include "services/i2c/bvstk_i2c_master_service.h"
#include "services/mdio/bvstk_mdio_service.h"

/* Bus adapters are separate from the generic registry and DCP2 wire service.
 * Attach after service initialization, before making the service available. */
typedef struct {
    bvstk_cache_registry_t *registry;
    bvstk_i2c_master_service_t *service;
    uint16_t handles[I2C_CFG_MAX_DEVICES];
    bvstk_status_t status;
} bvstk_cache_i2c_bridge_t;
typedef struct {
    bvstk_cache_registry_t *registry;
    bvstk_mdio_service_t *service;
    uint16_t handles[MDIO_CFG_MAX_DEVICES];
    bvstk_status_t status;
} bvstk_cache_mdio_bridge_t;

bvstk_status_t bvstk_cache_attach_i2c(bvstk_cache_i2c_bridge_t *, bvstk_cache_registry_t *,
    bvstk_i2c_master_service_t *, const i2c_device_config_t *);
bvstk_status_t bvstk_cache_attach_mdio(bvstk_cache_mdio_bridge_t *, bvstk_cache_registry_t *,
    bvstk_mdio_service_t *);
#endif
