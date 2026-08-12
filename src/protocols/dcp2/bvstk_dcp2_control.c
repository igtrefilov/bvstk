#include "protocols/dcp2/bvstk_dcp2_control.h"

#include <string.h>

static bvstk_status_t find_region(uint32_t address,
                                  size_t width,
                                  bvstk_pl_region_id_t *region,
                                  size_t *offset)
{
    size_t i;
    if (region == NULL || offset == NULL || width == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    for (i = 0U; i < bvstk_pl_region_count(); ++i) {
        const bvstk_pl_region_desc_t *desc =
            bvstk_pl_region_get((bvstk_pl_region_id_t)i);
        uint64_t start;
        uint64_t end;
        if (desc == NULL) {
            continue;
        }
        start = (uint64_t)desc->physical_base;
        end = start + desc->size;
        if ((uint64_t)address >= start &&
            (uint64_t)address + width <= end &&
            ((address - (uint32_t)start) & 3U) == 0U) {
            *region = desc->id;
            *offset = (size_t)((uint64_t)address - start);
            return BVSTK_OK;
        }
    }
    return BVSTK_ERR_RANGE;
}

static bvstk_status_t process_mem(bvstk_control_api_t *control,
                                  const bvstk_dcp2_request_t *request,
                                  uint8_t *body,
                                  uint16_t *body_size)
{
    uint8_t flags;
    uint8_t width;
    uint32_t address;
    uint16_t count;
    uint16_t i;

    if (request->body_size < 8U) {
        return BVSTK_ERR_MALFORMED;
    }
    flags = request->body[0];
    width = request->body[1];
    address = bvstk_dcp2_read_be32(request->body + 2);
    count = bvstk_dcp2_read_be16(request->body + 6);
    if (flags != 0U || width != 32U || count == 0U || count > 256U) {
        return BVSTK_ERR_UNSUPPORTED;
    }
    if (request->opcode == BVSTK_DCP2_OP_MEM_READ) {
        if (request->body_size != 8U || (size_t)count * 4U > BVSTK_DCP2_MAX_PAYLOAD - 6U) {
            return BVSTK_ERR_RANGE;
        }
        for (i = 0U; i < count; ++i) {
            bvstk_pl_region_id_t region;
            size_t offset;
            uint32_t value;
            bvstk_status_t status = find_region(address + (uint32_t)i * 4U,
                                                sizeof(value), &region, &offset);
            if (status != BVSTK_OK) {
                return status;
            }
            status = bvstk_control_pl_read32(control, region, offset, &value);
            if (status != BVSTK_OK) {
                return status;
            }
            bvstk_dcp2_write_be32(body + (size_t)i * 4U, value);
        }
        *body_size = (uint16_t)((size_t)count * 4U);
        return BVSTK_OK;
    }
    if (request->opcode == BVSTK_DCP2_OP_MEM_WRITE) {
        if (request->body_size != (uint16_t)(8U + (size_t)count * 4U)) {
            return BVSTK_ERR_MALFORMED;
        }
        for (i = 0U; i < count; ++i) {
            bvstk_pl_region_id_t region;
            size_t offset;
            bvstk_status_t status = find_region(address + (uint32_t)i * 4U,
                                                sizeof(uint32_t), &region, &offset);
            if (status != BVSTK_OK) {
                return status;
            }
            status = bvstk_control_pl_write32(control,
                                              region,
                                              offset,
                                              bvstk_dcp2_read_be32(request->body + 8U + (size_t)i * 4U));
            if (status != BVSTK_OK) {
                return status;
            }
        }
        *body_size = 0U;
        return BVSTK_OK;
    }
    return BVSTK_ERR_UNSUPPORTED;
}

static bvstk_status_t process_i2c(bvstk_control_api_t *control,
                                  const bvstk_dcp2_request_t *request,
                                  uint8_t *body,
                                  uint16_t *body_size)
{
    size_t device_id;
    bvstk_status_t status;
    uint8_t address;

    if (control == NULL || control->i2c == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    if (request->body_size < 2U) {
        return BVSTK_ERR_MALFORMED;
    }
    address = request->body[0];
    status = bvstk_i2c_service_find_by_addr(control->i2c, address, &device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (request->opcode == BVSTK_DCP2_OP_I2C_READ_REG) {
        if (request->body_size != 2U) {
            return BVSTK_ERR_MALFORMED;
        }
        status = bvstk_control_i2c_read(control,
                                        device_id,
                                        request->body[1],
                                        body,
                                        100U);
        *body_size = status == BVSTK_OK ? 1U : 0U;
        return status;
    }
    if (request->opcode == BVSTK_DCP2_OP_I2C_WRITE_REG) {
        if (request->body_size != 3U) {
            return BVSTK_ERR_MALFORMED;
        }
        status = bvstk_control_i2c_write(control,
                                         device_id,
                                         request->body[1],
                                         request->body[2],
                                         BVSTK_EVENT_SOURCE_DCP,
                                         100U);
        *body_size = 0U;
        return status;
    }
    if (request->opcode == BVSTK_DCP2_OP_I2C_POLICY_SET) {
        if (request->body_size != 2U ||
            (request->body[1] != 0U && request->body[1] != 1U)) {
            return BVSTK_ERR_MALFORMED;
        }
        status = bvstk_i2c_service_set_policy(control->i2c,
                                              device_id,
                                              request->body[1] == 0U
                                                  ? I2C_POLICY_WHITELIST
                                                  : I2C_POLICY_BLACKLIST);
        *body_size = 0U;
        return status;
    }
    return BVSTK_ERR_UNSUPPORTED;
}

static bvstk_status_t process_smi(bvstk_control_api_t *control,
                                  const bvstk_dcp2_request_t *request,
                                  uint8_t *body,
                                  uint16_t *body_size)
{
    size_t device_id;
    bvstk_status_t status;
    uint16_t value = 0U;

    if (control == NULL || control->smi == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    if (request->body_size < 2U) {
        return BVSTK_ERR_MALFORMED;
    }
    status = bvstk_smi_service_find_by_phy(control->smi,
                                           request->body[0],
                                           &device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (request->opcode == BVSTK_DCP2_OP_SMI_READ) {
        if (request->body_size != 2U) {
            return BVSTK_ERR_MALFORMED;
        }
        status = bvstk_control_smi_read(control,
                                        device_id,
                                        request->body[1],
                                        &value,
                                        100U);
        if (status == BVSTK_OK) {
            bvstk_dcp2_write_be16(body, value);
            *body_size = 2U;
        }
        return status;
    }
    if (request->opcode == BVSTK_DCP2_OP_SMI_WRITE) {
        if (request->body_size != 4U) {
            return BVSTK_ERR_MALFORMED;
        }
        value = bvstk_dcp2_read_be16(request->body + 2U);
        return bvstk_control_smi_write(control,
                                       device_id,
                                       request->body[1],
                                       value,
                                       BVSTK_EVENT_SOURCE_DCP,
                                       100U);
    }
    return BVSTK_ERR_UNSUPPORTED;
}

static bvstk_status_t process_spi(bvstk_control_api_t *control,
                                  const bvstk_dcp2_request_t *request,
                                  uint8_t *body,
                                  uint16_t *body_size)
{
    uint16_t count;
    uint16_t i;
    uint32_t tx[256];
    uint32_t rx[256];
    bvstk_status_t status;

    if (control == NULL || control->spi == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    if (request->opcode != BVSTK_DCP2_OP_SPI_TRANSFER ||
        request->body_size < 2U) {
        return BVSTK_ERR_MALFORMED;
    }
    count = bvstk_dcp2_read_be16(request->body);
    if (count == 0U || count > 256U ||
        request->body_size != (uint16_t)(2U + (size_t)count * 4U)) {
        return BVSTK_ERR_RANGE;
    }
    for (i = 0U; i < count; ++i) {
        tx[i] = bvstk_dcp2_read_be32(request->body + 2U + (size_t)i * 4U);
    }
    status = bvstk_control_spi_transfer(control,
                                        tx,
                                        count,
                                        rx,
                                        count,
                                        100U);
    if (status != BVSTK_OK) {
        return status;
    }
    for (i = 0U; i < count; ++i) {
        bvstk_dcp2_write_be32(body + (size_t)i * 4U, rx[i]);
    }
    *body_size = (uint16_t)((size_t)count * 4U);
    return BVSTK_OK;
}

int bvstk_dcp2_process_request(bvstk_control_api_t *control,
                               const bvstk_dcp2_request_t *request,
                               uint8_t *response_frame,
                               size_t response_capacity,
                               size_t *response_size)
{
    uint8_t body[BVSTK_DCP2_MAX_PAYLOAD];
    uint16_t body_size = 0U;
    bvstk_status_t status;

    if (control == NULL || request == NULL || response_frame == NULL) {
        return -1;
    }
    if (request->service == BVSTK_DCP2_SERVICE_PING &&
        request->opcode == BVSTK_DCP2_OP_PING &&
        request->body_size == 0U) {
        status = BVSTK_OK;
    } else if (request->service == BVSTK_DCP2_SERVICE_MEM) {
        status = process_mem(control, request, body, &body_size);
    } else if (request->service == BVSTK_DCP2_SERVICE_I2C) {
        status = process_i2c(control, request, body, &body_size);
    } else if (request->service == BVSTK_DCP2_SERVICE_SMI) {
        status = process_smi(control, request, body, &body_size);
    } else if (request->service == BVSTK_DCP2_SERVICE_SPI) {
        status = process_spi(control, request, body, &body_size);
    } else {
        status = BVSTK_ERR_UNSUPPORTED;
    }

    return bvstk_dcp2_encode_response(response_frame,
                                      response_capacity,
                                      request,
                                      bvstk_dcp2_status_from_bvstk(status),
                                      body_size == 0U ? NULL : body,
                                      body_size,
                                      response_size);
}
