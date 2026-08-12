#include "services/control/bvstk_control_api.h"

bvstk_status_t bvstk_control_pl_read32(bvstk_control_api_t *api,
                                       bvstk_pl_region_id_t region,
                                       size_t offset,
                                       uint32_t *value)
{
    if (api == NULL || api->pl == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_pl_service_read32(api->pl, region, offset, value);
}

bvstk_status_t bvstk_control_pl_write32(bvstk_control_api_t *api,
                                        bvstk_pl_region_id_t region,
                                        size_t offset,
                                        uint32_t value)
{
    if (api == NULL || api->pl == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_pl_service_write32(api->pl, region, offset, value);
}

bvstk_status_t bvstk_control_i2c_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint8_t *value,
                                      uint32_t timeout_ms)
{
    if (api == NULL || api->i2c == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_service_read_reg(api->i2c,
                                      device_id,
                                      reg,
                                      value,
                                      timeout_ms);
}

bvstk_status_t bvstk_control_i2c_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint8_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms)
{
    if (api == NULL || api->i2c == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_i2c_service_write_reg(api->i2c,
                                       device_id,
                                       reg,
                                       value,
                                       source,
                                       timeout_ms);
}

bvstk_status_t bvstk_control_smi_read(bvstk_control_api_t *api,
                                      size_t device_id,
                                      uint8_t reg,
                                      uint16_t *value,
                                      uint32_t timeout_ms)
{
    if (api == NULL || api->smi == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_smi_service_read(api->smi,
                                  device_id,
                                  reg,
                                  value,
                                  timeout_ms);
}

bvstk_status_t bvstk_control_smi_write(bvstk_control_api_t *api,
                                       size_t device_id,
                                       uint8_t reg,
                                       uint16_t value,
                                       bvstk_event_source_t source,
                                       uint32_t timeout_ms)
{
    if (api == NULL || api->smi == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_smi_service_write(api->smi,
                                   device_id,
                                   reg,
                                   value,
                                   source,
                                   timeout_ms);
}

bvstk_status_t bvstk_control_spi_transfer(bvstk_control_api_t *api,
                                          const uint32_t *tx_words,
                                          size_t tx_count,
                                          uint32_t *rx_words,
                                          size_t rx_capacity,
                                          uint32_t timeout_ms)
{
    if (api == NULL || api->spi == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    return bvstk_spi_core_transfer(api->spi,
                                   tx_words,
                                   tx_count,
                                   rx_words,
                                   rx_capacity,
                                   timeout_ms);
}
