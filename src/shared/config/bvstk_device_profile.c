#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_mdio_config_codec.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Bounded, allocation-free JSON views. Parsing never searches for a key in a
 * different object and never narrows a number before validating its range. */
typedef struct { const char *p, *end; } view_t;
typedef struct { char *p; size_t cap, n; int failed; } writer_t;

static void ws(const char **p, const char *end)
{
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')) ++*p;
}
static int digit(char c) { return c >= '0' && c <= '9'; }
static int hex_digit(char c)
{
    if (digit(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int emit(char *out, size_t cap, size_t *n, unsigned char c)
{
    if (out) {
        if (*n + 1U >= cap) return 0;
        out[*n] = (char)c;
    }
    ++*n;
    return 1;
}
static int codepoint(char *out, size_t cap, size_t *n, uint32_t cp)
{
    if (!cp || cp > 0x10FFFFU || (cp >= 0xD800U && cp <= 0xDFFFU)) return 0;
    if (cp < 0x80U) return emit(out, cap, n, (unsigned char)cp);
    if (cp < 0x800U)
        return emit(out, cap, n, (unsigned char)(0xC0U | (cp >> 6))) &&
               emit(out, cap, n, (unsigned char)(0x80U | (cp & 63U)));
    if (cp < 0x10000U)
        return emit(out, cap, n, (unsigned char)(0xE0U | (cp >> 12))) &&
               emit(out, cap, n, (unsigned char)(0x80U | ((cp >> 6) & 63U))) &&
               emit(out, cap, n, (unsigned char)(0x80U | (cp & 63U)));
    return emit(out, cap, n, (unsigned char)(0xF0U | (cp >> 18))) &&
           emit(out, cap, n, (unsigned char)(0x80U | ((cp >> 12) & 63U))) &&
           emit(out, cap, n, (unsigned char)(0x80U | ((cp >> 6) & 63U))) &&
           emit(out, cap, n, (unsigned char)(0x80U | (cp & 63U)));
}
static int hex4(const char **p, const char *end, uint32_t *cp)
{
    unsigned i;
    *cp = 0;
    if (end - *p < 4) return 0;
    for (i = 0; i < 4U; ++i) {
        int d = hex_digit(*(*p)++);
        if (d < 0) return 0;
        *cp = (*cp << 4) | (uint32_t)d;
    }
    return 1;
}
static int string(const char **p, const char *end, char *out, size_t cap)
{
    size_t n = 0;
    if (*p >= end || *(*p)++ != '"') return 0;
    while (*p < end && **p != '"') {
        unsigned char c = (unsigned char)*(*p)++;
        if (c < 32U) return 0;
        if (c == '\\') {
            uint32_t cp, low;
            if (*p >= end) return 0;
            c = (unsigned char)*(*p)++;
            if (c == 'u') {
                if (!hex4(p, end, &cp)) return 0;
                if (cp >= 0xD800U && cp <= 0xDBFFU) {
                    if (end - *p < 6 || (*p)[0] != '\\' || (*p)[1] != 'u') return 0;
                    *p += 2;
                    if (!hex4(p, end, &low) || low < 0xDC00U || low > 0xDFFFU) return 0;
                    cp = 0x10000U + ((cp - 0xD800U) << 10) + low - 0xDC00U;
                }
                if (!codepoint(out, cap, &n, cp)) return 0;
                continue;
            }
            if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c != '"' && c != '\\' && c != '/') return 0;
        } else if (c >= 128U) {
            uint32_t cp, minimum;
            unsigned count, i;
            if (c >= 0xC2U && c <= 0xDFU) { cp = c & 31U; count = 1; minimum = 0x80; }
            else if (c >= 0xE0U && c <= 0xEFU) { cp = c & 15U; count = 2; minimum = 0x800; }
            else if (c >= 0xF0U && c <= 0xF4U) { cp = c & 7U; count = 3; minimum = 0x10000; }
            else return 0;
            if ((size_t)(end - *p) < count) return 0;
            for (i = 0; i < count; ++i) {
                unsigned char next = (unsigned char)*(*p)++;
                if ((next & 0xC0U) != 0x80U) return 0;
                cp = (cp << 6) | (next & 63U);
            }
            if (cp < minimum || !codepoint(out, cap, &n, cp)) return 0;
            continue;
        }
        if (!emit(out, cap, &n, c)) return 0;
    }
    if (*p >= end || *(*p)++ != '"') return 0;
    if (out) out[n] = 0;
    return 1;
}
static int value(const char **p, const char *end, unsigned depth)
{
    const char *start;
    char close;
    ws(p, end);
    if (*p >= end) return 0;
    if (**p == '"') return string(p, end, NULL, 0);
    if (**p == '[' || **p == '{') {
        int object = **p == '{';
        if (depth >= 16U) return 0;
        close = object ? '}' : ']';
        ++*p; ws(p, end);
        if (*p < end && **p == close) { ++*p; return 1; }
        for (;;) {
            if (object) {
                if (!string(p, end, NULL, 0)) return 0;
                ws(p, end);
                if (*p >= end || *(*p)++ != ':') return 0;
            }
            if (!value(p, end, depth + 1U)) return 0;
            ws(p, end);
            if (*p >= end) return 0;
            if (**p == close) { ++*p; return 1; }
            if (*(*p)++ != ',') return 0;
            ws(p, end);
        }
    }
    if (end - *p >= 4 && (!memcmp(*p, "true", 4) || !memcmp(*p, "null", 4))) { *p += 4; return 1; }
    if (end - *p >= 5 && !memcmp(*p, "false", 5)) { *p += 5; return 1; }
    start = *p;
    if (**p == '-') ++*p;
    if (*p >= end || !digit(**p)) return 0;
    if (**p == '0') ++*p;
    else while (*p < end && digit(**p)) ++*p;
    if (*p < end && **p == '.') {
        ++*p;
        if (*p >= end || !digit(**p)) return 0;
        while (*p < end && digit(**p)) ++*p;
    }
    if (*p < end && (**p == 'e' || **p == 'E')) {
        ++*p;
        if (*p < end && (**p == '+' || **p == '-')) ++*p;
        if (*p >= end || !digit(**p)) return 0;
        while (*p < end && digit(**p)) ++*p;
    }
    return *p > start;
}
static int document(const char *json, view_t *root)
{
    const char *end, *p;
    size_t n;
    if (!json) return 0;
    for (n = 0; n <= BVSTK_PROFILE_JSON_MAX && json[n]; ++n) {}
    if (n > BVSTK_PROFILE_JSON_MAX) return 0;
    end = json + n; p = json; ws(&p, end);
    if (p >= end || *p != '{') return 0;
    root->p = p;
    if (!value(&p, end, 0)) return 0;
    root->end = p;
    ws(&p, end);
    return p == end;
}
static int fields(view_t object, const char *const *keys, size_t count, view_t *out)
{
    const char *p = object.p;
    uint64_t seen = 0;
    size_t i;
    if (!p || p >= object.end || *p++ != '{' || count > 64U) return 0;
    memset(out, 0, count * sizeof(*out));
    ws(&p, object.end);
    if (p < object.end && *p == '}') return ++p == object.end;
    for (;;) {
        char key[64];
        if (!string(&p, object.end, key, sizeof(key))) return 0;
        for (i = 0; i < count && strcmp(key, keys[i]); ++i) {}
        if (i == count || (seen & (UINT64_C(1) << i))) return 0;
        seen |= UINT64_C(1) << i;
        ws(&p, object.end);
        if (p >= object.end || *p++ != ':') return 0;
        ws(&p, object.end); out[i].p = p;
        if (!value(&p, object.end, 1)) return 0;
        out[i].end = p; ws(&p, object.end);
        if (p >= object.end) return 0;
        if (*p == '}') return ++p == object.end;
        if (*p++ != ',') return 0;
        ws(&p, object.end);
    }
}
static int text(view_t v, char *out, size_t cap)
{
    const char *p = v.p;
    size_t i;
    if (!p || !string(&p, v.end, out, cap) || p != v.end) return 0;
    for (i = 0; out[i]; ++i) if ((unsigned char)out[i] < 32U || out[i] == 127) return 0;
    return 1;
}
static int equal(view_t v, const char *s)
{
    char buf[64];
    return text(v, buf, sizeof(buf)) && !strcmp(buf, s);
}
static int number(view_t v, uint32_t max, uint32_t *out)
{
    const char *p = v.p;
    uint32_t n = 0;
    if (!p || p >= v.end || (v.end - p > 1 && *p == '0')) return 0;
    while (p < v.end) {
        unsigned d;
        if (!digit(*p)) return 0;
        d = (unsigned)(*p++ - '0');
        if (d > max || n > (max - d) / 10U) return 0;
        n = n * 10U + d;
    }
    *out = n; return 1;
}
static int hex(view_t v, uint32_t max, uint32_t *out)
{
    char buf[19];
    uint32_t n = 0;
    size_t i;
    if (!text(v, buf, sizeof(buf)) || buf[0] != '0' || buf[1] != 'x' || !buf[2]) return 0;
    for (i = 2; buf[i]; ++i) {
        int d = hex_digit(buf[i]);
        if (d < 0 || (unsigned)d > max || n > (max - (unsigned)d) / 16U) return 0;
        n = n * 16U + (unsigned)d;
    }
    *out = n; return 1;
}
static int boolean(view_t v, int *out)
{
    if (!v.p) return 0;
    if (v.end - v.p == 4 && !memcmp(v.p, "true", 4)) { *out = 1; return 1; }
    if (v.end - v.p == 5 && !memcmp(v.p, "false", 5)) { *out = 0; return 1; }
    return 0;
}
/* Iterator: zero=end, one=item, minus one=invalid. The whole document is
 * validated first, so delimiters cannot be hidden in a string. */
static int item(view_t array, const char **cursor, view_t *out)
{
    const char *p = *cursor;
    if (!array.p) return 0;
    if (!p) {
        p = array.p;
        if (p >= array.end || *p++ != '[') return -1;
    } else {
        ws(&p, array.end);
        if (p < array.end && *p == ',') ++p;
        else if (p >= array.end || *p != ']') return -1;
    }
    ws(&p, array.end);
    if (p < array.end && *p == ']') { *cursor = p; return p + 1 == array.end ? 0 : -1; }
    out->p = p;
    if (!value(&p, array.end, 1)) return -1;
    out->end = p; *cursor = p;
    return 1;
}
static int empty_array(view_t v)
{
    const char *p = NULL;
    view_t unused;
    return !v.p || item(v, &p, &unused) == 0;
}
static int id_valid(const char *s)
{
    size_t i;
    if (!s || s[0] < 'a' || s[0] > 'z') return 0;
    for (i = 0; i < 32U && s[i]; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || digit(s[i]) || s[i] == '_' || s[i] == '-')) return 0;
    return i < 32U;
}
static int class_valid(const char *s)
{
    static const char *const names[] = {"generic", "phy", "power", "gpio", "sensor", "memory", "storage", "bridge", "serial"};
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) if (!strcmp(s, names[i])) return 1;
    return 0;
}
bvstk_status_t bvstk_device_profile_version(const char *json, unsigned *version)
{
    view_t root;
    const char *p;
    int seen = 0, modern = 0;
    uint32_t n = 0;
    if (!version || !document(json, &root)) return BVSTK_ERR_MALFORMED;
    p = root.p + 1; ws(&p, root.end);
    while (p < root.end && *p != '}') {
        char key[64];
        view_t v;
        if (!string(&p, root.end, key, sizeof(key))) return BVSTK_ERR_MALFORMED;
        ws(&p, root.end); ++p; ws(&p, root.end);
        v.p = p;
        if (!value(&p, root.end, 1)) return BVSTK_ERR_MALFORMED;
        v.end = p;
        if (!strcmp(key, "schema_version")) {
            if (seen++ || !number(v, UINT32_MAX, &n)) return BVSTK_ERR_MALFORMED;
        }
        if (!strcmp(key, "resources") || !strcmp(key, "transport") || !strcmp(key, "protocol") || !strcmp(key, "id")) modern = 1;
        ws(&p, root.end);
        if (*p == ',') { ++p; ws(&p, root.end); }
    }
    if (!seen && modern) return BVSTK_ERR_MALFORMED;
    if (seen && n != 2U) return BVSTK_ERR_UNSUPPORTED;
    *version = seen ? 2U : 0U;
    return BVSTK_OK;
}

static const char *resource_id(const bvstk_profile_metadata_t *p, unsigned reg, char fallback[16])
{
    if (p->version == 2U) return p->ids + p->id_offset[reg];
    (void)snprintf(fallback, 16, "reg_%02x", reg);
    return fallback;
}
bvstk_status_t bvstk_device_profile_metadata_validate(const bvstk_profile_metadata_t *p, size_t count)
{
    size_t i, j;
    if (!p || count > I2C_CFG_MAX_REG_COUNT) return BVSTK_ERR_MALFORMED;
    if (!p->version) return BVSTK_OK;
    if (p->version != 2U || !count || p->ids_size > sizeof(p->ids) || !p->ids_size || p->ids[0] ||
        !memchr(p->vendor, 0, sizeof(p->vendor)) || !memchr(p->model, 0, sizeof(p->model)) ||
        !memchr(p->device_class, 0, sizeof(p->device_class)) || !memchr(p->label, 0, sizeof(p->label)) ||
        !p->model[0] || !class_valid(p->device_class)) return BVSTK_ERR_MALFORMED;
    for (i = 0; i < count; ++i) {
        const char *id;
        if (!p->id_offset[i] || p->id_offset[i] >= p->ids_size) return BVSTK_ERR_MALFORMED;
        id = p->ids + p->id_offset[i];
        if (!memchr(id, 0, p->ids_size - p->id_offset[i]) || !id_valid(id)) return BVSTK_ERR_MALFORMED;
        for (j = 0; j < i; ++j)
            if (!strcmp(id, p->ids + p->id_offset[j])) return BVSTK_ERR_MALFORMED;
    }
    return BVSTK_OK;
}
static int lookup(view_t id, const bvstk_profile_metadata_t *p, unsigned count, unsigned *reg)
{
    char name[32];
    unsigned i;
    if (!text(id, name, sizeof(name))) return 0;
    for (i = 0; i < count; ++i) if (!strcmp(name, p->ids + p->id_offset[i])) { *reg = i; return 1; }
    return 0;
}
static int driver_mode(view_t v)
{
    static const char *const keys[] = {"mode"};
    view_t f[1];
    return fields(v, keys, 1, f) && equal(f[0], "driver");
}
static int zero_effect(view_t v, int write)
{
    static const char *const read_keys[] = {"clear_mask", "latch_high_mask", "latch_low_mask"};
    static const char *const write_keys[] = {"one_clear_mask", "one_set_mask", "self_clear_mask", "fixed_mask", "fixed_value"};
    view_t f[5];
    uint32_t n;
    size_t i, count = write ? 5U : 3U;
    if (!v.p) return 1;
    if (!fields(v, write ? write_keys : read_keys, count, f)) return 0;
    for (i = 0; i < count; ++i) if (f[i].p && (!hex(f[i], 0, &n))) return 0;
    return 1;
}
static int no_refresh(view_t v)
{
    static const char *const keys[] = {"on", "period_ms"};
    view_t f[2];
    uint32_t n;
    return !v.p || (fields(v, keys, 2, f) && empty_array(f[0]) && (!f[1].p || number(f[1], 0, &n)));
}

static bvstk_status_t parse(const char *json, int mdio, i2c_device_config_t *i2c, mdio_phy_config_t *m)
{
    static const char *const root_keys[] = {"schema_version", "id", "device", "transport", "protocol", "resources", "defaults", "policy", "settings", "observability"};
    static const char *const device_keys[] = {"model", "vendor", "class", "label"};
    static const char *const transport_keys[] = {"bus", "controller", "address", "address_bits"};
    static const char *const protocol_keys[] = {"driver", "options"};
    static const char *const option_keys[] = {"max_write_value", "capture_cache_on_save", "autopoll_enabled", "autopoll_order", "autopoll_reg_delay_ms", "autopoll_cycle_delay_ms"};
    static const char *const resource_keys[] = {"id", "kind", "address", "width_bits", "access", "write_mask", "cache", "verify", "refresh", "persist_mask", "read_effect", "write_effect", "fields"};
    static const char *const defaults_keys[] = {"register"};
    static const char *const policy_keys[] = {"read", "write"};
    static const char *const policy_entry_keys[] = {"mode", "rules", "inactive_rules"};
    static const char *const rule_keys[] = {"resource", "match"};
    static const char *const match_keys[] = {"value", "mask"};
    static const char *const settings_keys[] = {"cache_seed", "desired", "apply_on_startup"};
    static const char *const seed_keys[] = {"resource", "value"};
    static const char *const observe_keys[] = {"telemetry", "events"};
    bvstk_profile_metadata_t *meta = mdio ? &m->profile : &i2c->profile;
    char *name = mdio ? m->name : i2c->name;
    view_t root, r[10], f[13], defaults[13] = {{0}}, options[6] = {{0}}, policies[2];
    view_t entry, part[3], list[2], rule[2], match[2];
    const char *cursor;
    uint8_t seen[256] = {0};
    unsigned count = 0, reg, width = mdio ? 16U : 8U, maximum = mdio ? 65535U : 255U;
    uint32_t n, val, mask;
    int next, flag, mode;
    size_t k, which, nr;
    if (!document(json, &root) || !fields(root, root_keys, 10, r) ||
        !number(r[0], 2, &n) || n != 2U || !text(r[1], name, 32) || !id_valid(name)) return BVSTK_ERR_MALFORMED;
    if (!fields(r[2], device_keys, 4, f) || !text(f[0], meta->model, sizeof(meta->model)) || !meta->model[0]) return BVSTK_ERR_MALFORMED;
    if (f[1].p && (!text(f[1], meta->vendor, sizeof(meta->vendor)) || !meta->vendor[0])) return BVSTK_ERR_MALFORMED;
    strcpy(meta->device_class, "generic");
    if (f[2].p && !text(f[2], meta->device_class, sizeof(meta->device_class))) return BVSTK_ERR_MALFORMED;
    if (!class_valid(meta->device_class)) return BVSTK_ERR_MALFORMED;
    if (f[3].p && (!text(f[3], meta->label, sizeof(meta->label)) || !meta->label[0])) return BVSTK_ERR_MALFORMED;
    if (!fields(r[3], transport_keys, 4, f) || !equal(f[0], mdio ? "mdio" : "i2c") ||
        !equal(f[1], mdio ? "pl_mdio_0" : "pl_i2c_0") || !hex(f[2], mdio ? 31U : 127U, &n)) return BVSTK_ERR_UNSUPPORTED;
    if (mdio) { if (f[3].p) return BVSTK_ERR_MALFORMED; m->phy_addr = (uint8_t)n; }
    else {
        i2c->addr_7b = (uint8_t)n;
        if (f[3].p && (!number(f[3], 7, &n) || n != 7U)) return BVSTK_ERR_UNSUPPORTED;
    }
    if (!fields(r[4], protocol_keys, 2, f) || !equal(f[0], mdio ? "bvstk_mdio_bridge" : "bvstk_i2c_bridge")) return BVSTK_ERR_UNSUPPORTED;
    if (f[1].p && !fields(f[1], option_keys, 6, options)) return BVSTK_ERR_MALFORMED;
    if (mdio) {
        if (options[0].p || options[1].p) return BVSTK_ERR_MALFORMED;
        m->autopoll_cycle_delay_ms = 50U;
        if (options[2].p) { if (!boolean(options[2], &flag)) return BVSTK_ERR_MALFORMED; m->autopoll_enabled = flag != 0; }
        if (options[4].p && !number(options[4], 60000, &m->autopoll_reg_delay_ms)) return BVSTK_ERR_MALFORMED;
        if (options[5].p && !number(options[5], 600000, &m->autopoll_cycle_delay_ms)) return BVSTK_ERR_MALFORMED;
    } else {
        if (!hex(options[0], 64, &n)) return BVSTK_ERR_MALFORMED;
        i2c->max_value_code = (uint8_t)n;
        if (options[1].p && (!boolean(options[1], &flag) || !flag)) return BVSTK_ERR_UNSUPPORTED;
        for (k = 2; k < 6; ++k) if (options[k].p) return BVSTK_ERR_UNSUPPORTED;
    }
    if (r[6].p) {
        if (!fields(r[6], defaults_keys, 1, f)) return BVSTK_ERR_UNSUPPORTED;
        if (f[0].p && (!fields(f[0], resource_keys, 13, defaults) || defaults[0].p || defaults[1].p || defaults[2].p)) return BVSTK_ERR_MALFORMED;
    }
    meta->ids_size = 1;
    cursor = NULL;
    while ((next = item(r[5], &cursor, &entry)) == 1) {
        char id[32];
        size_t len;
        if (!fields(entry, resource_keys, 13, f) || !text(f[0], id, sizeof(id)) || !id_valid(id) ||
            !equal(f[1], "register") || !hex(f[2], mdio ? 31U : 255U, &n)) return BVSTK_ERR_MALFORMED;
        reg = n;
        if (seen[reg] || count >= (mdio ? 32U : 256U)) return BVSTK_ERR_MALFORMED;
        for (k = 0; k < 256; ++k)
            if (seen[k] && !strcmp(id, meta->ids + meta->id_offset[k])) return BVSTK_ERR_MALFORMED;
        seen[reg] = 1; ++count;
        len = strlen(id) + 1;
        if (len > sizeof(meta->ids) - meta->ids_size) return BVSTK_ERR_RANGE;
        meta->id_offset[reg] = meta->ids_size;
        memcpy(meta->ids + meta->ids_size, id, len); meta->ids_size += (uint16_t)len;
        for (k = 3; k < 13; ++k) if (!f[k].p) f[k] = defaults[k];
        if (!number(f[3], width, &n) || n != width || !equal(f[4], "rw") ||
            !hex(f[5], maximum, &n) || n != maximum || !driver_mode(f[6]) || !driver_mode(f[7]) ||
            !no_refresh(f[8]) || (f[9].p && !hex(f[9], 0, &n)) ||
            !zero_effect(f[10], 0) || !zero_effect(f[11], 1) || !empty_array(f[12])) return BVSTK_ERR_UNSUPPORTED;
    }
    if (next < 0 || !count) return BVSTK_ERR_MALFORMED;
    for (k = 0; k < count; ++k) if (!seen[k]) return BVSTK_ERR_UNSUPPORTED;
    if (mdio) m->reg_count = (uint8_t)count; else i2c->reg_count = (uint16_t)count;
    if (mdio) {
        memset(seen, 0, sizeof(seen)); cursor = NULL;
        while ((next = item(options[3], &cursor, &entry)) == 1) {
            if (!lookup(entry, meta, count, &reg) || seen[reg] || m->autopoll_regs_len >= 32U) return BVSTK_ERR_MALFORMED;
            seen[reg] = 1; m->autopoll_regs[m->autopoll_regs_len++] = (uint8_t)reg;
        }
        if (next < 0) return BVSTK_ERR_MALFORMED;
    }
    if (!fields(r[7], policy_keys, 2, policies) || !fields(policies[0], policy_entry_keys, 3, part) ||
        !equal(part[0], "blacklist") || !empty_array(part[1]) || !empty_array(part[2])) return BVSTK_ERR_UNSUPPORTED;
    /* Read restrictions cannot currently be guaranteed for the PL slave path.
     * Only the explicit, unrestricted read policy is accepted by this adapter. */
    mode = 0;
    if (policies[1].p) {
        if (!fields(policies[1], policy_entry_keys, 3, part)) return BVSTK_ERR_MALFORMED;
        if (equal(part[0], "blacklist")) mode = 1;
        else if (!equal(part[0], "whitelist")) return BVSTK_ERR_MALFORMED;
        list[mode ? 1 : 0] = part[1]; list[mode ? 0 : 1] = part[2];
    } else memset(list, 0, sizeof(list));
    if (mdio) m->policy = mode ? MDIO_POLICY_BLACKLIST : MDIO_POLICY_WHITELIST;
    else i2c->policy = mode ? I2C_POLICY_BLACKLIST : I2C_POLICY_WHITELIST;
    for (which = 0; which < 2; ++which) {
        nr = 0; cursor = NULL;
        while ((next = item(list[which], &cursor, &entry)) == 1) {
            if (nr >= (mdio ? MDIO_CFG_RULES_MAX : I2C_CFG_RULES_MAX) ||
                !fields(entry, rule_keys, 2, rule) || !lookup(rule[0], meta, count, &reg)) return BVSTK_ERR_MALFORMED;
            val = 0; mask = 0;
            if (rule[1].p) {
                if (!fields(rule[1], match_keys, 2, match)) return BVSTK_ERR_MALFORMED;
                if (match[0].p || match[1].p) {
                    if (!hex(match[0], maximum, &val) || !hex(match[1], maximum, &mask)) return BVSTK_ERR_MALFORMED;
                }
            }
            if (mdio) {
                mdio_rule_entry_t *out = which ? m->blacklist : m->whitelist;
                out[nr].reg = (uint8_t)reg; out[nr].val = (uint16_t)val; out[nr].mask = (uint16_t)mask;
            } else {
                i2c_rule_entry_t *out = which ? i2c->blacklist : i2c->whitelist;
                if (mask != 255U) return BVSTK_ERR_UNSUPPORTED;
                if (val > i2c->max_value_code) return BVSTK_ERR_RANGE;
                out[nr].reg = (uint8_t)reg; out[nr].val = (uint8_t)val;
            }
            ++nr;
        }
        if (next < 0) return BVSTK_ERR_MALFORMED;
        if (mdio) { if (which) m->blacklist_len = nr; else m->whitelist_len = nr; }
        else { if (which) i2c->blacklist_len = nr; else i2c->whitelist_len = nr; }
    }
    if (r[8].p) {
        if (!fields(r[8], settings_keys, 3, part) || !empty_array(part[1]) ||
            (part[2].p && (!boolean(part[2], &flag) || flag))) return BVSTK_ERR_UNSUPPORTED;
        memset(seen, 0, sizeof(seen)); cursor = NULL; nr = 0;
        while ((next = item(part[0], &cursor, &entry)) == 1) {
            if (nr >= (mdio ? MDIO_CFG_SETTINGS_MAX : I2C_CFG_SETTINGS_MAX) ||
                !fields(entry, seed_keys, 2, rule) || !lookup(rule[0], meta, count, &reg) || seen[reg] ||
                !hex(rule[1], maximum, &val)) return BVSTK_ERR_MALFORMED;
            seen[reg] = 1;
            if (mdio) { m->settings[nr].reg = (uint8_t)reg; m->settings[nr].val = (uint16_t)val; }
            else { i2c->settings[nr].reg = (uint8_t)reg; i2c->settings[nr].val = (uint8_t)val; }
            ++nr;
        }
        if (next < 0) return BVSTK_ERR_MALFORMED;
        if (mdio) m->settings_len = nr; else i2c->settings_len = nr;
    }
    if (r[9].p) {
        if (!fields(r[9], observe_keys, 2, part)) return BVSTK_ERR_MALFORMED;
        for (k = 0; k < 2; ++k)
            if (part[k].p && (!boolean(part[k], &flag) || !flag)) return BVSTK_ERR_UNSUPPORTED;
    }
    meta->version = 2;
    return mdio ? bvstk_mdio_config_validate(m) : bvstk_i2c_config_validate(i2c);
}
bvstk_status_t bvstk_device_profile_parse_i2c(const char *json, i2c_device_config_t *out)
{
    i2c_device_config_t next;
    bvstk_status_t st;
    if (!out) return BVSTK_ERR_MALFORMED;
    memset(&next, 0, sizeof(next));
    st = parse(json, 0, &next, NULL);
    if (st == BVSTK_OK) *out = next;
    return st;
}
bvstk_status_t bvstk_device_profile_parse_mdio(const char *json, mdio_phy_config_t *out)
{
    mdio_phy_config_t next;
    bvstk_status_t st;
    if (!out) return BVSTK_ERR_MALFORMED;
    memset(&next, 0, sizeof(next));
    st = parse(json, 1, NULL, &next);
    if (st == BVSTK_OK) *out = next;
    return st;
}

static void append(writer_t *w, const char *format, ...)
{
    int n;
    va_list args;
    if (w->failed) return;
    va_start(args, format);
    n = vsnprintf(w->p + w->n, w->cap - w->n, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= w->cap - w->n) { w->failed = 1; return; }
    w->n += (size_t)n;
}
static void quoted(writer_t *w, const char *text_value)
{
    const unsigned char *p = (const unsigned char *)text_value;
    append(w, "\"");
    while (*p) {
        if (*p < 32U || *p == 127U) { w->failed = 1; return; }
        if (*p == '"' || *p == '\\') append(w, "\\%c", *p);
        else append(w, "%c", *p);
        ++p;
    }
    append(w, "\"");
}
static void reference(writer_t *w, const bvstk_profile_metadata_t *p, unsigned reg)
{
    char fallback[16];
    quoted(w, resource_id(p, reg, fallback));
}
static bvstk_status_t format(int mdio, const i2c_device_config_t *i2c, const mdio_phy_config_t *m,
                             char *out, size_t cap, size_t *length)
{
    const bvstk_profile_metadata_t *meta = mdio ? &m->profile : &i2c->profile;
    const char *name = mdio ? m->name : i2c->name;
    unsigned count = mdio ? m->reg_count : i2c->reg_count;
    unsigned address = mdio ? m->phy_addr : i2c->addr_7b;
    unsigned width = mdio ? 16U : 8U;
    int black = mdio ? m->policy == MDIO_POLICY_BLACKLIST : i2c->policy == I2C_POLICY_BLACKLIST;
    size_t i, j, which, nr, nseed = mdio ? m->settings_len : i2c->settings_len;
    writer_t w = {out, cap, 0, 0};
    if (length) *length = 0;
    if (!out || !cap) return BVSTK_ERR_MALFORMED;
    out[0] = 0;
    if (!id_valid(name)) return BVSTK_ERR_UNSUPPORTED;
    append(&w, "{\n  \"schema_version\": 2,\n  \"id\": "); quoted(&w, name);
    append(&w, ",\n  \"device\": {\"model\": "); quoted(&w, meta->version ? meta->model : name);
    append(&w, ", \"class\": "); quoted(&w, meta->version ? meta->device_class : mdio ? "phy" : "generic");
    if (meta->version && meta->vendor[0]) { append(&w, ", \"vendor\": "); quoted(&w, meta->vendor); }
    if (meta->version && meta->label[0]) { append(&w, ", \"label\": "); quoted(&w, meta->label); }
    append(&w, "},\n  \"transport\": {\"bus\": \"%s\", \"controller\": \"pl_%s_0\", \"address\": \"0x%02X\"%s},\n",
           mdio ? "mdio" : "i2c", mdio ? "mdio" : "i2c", address, mdio ? "" : ", \"address_bits\": 7");
    append(&w, "  \"protocol\": {\"driver\": \"bvstk_%s_bridge\", \"options\": {", mdio ? "mdio" : "i2c");
    if (mdio) {
        append(&w, "\"autopoll_enabled\": %s, \"autopoll_reg_delay_ms\": %lu, \"autopoll_cycle_delay_ms\": %lu, \"autopoll_order\": [",
               m->autopoll_enabled ? "true" : "false", (unsigned long)m->autopoll_reg_delay_ms, (unsigned long)m->autopoll_cycle_delay_ms);
        for (i = 0; i < m->autopoll_regs_len; ++i) { if (i) append(&w, ", "); reference(&w, meta, m->autopoll_regs[i]); }
        append(&w, "]");
    } else append(&w, "\"max_write_value\": \"0x%02X\", \"capture_cache_on_save\": true", i2c->max_value_code);
    append(&w, "}},\n  \"defaults\": {\"register\": {\"width_bits\": %u, \"access\": \"rw\", \"write_mask\": \"0x%0*X\", \"cache\": {\"mode\": \"driver\"}, \"verify\": {\"mode\": \"driver\"}}},\n  \"resources\": [\n",
           width, (int)(width / 4U), mdio ? 65535U : 255U);
    for (i = 0; i < count; ++i) {
        append(&w, "    {\"id\": "); reference(&w, meta, (unsigned)i);
        append(&w, ", \"kind\": \"register\", \"address\": \"0x%02X\"}%s\n", (unsigned)i, i + 1U == count ? "" : ",");
    }
    append(&w, "  ],\n  \"policy\": {\"read\": {\"mode\": \"blacklist\", \"rules\": []}, \"write\": {\"mode\": \"%s\"",
           black ? "blacklist" : "whitelist");
    for (which = 0; which < 2; ++which) {
        int deny = which ? !black : black;
        nr = mdio ? (deny ? m->blacklist_len : m->whitelist_len) : (deny ? i2c->blacklist_len : i2c->whitelist_len);
        append(&w, ", \"%s\": [", which ? "inactive_rules" : "rules");
        for (i = 0; i < nr; ++i) {
            unsigned reg, val, mask;
            if (mdio) {
                const mdio_rule_entry_t *rule = (deny ? m->blacklist : m->whitelist) + i;
                reg = rule->reg; val = rule->val; mask = rule->mask;
            } else {
                const i2c_rule_entry_t *rule = (deny ? i2c->blacklist : i2c->whitelist) + i;
                reg = rule->reg; val = rule->val; mask = 255;
            }
            append(&w, "%s{\"resource\": ", i ? ", " : ""); reference(&w, meta, reg);
            append(&w, ", \"match\": {\"value\": \"0x%0*X\", \"mask\": \"0x%0*X\"}}",
                   (int)(width / 4U), val, (int)(width / 4U), mask);
        }
        append(&w, "]");
    }
    append(&w, "}},\n  \"settings\": {\"desired\": [], \"cache_seed\": [");
    nr = 0;
    for (i = 0; i < nseed; ++i) {
        unsigned reg = mdio ? m->settings[i].reg : i2c->settings[i].reg;
        unsigned val = mdio ? m->settings[i].val : i2c->settings[i].val;
        /* Legacy I2C seed application is last-value-wins. Canonical v2 has
         * one seed per register, retaining precisely that effective value. */
        if (!mdio) {
            for (j = i + 1U; j < nseed && i2c->settings[j].reg != reg; ++j) {}
            if (j < nseed) continue;
        }
        append(&w, "%s{\"resource\": ", nr++ ? ", " : ""); reference(&w, meta, reg);
        append(&w, ", \"value\": \"0x%0*X\"}", (int)(width / 4U), val);
    }
    append(&w, "], \"apply_on_startup\": false},\n  \"observability\": {\"telemetry\": true, \"events\": true}\n}\n");
    if (w.failed || w.n > BVSTK_PROFILE_JSON_MAX) { out[0] = 0; return BVSTK_ERR_RANGE; }
    if (length) *length = w.n;
    return BVSTK_OK;
}
bvstk_status_t bvstk_device_profile_format_i2c(const i2c_device_config_t *c, char *out, size_t cap, size_t *length)
{
    bvstk_status_t st = bvstk_i2c_config_validate(c);
    if (length) *length = 0;
    return st == BVSTK_OK ? format(0, c, NULL, out, cap, length) : st;
}
bvstk_status_t bvstk_device_profile_format_mdio(const mdio_phy_config_t *c, char *out, size_t cap, size_t *length)
{
    bvstk_status_t st = bvstk_mdio_config_validate(c);
    if (length) *length = 0;
    return st == BVSTK_OK ? format(1, NULL, c, out, cap, length) : st;
}
