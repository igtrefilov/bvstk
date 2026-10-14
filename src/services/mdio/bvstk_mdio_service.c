#include "services/mdio/bvstk_mdio_service.h"
#include <string.h>
#include <strings.h>

static bvstk_status_t lock(const bvstk_mdio_service_t *s)
{
    if (!s || !s->initialized) return BVSTK_ERR_NOT_READY;
    return bvstk_mutex_lock(&s->mutex, 1000U);
}
static bvstk_status_t check(const bvstk_mdio_service_t *s, size_t d, uint8_t r)
{
    if (d >= s->device_count) return BVSTK_ERR_NOT_FOUND;
    return r < s->devices[d].reg_count ? BVSTK_OK : BVSTK_ERR_RANGE;
}
static size_t find_phy(const bvstk_mdio_service_t *s, uint8_t phy)
{
    size_t i;
    for (i = 0; i < s->device_count; ++i) if (s->devices[i].phy_addr == phy) break;
    return i;
}
static bvstk_status_t publish(bvstk_mdio_service_t *s, uint8_t phy, uint8_t reg, uint16_t value)
{
    if (s->bus.publish) return s->bus.publish(s->bus.context, phy, reg, value);
    return s->core ? bvstk_mdio_core_publish(s->core, phy, reg, value) : BVSTK_OK;
}
static void emit_flags(bvstk_mdio_service_t *s, uint16_t type, bvstk_status_t st,
    bvstk_event_source_t source, uint8_t phy, uint8_t reg, uint16_t value,
    uint8_t flags)
{
    bvstk_event_t e;
    if (!s->events.publish) return;
    memset(&e, 0, sizeof(e));
    e.time_us = bvstk_platform_now_ms() * UINT64_C(1000);
    e.type = type; e.status = (uint16_t)st; e.source = (uint8_t)source;
    e.bus = BVSTK_EVENT_BUS_MDIO;
    e.operation = type == BVSTK_EVENT_MASTER_READ ? BVSTK_EVENT_OP_READ : BVSTK_EVENT_OP_WRITE;
    e.flags = flags;
    e.arg0 = phy; e.arg1 = reg; e.arg2 = value;
    s->events.publish(s->events.context, &e);
}
static void cache_changed(bvstk_mdio_service_t *s, size_t d, unsigned r)
{
    const bvstk_mdio_cache_entry_t *e = &s->cache[d][r];
    bvstk_cache_sink_entry(&s->cache_sink, d, r, e->value, e->valid, e->seeded, e->updated_ms);
}
static void emit(bvstk_mdio_service_t *s, uint16_t type, bvstk_status_t st,
    bvstk_event_source_t source, uint8_t phy, uint8_t reg, uint16_t value)
{
    emit_flags(s, type, st, source, phy, reg, value, BVSTK_EVENT_FLAG_NONE);
}
static uint8_t telemetry_flags(const bvstk_mdio_service_t *s, size_t d,
    uint16_t type, bvstk_status_t st, uint16_t value, int probe)
{
    if (probe || d >= s->device_count) return BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED;
    if (s->online[d]) return BVSTK_EVENT_FLAG_NONE;
    /* A successful non-FFFF read is itself useful evidence of a PHY. Writes
     * have no MDIO acknowledgement and therefore remain hidden until the
     * identity probe has confirmed the device. */
    if (type == BVSTK_EVENT_MASTER_READ && st == BVSTK_OK && value != 0xFFFFU)
        return BVSTK_EVENT_FLAG_NONE;
    return BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED;
}
static bvstk_status_t invalidate(bvstk_mdio_service_t *s, size_t d)
{
    uint8_t r;
    bvstk_status_t result = BVSTK_OK;
    memset(s->cache[d], 0, sizeof(s->cache[d]));
    s->online[d] = 0;
    s->probe_due[d] = 0;
    for (r = 0; r < s->devices[d].reg_count; ++r) cache_changed(s, d, r);
    bvstk_cache_sink_presence(&s->cache_sink, d, 2U);
    for (r = 0; r < 32U; ++r)
        if (publish(s, s->devices[d].phy_addr, r, 0xFFFFU) != BVSTK_OK) result = BVSTK_ERR_IO;
    return result;
}
static bvstk_status_t read_phy(bvstk_mdio_service_t *s, size_t d, uint8_t r,
    uint16_t *v, bvstk_event_source_t source, uint32_t timeout, int probe)
{
    bvstk_status_t st;
    bvstk_mdio_cache_entry_t *e = &s->cache[d][r];
    uint8_t phy = s->devices[d].phy_addr;
    if (s->bus.read) st = s->bus.read(s->bus.context, phy, r, v, timeout);
    else st = s->core ? bvstk_mdio_core_read(s->core, phy, r, v, timeout) : BVSTK_ERR_NOT_READY;
    emit_flags(s, BVSTK_EVENT_MASTER_READ, st, source, phy, r,
        st == BVSTK_OK ? *v : 0,
        telemetry_flags(s, d, BVSTK_EVENT_MASTER_READ, st,
            st == BVSTK_OK ? *v : 0, probe));
    ++s->stats.reads;
    if (st == BVSTK_OK) {
        st = publish(s, phy, r, *v);
        if (st == BVSTK_OK) {
            e->value = *v; e->valid = 1; e->seeded = 0;
            e->updated_ms = bvstk_platform_now_ms();
        }
    }
    if (st != BVSTK_OK) {
        e->valid = 0;
        (void)publish(s, phy, r, 0xFFFFU);
        /* Identity probes are presence checks, not device transactions.
         * A missing or temporarily unreachable PHY must not turn the
         * diagnostic exchange-error counter into a probe-attempt counter. */
        if (!probe) ++s->stats.errors;
    }
    /* Publish identity probes only after the complete ID pair is accepted.
     * An undriven MDIO line (FFFF) must never look like a cached PHY sample. */
    if (!probe) cache_changed(s, d, r);
    return st;
}
static bvstk_status_t write_phy(bvstk_mdio_service_t *s, size_t d, uint8_t r,
    uint16_t v, bvstk_event_source_t source, uint32_t timeout)
{
    bvstk_status_t st;
    uint8_t phy = s->devices[d].phy_addr;
    int was_online = s->online[d] != 0U;
    s->stats.last_phy = phy; s->stats.last_reg = r;
    s->stats.last_value = v; s->stats.last_source = (uint8_t)source;
    emit(s, BVSTK_EVENT_REG_ATTEMPT, BVSTK_OK, source, phy, r, v);
    if (!bvstk_mdio_policy_allows(&s->devices[d], r, v)) {
        ++s->stats.denied;
        st = BVSTK_ERR_DENIED;
        emit(s, BVSTK_EVENT_REG_DENIED, st, source, phy, r, v);
    } else {
        /* A cached command (especially RESET/RESTART) is not a PHY reply.
         * Invalidate before dispatch, then refresh from actual hardware. */
        if (r == 0U && (v & 0x8000U)) (void)invalidate(s, d);
        else {
            s->cache[d][r].valid = 0;
            cache_changed(s, d, r);
            (void)publish(s, phy, r, 0xFFFFU);
        }
        if (s->bus.write) st = s->bus.write(s->bus.context, phy, r, v, source, timeout);
        else st = s->core ? bvstk_mdio_core_write(s->core, phy, r, v, timeout) : BVSTK_ERR_NOT_READY;
        emit_flags(s, BVSTK_EVENT_MASTER_WRITE, st, source, phy, r, v,
            was_online ? BVSTK_EVENT_FLAG_NONE : BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED);
        if (st == BVSTK_OK) {
            uint16_t actual;
            ++s->stats.writes;
            /* Write success means frame completed, not PHY acceptance. Never retry. */
            (void)read_phy(s, d, r, &actual, source, timeout, 0);
            s->poll_due[d] = 0; s->poll_index[d] = 0;
            emit(s, BVSTK_EVENT_REG_COMMIT, st, source, phy, r, v);
        } else {
            ++s->stats.errors;
            emit(s, BVSTK_EVENT_FAULT, st, source, phy, r, v);
        }
    }
    s->stats.last_status = st;
    return st;
}
static bvstk_status_t seed(bvstk_mdio_service_t *s, size_t d)
{
    size_t i;
    for (i = 0; i < s->devices[d].settings_len; ++i) {
        mdio_setting_entry_t v = s->devices[d].settings[i];
        bvstk_mdio_cache_entry_t *e = &s->cache[d][v.reg];
        if (publish(s, s->devices[d].phy_addr, v.reg, v.val) != BVSTK_OK) return BVSTK_ERR_IO;
        e->value = v.val; e->valid = 1; e->seeded = 1;
        e->updated_ms = bvstk_platform_now_ms();
        cache_changed(s, d, v.reg);
    }
    return BVSTK_OK;
}
bvstk_status_t bvstk_mdio_service_init(bvstk_mdio_service_t *s, bvstk_mdio_core_t *core,
    const bvstk_mdio_bus_ops_t *bus, const mdio_phy_config_t *devices,
    size_t count, const bvstk_event_sink_t *events)
{
    size_t i, j;
    if (!s || count > MDIO_CFG_MAX_DEVICES || (count && !devices)) return BVSTK_ERR_MALFORMED;
    for (i = 0; i < count; ++i) {
        if (bvstk_mdio_config_validate(&devices[i]) != BVSTK_OK) return BVSTK_ERR_MALFORMED;
        for (j = 0; j < i; ++j)
            if (devices[i].phy_addr == devices[j].phy_addr ||
                !strcasecmp(devices[i].name, devices[j].name)) return BVSTK_ERR_MALFORMED;
    }
    memset(s, 0, sizeof(*s));
    s->core = core;
    if (bus) s->bus = *bus;
    if (events) s->events = *events;
    if (count) memcpy(s->devices, devices, count * sizeof(*devices));
    s->device_count = count;
    if (core && bvstk_mdio_core_prepare_slave(core) != BVSTK_OK) return BVSTK_ERR_IO;
    for (i = 0; i < count; ++i) if (seed(s, i) != BVSTK_OK) return BVSTK_ERR_IO;
    s->initialized = 1;
    return BVSTK_OK;
}
void bvstk_mdio_service_shutdown(bvstk_mdio_service_t *s)
{
    if (s) memset(s, 0, sizeof(*s)); /* Caller must stop/join its worker first. */
}
size_t bvstk_mdio_service_device_count(const bvstk_mdio_service_t *s)
{
    return s && s->initialized ? s->device_count : 0; /* Immutable after init. */
}
bvstk_status_t bvstk_mdio_service_get_config(const bvstk_mdio_service_t *s, size_t d, mdio_phy_config_t *out)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, 0);
    if (!out) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK) *out = s->devices[d];
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_device_info(const bvstk_mdio_service_t *s, size_t d, bvstk_mdio_device_info_t *out)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, 0);
    if (!out) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK) {
        memcpy(out->name, s->devices[d].name, sizeof(out->name));
        out->phy_addr = s->devices[d].phy_addr;
        out->reg_count = s->devices[d].reg_count;
        out->policy = s->devices[d].policy;
    }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_find_by_name(const bvstk_mdio_service_t *s, const char *name, size_t *d)
{
    size_t i;
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = BVSTK_ERR_NOT_FOUND;
    if (!name || !d) st = BVSTK_ERR_MALFORMED;
    else for (i = 0; i < s->device_count; ++i)
        if (!strcasecmp(name, s->devices[i].name)) { *d = i; st = BVSTK_OK; break; }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_find_by_phy(const bvstk_mdio_service_t *s, uint8_t phy, size_t *d)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    if (!d || phy > 31U) st = BVSTK_ERR_RANGE;
    else { *d = find_phy(s, phy); st = *d < s->device_count ? BVSTK_OK : BVSTK_ERR_NOT_FOUND; }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_read(bvstk_mdio_service_t *s, size_t d, uint8_t r, uint16_t *v, uint32_t timeout)
{
    return bvstk_mdio_service_read_from(s, d, r, v, BVSTK_EVENT_SOURCE_INTERNAL, timeout);
}
bvstk_status_t bvstk_mdio_service_read_from(bvstk_mdio_service_t *s, size_t d, uint8_t r,
    uint16_t *v, bvstk_event_source_t source, uint32_t timeout)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, r);
    if (!v) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK) st = read_phy(s, d, r, v, source, timeout, 0);
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_cache_entry(const bvstk_mdio_service_t *s, size_t d, uint8_t r, bvstk_mdio_cache_entry_t *e)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, r);
    if (!e) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK) *e = s->cache[d][r];
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_read_cached(const bvstk_mdio_service_t *s, size_t d, uint8_t r, uint16_t *v)
{
    bvstk_mdio_cache_entry_t e;
    bvstk_status_t st;
    if (!v) return BVSTK_ERR_MALFORMED;
    st = bvstk_mdio_service_cache_entry(s, d, r, &e);
    if (st != BVSTK_OK) return st;
    if (!e.valid) return BVSTK_ERR_NOT_READY;
    *v = e.value; return BVSTK_OK;
}
bvstk_status_t bvstk_mdio_service_invalidate(bvstk_mdio_service_t *s, size_t d)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, 0);
    if (st == BVSTK_OK) st = invalidate(s, d);
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_write(bvstk_mdio_service_t *s, size_t d, uint8_t r, uint16_t v, bvstk_event_source_t source, uint32_t timeout)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, r);
    if (st == BVSTK_OK) st = write_phy(s, d, r, v, source, timeout);
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_set_policy(bvstk_mdio_service_t *s, size_t d, mdio_policy_t policy)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, 0);
    if (policy != MDIO_POLICY_WHITELIST && policy != MDIO_POLICY_BLACKLIST) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK) s->devices[d].policy = policy;
    bvstk_mutex_unlock(&s->mutex);
    return st;
}

bvstk_status_t bvstk_mdio_service_get_policy(const bvstk_mdio_service_t *s,
    size_t d, bvstk_mdio_policy_entry_t *out)
{
    const mdio_phy_config_t *c;
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, 0);
    if (!out) st = BVSTK_ERR_MALFORMED;
    if (st == BVSTK_OK) {
        c = &s->devices[d];
        memset(out, 0, sizeof(*out));
        out->mode = c->policy;
        out->whitelist_len = c->whitelist_len;
        out->blacklist_len = c->blacklist_len;
        memcpy(out->whitelist, c->whitelist, c->whitelist_len * sizeof(c->whitelist[0]));
        memcpy(out->blacklist, c->blacklist, c->blacklist_len * sizeof(c->blacklist[0]));
    }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}

/* Caller holds the service mutex. Select lists without changing the mode. */
static bvstk_status_t policy_list(bvstk_mdio_service_t *s, size_t d,
    mdio_policy_t list, mdio_rule_entry_t **rules, size_t **length)
{
    bvstk_status_t st = check(s, d, 0);
    if (st != BVSTK_OK) return st;
    if (list == MDIO_POLICY_WHITELIST) {
        *rules = s->devices[d].whitelist;
        *length = &s->devices[d].whitelist_len;
    } else if (list == MDIO_POLICY_BLACKLIST) {
        *rules = s->devices[d].blacklist;
        *length = &s->devices[d].blacklist_len;
    } else return BVSTK_ERR_MALFORMED;
    return BVSTK_OK;
}

static int policy_rule_equal(const mdio_rule_entry_t *rule,
    uint8_t reg, uint16_t value, uint16_t mask)
{
    return rule->reg == reg && rule->mask == mask &&
        (rule->val & mask) == (value & mask);
}

bvstk_status_t bvstk_mdio_service_rule_add(bvstk_mdio_service_t *s,
    size_t d, mdio_policy_t list, uint8_t reg, uint16_t value, uint16_t mask)
{
    mdio_rule_entry_t *rules;
    size_t *length, i;
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = policy_list(s, d, list, &rules, &length);
    if (st == BVSTK_OK) st = check(s, d, reg);
    if (st == BVSTK_OK) {
        for (i = 0; i < *length; ++i)
            if (policy_rule_equal(&rules[i], reg, value, mask)) break;
        if (i == *length) {
            if (*length >= MDIO_CFG_RULES_MAX) st = BVSTK_ERR_RANGE;
            else {
                rules[*length].reg = reg;
                rules[*length].val = (uint16_t)(value & mask);
                rules[*length].mask = mask;
                ++*length;
            }
        }
    }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}

bvstk_status_t bvstk_mdio_service_rule_remove(bvstk_mdio_service_t *s,
    size_t d, mdio_policy_t list, uint8_t reg, uint16_t value, uint16_t mask)
{
    mdio_rule_entry_t *rules;
    size_t *length, i, kept = 0;
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = policy_list(s, d, list, &rules, &length);
    if (st == BVSTK_OK) st = check(s, d, reg);
    if (st == BVSTK_OK) {
        for (i = 0; i < *length; ++i)
            if (policy_rule_equal(&rules[i], reg, value, mask)) break;
        if (i == *length) st = BVSTK_ERR_NOT_FOUND;
    }
    if (st == BVSTK_OK) {
        for (i = 0; i < *length; ++i)
            if (!policy_rule_equal(&rules[i], reg, value, mask))
                rules[kept++] = rules[i];
        memset(&rules[kept], 0, (*length - kept) * sizeof(rules[0]));
        *length = kept;
    }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}

bvstk_status_t bvstk_mdio_service_rule_clear_list(bvstk_mdio_service_t *s,
    size_t d, mdio_policy_t list)
{
    mdio_rule_entry_t *rules;
    size_t *length;
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    st = policy_list(s, d, list, &rules, &length);
    if (st == BVSTK_OK) {
        memset(rules, 0, *length * sizeof(rules[0]));
        *length = 0;
    }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}

bvstk_status_t bvstk_mdio_service_set_config(bvstk_mdio_service_t *s, size_t d, const mdio_phy_config_t *c)
{
    size_t i;
    bvstk_status_t st = bvstk_mdio_config_validate(c);
    if (st != BVSTK_OK) return st;
    st = lock(s);
    if (st != BVSTK_OK) return st;
    st = check(s, d, 0);
    /* Identity is stable: avoids dangling selectors and ambiguous persistence. */
    if (st == BVSTK_OK && (strcmp(c->name, s->devices[d].name) ||
        strcmp(c->file_name, s->devices[d].file_name))) st = BVSTK_ERR_DENIED;
    for (i = 0; i < s->device_count; ++i)
        if (i != d && c->phy_addr == s->devices[i].phy_addr) st = BVSTK_ERR_DENIED;
    if (st == BVSTK_OK) {
        int changed = c->phy_addr != s->devices[d].phy_addr ||
            c->reg_count != s->devices[d].reg_count ||
            c->settings_len != s->devices[d].settings_len ||
            memcmp(c->settings, s->devices[d].settings, sizeof(c->settings));
        if (changed) st = invalidate(s, d);
        if (st == BVSTK_OK) {
            s->devices[d] = *c;
            if (s->cache_config_changed) s->cache_config_changed(s->cache_sink.context, d, c);
            s->poll_due[d] = 0; s->poll_index[d] = 0;
            if (changed) { st = invalidate(s, d); if (st == BVSTK_OK) st = seed(s, d); }
        }
    }
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_stats(const bvstk_mdio_service_t *s, bvstk_mdio_stats_t *out)
{
    bvstk_status_t st = lock(s);
    if (st != BVSTK_OK) return st;
    if (!out) st = BVSTK_ERR_MALFORMED; else *out = s->stats;
    bvstk_mutex_unlock(&s->mutex);
    return st;
}
bvstk_status_t bvstk_mdio_service_step(bvstk_mdio_service_t *s, uint64_t now, uint32_t timeout)
{
    size_t n, taken = 0;
    bvstk_status_t result = BVSTK_OK, st = lock(s);
    if (st != BVSTK_OK) return st;
    /* Inspect the per-register mailboxes, not just the single last-address
     * register. Equal consecutive writes are observable after consumption.
     * Ordering across registers and exact losses cannot be recovered. */
    if (s->core || s->bus.take_write) for (n = 0; n < 1024U && taken < 4U; ++n) {
        uint8_t phy = (uint8_t)(s->scan_index >> 5), reg = (uint8_t)(s->scan_index & 31U);
        uint16_t v;
        size_t d;
        s->scan_index = (s->scan_index + 1U) & 1023U;
        st = s->bus.take_write ? s->bus.take_write(s->bus.context, phy, reg, &v) :
            bvstk_mdio_core_take_write(s->core, phy, reg, &v);
        if (st == BVSTK_ERR_NOT_FOUND) continue;
        if (st != BVSTK_OK) { ++s->stats.errors; result = st; break; }
        ++taken; ++s->stats.observed;
        d = find_phy(s, phy);
        emit_flags(s, BVSTK_EVENT_CAPTURE_WRITE, BVSTK_OK, BVSTK_EVENT_SOURCE_HOST,
            phy, reg, v, d < s->device_count && s->online[d] ?
                BVSTK_EVENT_FLAG_NONE : BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED);
        if (d == s->device_count) {
            ++s->stats.unknown_phy; ++s->stats.denied;
            emit(s, BVSTK_EVENT_REG_ATTEMPT, BVSTK_OK, BVSTK_EVENT_SOURCE_HOST, phy, reg, v);
            emit(s, BVSTK_EVENT_REG_DENIED, BVSTK_ERR_NOT_FOUND, BVSTK_EVENT_SOURCE_HOST, phy, reg, v);
        } else {
            st = write_phy(s, d, reg, v, BVSTK_EVENT_SOURCE_HOST, timeout);
            if (st != BVSTK_OK && st != BVSTK_ERR_DENIED) result = st;
        }
    }
    if (s->core && bvstk_mdio_core_ack_slave(s->core) != BVSTK_OK) {
        ++s->stats.errors; result = BVSTK_ERR_IO;
    }
    for (n = 0; n < s->device_count; ++n) {
        size_t d = s->poll_device++ % s->device_count;
        mdio_phy_config_t *c = &s->devices[d];
        uint16_t v;
        /* Probe even with autopoll disabled: detect a disconnected PHY and
         * retry it independently of the user-selected cache refresh list. */
        if (now >= s->probe_due[d] && c->reg_count >= 4U) {
            uint16_t id1 = 0, id2 = 0;
            st = read_phy(s, d, 2, &id1, BVSTK_EVENT_SOURCE_INTERNAL, timeout, 1);
            if (st == BVSTK_OK) st = read_phy(s, d, 3, &id2, BVSTK_EVENT_SOURCE_INTERNAL, timeout, 1);
            if (st == BVSTK_OK && id1 != 0xFFFFU && id2 != 0xFFFFU && (id1 || id2)) {
                cache_changed(s, d, 2); cache_changed(s, d, 3);
                if (!s->online[d]) s->poll_due[d] = 0;
                s->online[d] = 1;
                bvstk_cache_sink_presence(&s->cache_sink, d, 1U);
            } else {
                (void)invalidate(s, d);
                result = st == BVSTK_OK ? BVSTK_ERR_NOT_READY : st;
            }
            s->probe_due[d] = now + 1000U;
            break;
        }
        if (!c->autopoll_enabled || !c->autopoll_regs_len || now < s->poll_due[d]) continue;
        if (c->reg_count >= 4U && !s->online[d]) continue;
        st = read_phy(s, d, c->autopoll_regs[s->poll_index[d]], &v,
            BVSTK_EVENT_SOURCE_INTERNAL, timeout, 0);
        if (st != BVSTK_OK) {
            result = st;
            (void)invalidate(s, d);
            s->probe_due[d] = now + 1000U;
        }
        if (++s->poll_index[d] >= c->autopoll_regs_len) {
            s->poll_index[d] = 0;
            s->poll_due[d] = now + (c->autopoll_cycle_delay_ms ? c->autopoll_cycle_delay_ms : 1U);
        } else s->poll_due[d] = now + c->autopoll_reg_delay_ms;
        break;
    }
    bvstk_mutex_unlock(&s->mutex);
    return result;
}
bvstk_status_t bvstk_mdio_service_poll(bvstk_mdio_service_t *s, uint32_t timeout)
{
    return bvstk_mdio_service_step(s, bvstk_platform_now_ms(), timeout);
}
