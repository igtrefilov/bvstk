#include "protocols/dcp2/bvstk_dcp2_policy.h"

#include <stdio.h>
#include <string.h>

static int valid_name(const char *name, size_t size)
{
    if (!size || size >= BVSTK_DCP2_POLICY_NAME_MAX) return 0;
    for (size_t i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return 0;
    }
    return 1;
}

static int read_name(const uint8_t **cursor, size_t *left, char name[32])
{
    size_t size;
    if (!*left) return 0;
    size = *(*cursor)++;
    --*left;
    if (size > *left || !valid_name((const char *)*cursor, size)) return 0;
    memcpy(name, *cursor, size);
    name[size] = 0;
    *cursor += size;
    *left -= size;
    return 1;
}

static uint64_t read64(const uint8_t *p)
{
    uint64_t n = 0;
    for (unsigned i = 0; i < 8; ++i) n = (n << 8) | p[i];
    return n;
}

static void write64(uint8_t *p, uint64_t n)
{
    for (unsigned i = 0; i < 8; ++i) { p[7U - i] = (uint8_t)n; n >>= 8; }
}

static uint16_t wire_status(bvstk_status_t status)
{
    if (status == BVSTK_ERR_NOT_FOUND) status = BVSTK_ERR_RANGE;
    if (status == BVSTK_ERR_NOT_READY) status = BVSTK_ERR_BUSY;
    return bvstk_dcp2_status_from_bvstk(status);
}

void bvstk_dcp2_policy_init(bvstk_dcp2_policy_session_t *session)
{
    memset(session, 0, sizeof(*session));
}

bvstk_status_t bvstk_dcp2_policy_catalog_from_profile(
    bvstk_dcp2_policy_catalog_t *catalog, const bvstk_profile_metadata_t *profile, size_t count)
{
    if (!catalog || !profile || !count || count > I2C_CFG_MAX_REG_COUNT)
        return BVSTK_ERR_RANGE;
    memset(catalog, 0, sizeof(*catalog));
    catalog->count = (uint16_t)count;
    for (unsigned i = 0; i < count; ++i) {
        char fallback[16];
        const char *id;
        size_t length;
        if (profile->version == 2U) {
            unsigned offset = profile->id_offset[i];
            if (profile->ids_size > sizeof(profile->ids) || offset >= profile->ids_size ||
                !memchr(profile->ids + offset, 0, profile->ids_size - offset)) return BVSTK_ERR_MALFORMED;
            id = profile->ids + offset;
        } else if (profile->version == 0U) {
            (void)snprintf(fallback, sizeof(fallback), "reg_%02x", i);
            id = fallback;
        } else return BVSTK_ERR_UNSUPPORTED;
        length = strlen(id);
        if (!valid_name(id, length) || length + 1U > sizeof(catalog->ids) - catalog->ids_size)
            return BVSTK_ERR_RANGE;
        for (unsigned j = 0; j < i; ++j)
            if (!strcmp(id, catalog->ids + catalog->offset[j])) return BVSTK_ERR_MALFORMED;
        catalog->offset[i] = catalog->ids_size;
        memcpy(catalog->ids + catalog->ids_size, id, length + 1U);
        catalog->ids_size += (uint16_t)(length + 1U);
    }
    return BVSTK_OK;
}

static bvstk_status_t resolve(const bvstk_dcp2_policy_adapter_t *adapter,
    const char *subsystem, const char *device, uint8_t *service, size_t *device_id)
{
    if (!strcmp(subsystem, "i2c")) {
        bvstk_i2c_device_t info;
        bvstk_status_t status;
        *service = BVSTK_DCP2_SERVICE_I2C;
        status = bvstk_i2c_master_service_find_by_name(adapter->i2c, device, device_id);
        if (status != BVSTK_OK) return status;
        status = bvstk_i2c_master_service_device_info(adapter->i2c, *device_id, &info);
        return status == BVSTK_OK && strcmp(device, info.name) ? BVSTK_ERR_NOT_FOUND : status;
    }
    if (!strcmp(subsystem, "mdio")) {
        bvstk_mdio_device_info_t info;
        bvstk_status_t status;
        *service = BVSTK_DCP2_SERVICE_MDIO;
        status = bvstk_mdio_service_find_by_name(adapter->mdio, device, device_id);
        if (status != BVSTK_OK) return status;
        status = bvstk_mdio_service_device_info(adapter->mdio, *device_id, &info);
        return status == BVSTK_OK && strcmp(device, info.name) ? BVSTK_ERR_NOT_FOUND : status;
    }
    return BVSTK_ERR_UNSUPPORTED;
}

static uint16_t snapshot(bvstk_dcp2_policy_session_t *s, const bvstk_dcp2_policy_adapter_t *a,
    const char *subsystem, const char *device, uint64_t now_ms)
{
    size_t device_id;
    bvstk_status_t status = resolve(a, subsystem, device, &s->service, &device_id);
    s->snapshot_id = 0;
    if (status != BVSTK_OK) return wire_status(status);
    if (!a->catalog) return wire_status(BVSTK_ERR_UNSUPPORTED);
    status = a->catalog(a->context, s->service, device_id, &s->catalog);
    if (status != BVSTK_OK) return wire_status(status);
    status = s->service == BVSTK_DCP2_SERVICE_I2C ?
        bvstk_i2c_master_service_get_policy(a->i2c, device_id, &s->policy.i2c) :
        bvstk_mdio_service_get_policy(a->mdio, device_id, &s->policy.mdio);
    if (status != BVSTK_OK) return wire_status(status);
    strcpy(s->subsystem, subsystem);
    strcpy(s->device, device);
    if (++s->next_snapshot == 0) ++s->next_snapshot;
    s->snapshot_id = s->next_snapshot;
    s->activity_ms = now_ms;
    return 0;
}

static uint16_t page(bvstk_dcp2_policy_session_t *s, uint16_t first,
    uint8_t *out, size_t capacity, uint16_t *out_size)
{
    int i2c = s->service == BVSTK_DCP2_SERVICE_I2C;
    size_t white = i2c ? s->policy.i2c.whitelist_len : s->policy.mdio.whitelist_len;
    size_t black = i2c ? s->policy.i2c.blacklist_len : s->policy.mdio.blacklist_len;
    size_t limit = i2c ? I2C_CFG_RULES_MAX : MDIO_CFG_RULES_MAX;
    size_t used = BVSTK_DCP2_POLICY_PAGE_HEADER, n = first;
    if (white > limit || black > limit) return wire_status(BVSTK_ERR_INTERNAL);
    if (first > white + black) return wire_status(BVSTK_ERR_RANGE);
    if (!out || capacity < used) return wire_status(BVSTK_ERR_RANGE);
    while (n < white + black) {
        size_t index = n < white ? n : n - white;
        unsigned reg;
        uint64_t value, mask;
        const char *id;
        size_t length;
        if (i2c) {
            const i2c_rule_entry_t *r = n < white ? &s->policy.i2c.whitelist[index] : &s->policy.i2c.blacklist[index];
            reg = r->reg; value = r->val; mask = UINT64_C(255);
        } else {
            const mdio_rule_entry_t *r = n < white ? &s->policy.mdio.whitelist[index] : &s->policy.mdio.blacklist[index];
            reg = r->reg; value = r->val; mask = r->mask;
        }
        if (reg >= s->catalog.count) return wire_status(BVSTK_ERR_INTERNAL);
        id = s->catalog.ids + s->catalog.offset[reg];
        length = strlen(id);
        if (length + 17U > capacity - used) break;
        out[used++] = (uint8_t)length;
        memcpy(out + used, id, length); used += length;
        write64(out + used, value); used += 8;
        write64(out + used, mask); used += 8;
        ++n;
    }
    if (n == first && n < white + black) return wire_status(BVSTK_ERR_RANGE);
    out[0] = BVSTK_DCP2_POLICY_VERSION;
    out[1] = (uint8_t)(i2c ? (unsigned)s->policy.i2c.mode : (unsigned)s->policy.mdio.mode);
    bvstk_dcp2_write_be32(out + 2, s->snapshot_id);
    bvstk_dcp2_write_be16(out + 6, (uint16_t)white);
    bvstk_dcp2_write_be16(out + 8, (uint16_t)black);
    bvstk_dcp2_write_be16(out + 10, first);
    bvstk_dcp2_write_be16(out + 12, (uint16_t)(n - first));
    bvstk_dcp2_write_be16(out + 14, n == white + black ? UINT16_MAX : (uint16_t)n);
    *out_size = (uint16_t)used;
    return 0;
}

uint16_t bvstk_dcp2_policy_request(bvstk_dcp2_policy_session_t *s,
    const bvstk_dcp2_policy_adapter_t *a, uint8_t opcode,
    const uint8_t *body, size_t size, uint64_t now_ms,
    uint8_t *out, size_t capacity, uint16_t *out_size)
{
    char subsystem[32], device[32], resource[32];
    const uint8_t *p = body;
    size_t left = size, device_id;
    uint8_t service = 0, list;
    unsigned reg = 0;
    bvstk_status_t status;

    if (!out_size) return wire_status(BVSTK_ERR_MALFORMED);
    *out_size = 0;
    if (!s || !a) return wire_status(BVSTK_ERR_BUSY);
    if (!body || !left) return wire_status(BVSTK_ERR_MALFORMED);
    if (*p++ != BVSTK_DCP2_POLICY_VERSION) return wire_status(BVSTK_ERR_UNSUPPORTED);
    --left;
    if (opcode < BVSTK_DCP2_OP_I2C_POLICY_SET || opcode > BVSTK_DCP2_OP_I2C_POLICY_SAVE)
        return wire_status(BVSTK_ERR_UNSUPPORTED);
    if (!read_name(&p, &left, subsystem) || !read_name(&p, &left, device))
        return wire_status(BVSTK_ERR_MALFORMED);
    if (opcode == BVSTK_DCP2_OP_I2C_POLICY_GET) {
        uint32_t id;
        uint16_t first, result;
        if (left != 6U) return wire_status(BVSTK_ERR_MALFORMED);
        id = bvstk_dcp2_read_be32(p); first = bvstk_dcp2_read_be16(p + 4);
        if (!id) {
            if (first) return wire_status(BVSTK_ERR_RANGE);
            result = snapshot(s, a, subsystem, device, now_ms);
            if (result) return result;
        } else if (id != s->snapshot_id || strcmp(subsystem, s->subsystem) ||
                   strcmp(device, s->device) || now_ms - s->activity_ms > BVSTK_DCP2_POLICY_SNAPSHOT_MS) {
            return BVSTK_DCP2_POLICY_STALE_SNAPSHOT;
        }
        result = page(s, first, out, capacity, out_size);
        if (!result) s->activity_ms = now_ms;
        return result;
    }
    if (opcode == BVSTK_DCP2_OP_I2C_POLICY_SAVE) {
        if (left) return wire_status(BVSTK_ERR_MALFORMED);
    } else {
        if (!left || *p > 1U) return wire_status(BVSTK_ERR_MALFORMED);
        list = *p++; --left;
        if (opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_ADD || opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE) {
            if (!read_name(&p, &left, resource) || left != 16U) return wire_status(BVSTK_ERR_MALFORMED);
        } else if (left) return wire_status(BVSTK_ERR_MALFORMED);
    }
    status = resolve(a, subsystem, device, &service, &device_id);
    if (status != BVSTK_OK) return wire_status(status);
    if (opcode == BVSTK_DCP2_OP_I2C_POLICY_SAVE)
        return wire_status(a->save ? a->save(a->context, service, device_id) : BVSTK_ERR_UNSUPPORTED);
    if (opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_ADD || opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE) {
        if (!a->catalog) return wire_status(BVSTK_ERR_UNSUPPORTED);
        /* Reuse the bounded session catalog instead of allocating it on a task stack. */
        s->snapshot_id = 0;
        status = a->catalog(a->context, service, device_id, &s->catalog);
        if (status != BVSTK_OK) return wire_status(status);
        for (reg = 0; reg < s->catalog.count; ++reg)
            if (!strcmp(resource, s->catalog.ids + s->catalog.offset[reg])) break;
        if (reg == s->catalog.count) return wire_status(BVSTK_ERR_RANGE);
    }
    s->snapshot_id = 0;
    if (service == BVSTK_DCP2_SERVICE_I2C) {
        if (opcode == BVSTK_DCP2_OP_I2C_POLICY_SET)
            status = bvstk_i2c_master_service_set_policy(a->i2c, device_id, (i2c_policy_t)list);
        else if (opcode == BVSTK_DCP2_OP_I2C_POLICY_CLEAR)
            status = bvstk_i2c_master_service_rule_clear_range(a->i2c, device_id, (i2c_policy_t)list, 0, 256);
        else {
            uint64_t value = read64(p), mask = read64(p + 8);
            if (value > UINT8_MAX || mask != UINT8_MAX) return wire_status(BVSTK_ERR_RANGE);
            status = opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ?
                bvstk_i2c_master_service_rule_remove(a->i2c, device_id, (i2c_policy_t)list, (uint8_t)reg, (uint8_t)value) :
                list == 0 ? bvstk_i2c_master_service_rule_allow(a->i2c, device_id, (uint8_t)reg, (uint8_t)value) :
                            bvstk_i2c_master_service_rule_deny(a->i2c, device_id, (uint8_t)reg, (uint8_t)value);
        }
    } else {
        if (opcode == BVSTK_DCP2_OP_I2C_POLICY_SET)
            status = bvstk_mdio_service_set_policy(a->mdio, device_id, (mdio_policy_t)list);
        else if (opcode == BVSTK_DCP2_OP_I2C_POLICY_CLEAR)
            status = bvstk_mdio_service_rule_clear_list(a->mdio, device_id, (mdio_policy_t)list);
        else {
            uint64_t value = read64(p), mask = read64(p + 8);
            if (reg > 31U || value > UINT16_MAX || mask > UINT16_MAX) return wire_status(BVSTK_ERR_RANGE);
            status = opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ?
                bvstk_mdio_service_rule_remove(a->mdio, device_id, (mdio_policy_t)list, (uint8_t)reg, (uint16_t)value, (uint16_t)mask) :
                bvstk_mdio_service_rule_add(a->mdio, device_id, (mdio_policy_t)list, (uint8_t)reg, (uint16_t)value, (uint16_t)mask);
        }
    }
    if (status == BVSTK_OK && a->changed) status = a->changed(a->context, service, device_id);
    return wire_status(status);
}
