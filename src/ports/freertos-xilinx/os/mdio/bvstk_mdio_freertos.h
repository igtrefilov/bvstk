#ifndef BVSTK_MDIO_FREERTOS_H
#define BVSTK_MDIO_FREERTOS_H
#include "services/mdio/bvstk_mdio_service.h"
const bvstk_clock_t *bvstk_mdio_freertos_clock(void);
bvstk_status_t bvstk_mdio_freertos_start(bvstk_mdio_service_t *service);
#endif
