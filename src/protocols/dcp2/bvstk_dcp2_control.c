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

static bvstk_status_t policy_status(bvstk_status_t status)
{
    if (status == BVSTK_ERR_NOT_FOUND) return BVSTK_ERR_RANGE;
    if (status == BVSTK_ERR_NOT_READY) return BVSTK_ERR_BUSY;
    return status;
}

bvstk_status_t bvstk_dcp2_process_policy(
    bvstk_i2c_master_service_t *i2c, bvstk_mdio_service_t *mdio,
    const bvstk_dcp2_request_t *request, uint8_t *body,
    size_t body_capacity, uint16_t *body_size)
{
    size_t device_id, expected, rule_size, n, i;
    int is_i2c;
    bvstk_status_t status;
    uint8_t op, list;
    union {
        bvstk_i2c_policy_entry_t i2c;
        bvstk_mdio_policy_entry_t mdio;
    } snapshot;

    if (body_size == NULL) return BVSTK_ERR_MALFORMED;
    *body_size = 0U;
    if (request == NULL) return BVSTK_ERR_MALFORMED;
    is_i2c = request->service == BVSTK_DCP2_SERVICE_I2C;
    if (!is_i2c && request->service != BVSTK_DCP2_SERVICE_MDIO)
        return BVSTK_ERR_UNSUPPORTED;
    op = request->opcode;
    if (op < BVSTK_DCP2_OP_I2C_POLICY_SET || op > BVSTK_DCP2_OP_I2C_POLICY_CLEAR)
        return BVSTK_ERR_UNSUPPORTED;
    rule_size = is_i2c ? 2U : 5U;
    expected = op == BVSTK_DCP2_OP_I2C_POLICY_GET ? 1U :
               op == BVSTK_DCP2_OP_I2C_POLICY_RULE_ADD ||
               op == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ? 2U + rule_size : 2U;
    if (request->body == NULL || request->body_size != expected)
        return BVSTK_ERR_MALFORMED;
    if (op != BVSTK_DCP2_OP_I2C_POLICY_GET && request->body[1] > 1U)
        return BVSTK_ERR_MALFORMED;
    if (request->body[0] > (is_i2c ? 127U : 31U)) return BVSTK_ERR_RANGE;
    status = is_i2c ? bvstk_i2c_master_service_find_by_addr(i2c, request->body[0], &device_id) :
                     bvstk_mdio_service_find_by_phy(mdio, request->body[0], &device_id);
    if (status != BVSTK_OK) return policy_status(status);
    if (op == BVSTK_DCP2_OP_I2C_POLICY_GET) {
        size_t white, black;
        status = is_i2c ? bvstk_i2c_master_service_get_policy(i2c, device_id, &snapshot.i2c) :
                         bvstk_mdio_service_get_policy(mdio, device_id, &snapshot.mdio);
        if (status != BVSTK_OK) return policy_status(status);
        white = is_i2c ? snapshot.i2c.whitelist_len : snapshot.mdio.whitelist_len;
        black = is_i2c ? snapshot.i2c.blacklist_len : snapshot.mdio.blacklist_len;
        if (white > (is_i2c ? I2C_CFG_RULES_MAX : MDIO_CFG_RULES_MAX) ||
            black > (is_i2c ? I2C_CFG_RULES_MAX : MDIO_CFG_RULES_MAX)) return BVSTK_ERR_RANGE;
        n = 5U + (white + black) * rule_size;
        if (body == NULL || n > body_capacity || n > BVSTK_DCP2_MAX_PAYLOAD - 6U)
            return BVSTK_ERR_RANGE;
        body[0] = (uint8_t)(is_i2c ? (unsigned)snapshot.i2c.mode : (unsigned)snapshot.mdio.mode);
        bvstk_dcp2_write_be16(body + 1, (uint16_t)white);
        bvstk_dcp2_write_be16(body + 3, (uint16_t)black);
        n = 5U;
        for (i = 0U; i < white + black; ++i) {
            size_t index = i < white ? i : i - white;
            if (is_i2c) {
                const i2c_rule_entry_t *rule = i < white ? &snapshot.i2c.whitelist[index] : &snapshot.i2c.blacklist[index];
                body[n++] = rule->reg; body[n++] = rule->val;
            } else {
                const mdio_rule_entry_t *rule = i < white ? &snapshot.mdio.whitelist[index] : &snapshot.mdio.blacklist[index];
                body[n++] = rule->reg;
                bvstk_dcp2_write_be16(body + n, rule->val); n += 2U;
                bvstk_dcp2_write_be16(body + n, rule->mask); n += 2U;
            }
        }
        *body_size = (uint16_t)n;
        return BVSTK_OK;
    }
    list = request->body[1];
    if (op == BVSTK_DCP2_OP_I2C_POLICY_SET) {
        status = is_i2c ? bvstk_i2c_master_service_set_policy(i2c, device_id, (i2c_policy_t)list) :
                         bvstk_mdio_service_set_policy(mdio, device_id, (mdio_policy_t)list);
    } else if (op == BVSTK_DCP2_OP_I2C_POLICY_CLEAR) {
        status = is_i2c ? bvstk_i2c_master_service_rule_clear_list(i2c, device_id, (i2c_policy_t)list) :
                         bvstk_mdio_service_rule_clear_list(mdio, device_id, (mdio_policy_t)list);
    } else if (is_i2c) {
        status = op == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ?
            bvstk_i2c_master_service_rule_remove(i2c, device_id, (i2c_policy_t)list, request->body[2], request->body[3]) :
            list == 0U ? bvstk_i2c_master_service_rule_allow(i2c, device_id, request->body[2], request->body[3]) :
                         bvstk_i2c_master_service_rule_deny(i2c, device_id, request->body[2], request->body[3]);
    } else {
        uint16_t value = bvstk_dcp2_read_be16(request->body + 3);
        uint16_t mask = bvstk_dcp2_read_be16(request->body + 5);
        status = op == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ?
            bvstk_mdio_service_rule_remove(mdio, device_id, (mdio_policy_t)list, request->body[2], value, mask) :
            bvstk_mdio_service_rule_add(mdio, device_id, (mdio_policy_t)list, request->body[2], value, mask);
    }
    return policy_status(status);
}

static bvstk_status_t process_i2c(bvstk_control_api_t *control,
                                  const bvstk_dcp2_request_t *request,
                                  uint8_t *body,
                                  size_t body_capacity,
                                  uint16_t *body_size)
{
    size_t device_id;
    bvstk_status_t status;
    uint8_t address;

    if (request->opcode >= BVSTK_DCP2_OP_I2C_POLICY_SET &&
        request->opcode <= BVSTK_DCP2_OP_I2C_POLICY_CLEAR) {
        return bvstk_dcp2_process_policy(control == NULL ? NULL : control->i2c,
                                         control == NULL ? NULL : control->mdio,
                                         request, body, body_capacity, body_size);
    }
    if (request->opcode != BVSTK_DCP2_OP_I2C_READ_REG &&
        request->opcode != BVSTK_DCP2_OP_I2C_WRITE_REG) return BVSTK_ERR_UNSUPPORTED;
    if (control == NULL || control->i2c == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    if (request->body_size < 2U) {
        return BVSTK_ERR_MALFORMED;
    }
    address = request->body[0];
    status = bvstk_i2c_master_service_find_by_addr(control->i2c,
                                                   address,
                                                   &device_id);
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
    return BVSTK_ERR_UNSUPPORTED;
}

static bvstk_status_t process_mdio(bvstk_control_api_t *control,
                                  const bvstk_dcp2_request_t *request,
                                  uint8_t *body,
                                  size_t body_capacity,
                                  uint16_t *body_size)
{
    size_t device_id;
    bvstk_status_t status;
    uint16_t value = 0U;

    if (request->opcode >= BVSTK_DCP2_OP_MDIO_POLICY_SET &&
        request->opcode <= BVSTK_DCP2_OP_MDIO_POLICY_CLEAR) {
        return bvstk_dcp2_process_policy(control == NULL ? NULL : control->i2c,
                                         control == NULL ? NULL : control->mdio,
                                         request, body, body_capacity, body_size);
    }
    if (request->opcode != BVSTK_DCP2_OP_MDIO_READ &&
        request->opcode != BVSTK_DCP2_OP_MDIO_WRITE) return BVSTK_ERR_UNSUPPORTED;
    if (control == NULL || control->mdio == NULL) {
        return BVSTK_ERR_NOT_READY;
    }
    if (request->body_size < 2U) {
        return BVSTK_ERR_MALFORMED;
    }
    status = bvstk_mdio_service_find_by_phy(control->mdio,
                                           request->body[0],
                                           &device_id);
    if (status != BVSTK_OK) {
        return status;
    }
    if (request->opcode == BVSTK_DCP2_OP_MDIO_READ) {
        if (request->body_size != 2U) {
            return BVSTK_ERR_MALFORMED;
        }
        status = bvstk_control_mdio_read(control,
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
    if (request->opcode == BVSTK_DCP2_OP_MDIO_WRITE) {
        if (request->body_size != 4U) {
            return BVSTK_ERR_MALFORMED;
        }
        value = bvstk_dcp2_read_be16(request->body + 2U);
        return bvstk_control_mdio_write(control,
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
    size_t body_capacity;

    if (control == NULL || request == NULL || response_frame == NULL || response_capacity < 14U) {
        return -1;
    }
    body_capacity = response_capacity - 14U;
    if (body_capacity > BVSTK_DCP2_MAX_PAYLOAD - 6U) body_capacity = BVSTK_DCP2_MAX_PAYLOAD - 6U;
    if (request->service == BVSTK_DCP2_SERVICE_PING &&
        request->opcode == BVSTK_DCP2_OP_PING &&
        request->body_size == 0U) {
        status = BVSTK_OK;
    } else if (request->service == BVSTK_DCP2_SERVICE_MEM) {
        status = process_mem(control, request, body, &body_size);
    } else if (request->service == BVSTK_DCP2_SERVICE_I2C) {
        status = process_i2c(control, request, body, body_capacity, &body_size);
    } else if (request->service == BVSTK_DCP2_SERVICE_MDIO) {
        status = process_mdio(control, request, body, body_capacity, &body_size);
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
