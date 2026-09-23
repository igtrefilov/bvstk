#include "shared/config/bvstk_mdio_config_codec.h"
#include "shared/config/bvstk_device_profile.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int safe_name(const char *s, size_t cap, int empty)
{
    size_t i;
    if (!s[0] && !empty) return 0;
    for (i = 0; i < cap && s[i]; ++i)
        if ((unsigned char)s[i] > 127U ||
            (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-' && s[i] != '.')) return 0;
    return i < cap && !strstr(s, "..");
}
bvstk_status_t bvstk_mdio_config_validate(const mdio_phy_config_t *c)
{
    size_t i, j;
    if (!c || !safe_name(c->name, sizeof(c->name), 0) ||
        !safe_name(c->file_name, sizeof(c->file_name), 1) ||
        c->phy_addr > 31U || !c->reg_count || c->reg_count > 32U ||
        (c->policy != MDIO_POLICY_WHITELIST && c->policy != MDIO_POLICY_BLACKLIST) ||
        c->whitelist_len > MDIO_CFG_RULES_MAX || c->blacklist_len > MDIO_CFG_RULES_MAX ||
        c->settings_len > MDIO_CFG_SETTINGS_MAX || c->autopoll_regs_len > 32U ||
        c->autopoll_reg_delay_ms > 60000U || c->autopoll_cycle_delay_ms > 600000U)
        return BVSTK_ERR_MALFORMED;
    for (i = 0; i < c->whitelist_len; ++i)
        if (c->whitelist[i].reg >= c->reg_count) return BVSTK_ERR_RANGE;
    for (i = 0; i < c->blacklist_len; ++i)
        if (c->blacklist[i].reg >= c->reg_count) return BVSTK_ERR_RANGE;
    for (i = 0; i < c->autopoll_regs_len; ++i) {
        if (c->autopoll_regs[i] >= c->reg_count) return BVSTK_ERR_RANGE;
        for (j = 0; j < i; ++j)
            if (c->autopoll_regs[j] == c->autopoll_regs[i]) return BVSTK_ERR_MALFORMED;
    }
    for (i = 0; i < c->settings_len; ++i) {
        if (c->settings[i].reg >= c->reg_count) return BVSTK_ERR_RANGE;
        for (j = 0; j < i; ++j)
            if (c->settings[j].reg == c->settings[i].reg) return BVSTK_ERR_MALFORMED;
    }
    return bvstk_device_profile_metadata_validate(&c->profile, c->reg_count);
}
int bvstk_mdio_policy_allows(const mdio_phy_config_t *c, uint8_t reg, uint16_t value)
{
    const mdio_rule_entry_t *rules;
    size_t count, i;
    int matched = 0;
    if (!c || reg >= c->reg_count ||
        c->whitelist_len > MDIO_CFG_RULES_MAX || c->blacklist_len > MDIO_CFG_RULES_MAX)
        return 0;
    if (c->policy == MDIO_POLICY_WHITELIST) {
        rules = c->whitelist; count = c->whitelist_len;
    } else if (c->policy == MDIO_POLICY_BLACKLIST) {
        rules = c->blacklist; count = c->blacklist_len;
    } else return 0;
    for (i = 0; i < count; ++i)
        if (rules[i].reg == reg && (value & rules[i].mask) == (rules[i].val & rules[i].mask))
            matched = 1;
    return c->policy == MDIO_POLICY_WHITELIST ? matched : !matched;
}

static void ws(const char **p) { while (isspace((unsigned char)**p)) ++*p; }
static int ch(const char **p, char c)
{
    ws(p);
    if (**p != c) return 0;
    ++*p; return 1;
}
static int string(const char **p, char *out, size_t cap)
{
    size_t n = 0;
    if (!ch(p, '"')) return 0;
    while (**p && **p != '"') {
        unsigned char c = (unsigned char)*(*p)++;
        /* Names/keys deliberately have no escapes or control characters. */
        if (c < 32U || c == '\\' || n + 1U >= cap) return 0;
        out[n++] = (char)c;
    }
    out[n] = '\0';
    return ch(p, '"');
}
static int number(const char **p, uint32_t max, uint32_t *out)
{
    uint32_t v = 0;
    ws(p);
    if (!isdigit((unsigned char)**p)) return 0;
    if (**p == '0' && isdigit((unsigned char)(*p)[1])) return 0;
    while (isdigit((unsigned char)**p)) {
        uint32_t d = (uint32_t)(*(*p)++ - '0');
        if (d > max || v > (max - d) / 10U) return 0;
        v = v * 10U + d;
    }
    *out = v; return 1;
}
/* Return 0=end, 1=next item, -1=invalid (including a trailing comma). */
static int next(const char **p, char close)
{
    if (ch(p, close)) return 0;
    if (!ch(p, ',')) return -1;
    ws(p);
    return **p == close ? -1 : 1;
}
static int rules(const char **p, mdio_rule_entry_t *out, size_t *len, int legacy)
{
    int more;
    *len = 0;
    if (!ch(p, '[')) return 0;
    if (ch(p, ']')) return 1;
    do {
        mdio_rule_entry_t r = {0, 0, 0xFFFFU};
        uint32_t v;
        unsigned seen = 0;
        if (*len >= MDIO_CFG_RULES_MAX) return 0;
        if (legacy) {
            if (!number(p, 31U, &v)) return 0;
            r.reg = (uint8_t)v; r.mask = 0;
        } else {
            if (!ch(p, '{')) return 0;
            do {
                char key[12];
                unsigned bit;
                if (!string(p, key, sizeof(key)) || !ch(p, ':')) return 0;
                bit = !strcmp(key, "reg") ? 1U : !strcmp(key, "val") ? 2U : !strcmp(key, "mask") ? 4U : 0U;
                if (!bit || (seen & bit) || !number(p, bit == 1U ? 31U : 65535U, &v)) return 0;
                seen |= bit;
                if (bit == 1U) r.reg = (uint8_t)v;
                else if (bit == 2U) r.val = (uint16_t)v;
                else r.mask = (uint16_t)v;
                more = next(p, '}');
            } while (more == 1);
            if (more < 0 || (seen & 3U) != 3U) return 0;
        }
        out[(*len)++] = r;
        more = next(p, ']');
    } while (more == 1);
    return more == 0;
}
bvstk_status_t bvstk_mdio_config_parse(const char *json, mdio_phy_config_t *out)
{
    static const char *const keys[] = {"name", "phy_addr", "reg_count", "policy",
        "autopoll_enabled", "autopoll_reg_delay_ms", "autopoll_cycle_delay_ms",
        "autopoll_regs", "whitelist", "blacklist", "settings",
        "write_allow_regs", "write_deny_regs"};
    mdio_phy_config_t c;
    uint32_t seen = 0;
    int more;
    const char *p = json;
    unsigned version;
    bvstk_status_t status;
    if (!json || !out) return BVSTK_ERR_MALFORMED;
    status = bvstk_device_profile_version(json, &version);
    if (status != BVSTK_OK) return status;
    if (version == 2U) return bvstk_device_profile_parse_mdio(json, out);
    memset(&c, 0, sizeof(c));
    c.autopoll_cycle_delay_ms = 50U;
    if (!ch(&p, '{')) return BVSTK_ERR_MALFORMED;
    do {
        char key[40];
        uint32_t v = 0;
        size_t k;
        if (!string(&p, key, sizeof(key)) || !ch(&p, ':')) goto bad;
        for (k = 0; k < sizeof(keys) / sizeof(keys[0]); ++k)
            if (!strcmp(key, keys[k])) break;
        if (k == sizeof(keys) / sizeof(keys[0]) || (seen & (1U << k))) goto bad;
        seen |= 1U << k;
        if (k == 0) {
            if (!string(&p, c.name, sizeof(c.name))) goto bad;
        } else if (k == 3) {
            if (!string(&p, key, sizeof(key))) goto bad;
            if (!strcmp(key, "whitelist")) c.policy = MDIO_POLICY_WHITELIST;
            else if (!strcmp(key, "blacklist")) c.policy = MDIO_POLICY_BLACKLIST;
            else goto bad;
        } else if (k == 4) {
            ws(&p);
            if (!strncmp(p, "true", 4)) { c.autopoll_enabled = true; p += 4; }
            else if (!strncmp(p, "false", 5)) p += 5;
            else goto bad;
        } else if (k == 7) {
            if (!ch(&p, '[')) goto bad;
            if (!ch(&p, ']')) do {
                if (c.autopoll_regs_len >= 32U || !number(&p, 31U, &v)) goto bad;
                c.autopoll_regs[c.autopoll_regs_len++] = (uint8_t)v;
                more = next(&p, ']');
                if (more < 0) goto bad;
            } while (more);
        } else if (k == 8 || k == 11) {
            if ((seen & (1U << 8)) && (seen & (1U << 11))) goto bad;
            if (!rules(&p, c.whitelist, &c.whitelist_len, k == 11)) goto bad;
        } else if (k == 9 || k == 12) {
            if ((seen & (1U << 9)) && (seen & (1U << 12))) goto bad;
            if (!rules(&p, c.blacklist, &c.blacklist_len, k == 12)) goto bad;
        } else if (k == 10) {
            /* Parse one setting at a time, without a second large rule array. */
            if (!ch(&p, '[')) goto bad;
            if (!ch(&p, ']')) do {
                unsigned fields = 0;
                mdio_setting_entry_t e = {0, 0};
                if (c.settings_len >= MDIO_CFG_SETTINGS_MAX || !ch(&p, '{')) goto bad;
                do {
                    unsigned bit;
                    if (!string(&p, key, sizeof(key)) || !ch(&p, ':')) goto bad;
                    bit = !strcmp(key, "reg") ? 1U : !strcmp(key, "val") ? 2U : 0U;
                    if (!bit || (fields & bit) || !number(&p, bit == 1U ? 31U : 65535U, &v)) goto bad;
                    fields |= bit;
                    if (bit == 1U) e.reg = (uint8_t)v; else e.val = (uint16_t)v;
                    more = next(&p, '}');
                    if (more < 0) goto bad;
                } while (more);
                if (fields != 3U) goto bad;
                c.settings[c.settings_len++] = e;
                more = next(&p, ']');
                if (more < 0) goto bad;
            } while (more);
        } else {
            if (!number(&p, k == 1 ? 31U : k == 2 ? 32U : 600000U, &v)) goto bad;
            if (k == 1) c.phy_addr = (uint8_t)v;
            if (k == 2) c.reg_count = (uint8_t)v;
            if (k == 5) c.autopoll_reg_delay_ms = v;
            if (k == 6) c.autopoll_cycle_delay_ms = v;
        }
        more = next(&p, '}');
    } while (more == 1);
    ws(&p);
    if (more < 0 || *p || (seen & 15U) != 15U ||
        bvstk_mdio_config_validate(&c) != BVSTK_OK) goto bad;
    *out = c;
    return BVSTK_OK;
bad:
    return BVSTK_ERR_MALFORMED;
}
static int append(char *out, size_t cap, size_t *n, const char *fmt, ...)
{
    int r;
    va_list ap;
    if (*n >= cap) return 0;
    va_start(ap, fmt);
    r = vsnprintf(out + *n, cap - *n, fmt, ap);
    va_end(ap);
    if (r < 0 || (size_t)r >= cap - *n) return 0;
    *n += (size_t)r;
    return 1;
}
bvstk_status_t bvstk_mdio_config_format(const mdio_phy_config_t *c,
    char *out, size_t cap, size_t *length)
{
    size_t n = 0, i, list;
    if (length) *length = 0;
    if (!out || !cap || bvstk_mdio_config_validate(c) != BVSTK_OK) return BVSTK_ERR_MALFORMED;
#define ADD(...) do { if (!append(out, cap, &n, __VA_ARGS__)) return BVSTK_ERR_RANGE; } while (0)
    ADD("{\n  \"name\": \"%s\",\n  \"phy_addr\": %u,\n  \"reg_count\": %u,\n"
        "  \"policy\": \"%s\",\n  \"autopoll_enabled\": %s,\n"
        "  \"autopoll_reg_delay_ms\": %lu,\n  \"autopoll_cycle_delay_ms\": %lu,\n  \"autopoll_regs\": [",
        c->name, c->phy_addr, c->reg_count,
        c->policy == MDIO_POLICY_WHITELIST ? "whitelist" : "blacklist",
        c->autopoll_enabled ? "true" : "false",
        (unsigned long)c->autopoll_reg_delay_ms, (unsigned long)c->autopoll_cycle_delay_ms);
    for (i = 0; i < c->autopoll_regs_len; ++i) ADD("%s%u", i ? ", " : "", c->autopoll_regs[i]);
    ADD("]");
    for (list = 0; list < 2; ++list) {
        const mdio_rule_entry_t *r = list ? c->blacklist : c->whitelist;
        size_t count = list ? c->blacklist_len : c->whitelist_len;
        ADD(",\n  \"%s\": [", list ? "blacklist" : "whitelist");
        for (i = 0; i < count; ++i)
            ADD("%s{\"reg\": %u, \"val\": %u, \"mask\": %u}", i ? ", " : "", r[i].reg, r[i].val, r[i].mask);
        ADD("]");
    }
    ADD(",\n  \"settings\": [");
    for (i = 0; i < c->settings_len; ++i)
        ADD("%s{\"reg\": %u, \"val\": %u}", i ? ", " : "", c->settings[i].reg, c->settings[i].val);
    ADD("]\n}\n");
#undef ADD
    if (length) *length = n;
    return BVSTK_OK;
}
