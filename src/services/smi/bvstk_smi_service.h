#ifndef BVSTK_SMI_SERVICE_H
#define BVSTK_SMI_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "drivers/pl/smi/bvstk_smi_core.h"
#include "shared/config/bvstk_config_model.h"
#include "shared/events/bvstk_event.h"

typedef struct {
    void *context;
    bvstk_status_t (*read)(void *context,
                           uint8_t phy_addr,
                           uint8_t reg,
                           uint16_t *value,
                           uint32_t timeout_ms);
    bvstk_status_t (*write)(void *context,
                            uint8_t phy_addr,
                            uint8_t reg,
                            uint16_t value,
                            bvstk_event_source_t source,
                            uint32_t timeout_ms);
} bvstk_smi_bus_ops_t;

typedef struct {
    const char *name;
    uint8_t phy_addr;
    uint8_t reg_count;
    smi_policy_t policy;
} bvstk_smi_device_info_t;

typedef struct {
    bvstk_smi_core_t *core;
    bvstk_smi_bus_ops_t bus;
    bvstk_event_sink_t events;
    smi_phy_config_t devices[SMI_CFG_MAX_DEVICES];
    uint16_t cache[SMI_CFG_MAX_DEVICES][32];
    size_t device_count;
    uint8_t initialized;
} bvstk_smi_service_t;

bvstk_status_t bvstk_smi_service_init(bvstk_smi_service_t *service,
                                      bvstk_smi_core_t *core,
                                      const bvstk_smi_bus_ops_t *bus,
                                      const smi_phy_config_t *devices,
                                      size_t device_count,
                                      const bvstk_event_sink_t *events);
void bvstk_smi_service_shutdown(bvstk_smi_service_t *service);

size_t bvstk_smi_service_device_count(const bvstk_smi_service_t *service);
bvstk_status_t bvstk_smi_service_device_info(const bvstk_smi_service_t *service,
                                             size_t device_id,
                                             bvstk_smi_device_info_t *out);
bvstk_status_t bvstk_smi_service_find_by_name(const bvstk_smi_service_t *service,
                                              const char *name,
                                              size_t *device_id);
bvstk_status_t bvstk_smi_service_find_by_phy(const bvstk_smi_service_t *service,
                                             uint8_t phy_addr,
                                             size_t *device_id);

bvstk_status_t bvstk_smi_service_read(bvstk_smi_service_t *service,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint16_t *value,
                                      uint32_t timeout_ms);
bvstk_status_t bvstk_smi_service_read_cached(const bvstk_smi_service_t *service,
                                             size_t device_id,
                                             uint8_t reg,
                                             uint16_t *value);
bvstk_status_t bvstk_smi_service_write(bvstk_smi_service_t *service,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint16_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms);
bvstk_status_t bvstk_smi_service_set_policy(bvstk_smi_service_t *service,
                                             size_t device_id,
                                             smi_policy_t policy);
bvstk_status_t bvstk_smi_service_get_config(const bvstk_smi_service_t *service,
                                             size_t device_id,
                                             smi_phy_config_t *out);
bvstk_status_t bvstk_smi_service_set_config(bvstk_smi_service_t *service,
                                             size_t device_id,
                                             const smi_phy_config_t *config);
bvstk_status_t bvstk_smi_service_poll(bvstk_smi_service_t *service,
                                      uint32_t timeout_ms);

#endif /* BVSTK_SMI_SERVICE_H */
