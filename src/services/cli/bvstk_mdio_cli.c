#include "services/cli/bvstk_cli.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include <string.h>
#include <strings.h>

static int eq(const char *s, const char *v) { return s && !strcasecmp(s, v); }
static int number(const char *s, unsigned long max, unsigned long *v)
{
    return bvstk_cli_number(s, max, v) == BVSTK_OK;
}
static void result(bvstk_cli_t *cli, bvstk_status_t st)
{
    bvstk_cli_result(cli, "mdio", st);
}
void bvstk_mdio_cli_help(bvstk_cli_t *cli)
{
    bvstk_cli_text(cli,
        "mdio list | status | -h | --help\r\n"
        "mdio <name|@phy|phy> [info]\r\n"
        "mdio <sel> r <reg> | w <reg> <value>\r\n"
        "mdio <sel> cache [show [reg]|refresh|invalidate]\r\n"
        "mdio <sel> policy [show]\r\n"
        "mdio <sel> policy show <rules|whitelist|blacklist>\r\n"
        "mdio <sel> policy <whitelist|blacklist>  (show list)\r\n"
        "mdio <sel> policy set <whitelist|blacklist>\r\n"
        "mdio <sel> policy <whitelist|blacklist> add|del <reg> <value|*> [mask]\r\n"
        "mdio <sel> policy <whitelist|blacklist> clear\r\n"
        "mdio <sel> autopoll [show|on|off|reg_delay <ms>|cycle_delay <ms>|regs <r0> ...]\r\n"
        "mdio <sel> settings [show|set <reg> <value>|del <reg>|clear]\r\n"
        "mdio <sel> phy <0..31> | save\r\n"
        "Changes apply in RAM; save persists flash:/config/mdio/<device>.json.\r\n"
        "policy / policy show: mode only; policy show rules: mode and both lists.\r\n"
        "settings seeds read cache, never writes PHY. Autopoll replaces seeds.\r\n"
        "Policy mask defaults to 0xFFFF (exact); * matches any value of a register.\r\n"
        "PL may coalesce/drop writes; denied writes cannot be signalled on MDIO wire.\r\n"
        "A successful write means a frame was sent, not a PHY acknowledgement.\r\n");
}
static void rules_show(bvstk_cli_t *cli, const char *name, const mdio_rule_entry_t *r, size_t n)
{
    bvstk_cli_rules_header(cli, name, n);
    for (size_t i = 0; i < n; ++i)
        bvstk_cli_rule(cli, r[i].reg, r[i].val, &r[i].mask);
}
static void info(bvstk_cli_t *cli, const mdio_phy_config_t *c)
{
    bvstk_cli_writef(cli, "name=%s phy=%u regs=%u policy=%s\r\n", c->name, c->phy_addr, c->reg_count,
        c->policy == MDIO_POLICY_WHITELIST ? "whitelist" : "blacklist");
    bvstk_cli_writef(cli, "file=%s whitelist=%u blacklist=%u settings=%u\r\n",
        c->file_name, (unsigned)c->whitelist_len, (unsigned)c->blacklist_len, (unsigned)c->settings_len);
    bvstk_cli_writef(cli, "autopoll=%s reg_delay=%lu ms cycle_delay=%lu ms\r\n",
        c->autopoll_enabled ? "on" : "off",
        (unsigned long)c->autopoll_reg_delay_ms, (unsigned long)c->autopoll_cycle_delay_ms);
    bvstk_cli_text(cli, "autopoll_regs:");
    for (size_t i = 0; i < c->autopoll_regs_len; ++i) bvstk_cli_writef(cli, " %u", c->autopoll_regs[i]);
    bvstk_cli_text(cli, "\r\n");
}
static void status_show(bvstk_cli_t *cli, bvstk_mdio_service_t *s)
{
    bvstk_mdio_stats_t st;
    if (bvstk_mdio_service_stats(s, &st) != BVSTK_OK) { result(cli, BVSTK_ERR_NOT_READY); return; }
    bvstk_cli_writef(cli, "mdio: master=%08lX slave=%08lX bram=%08lX worker=running devices=%u\r\n",
        (unsigned long)BVSTK_MDIO_MASTER_BASE, (unsigned long)BVSTK_MDIO_SLAVE_BASE,
        (unsigned long)BVSTK_MDIO_BRAM_BASE, (unsigned)bvstk_mdio_service_device_count(s));
    bvstk_cli_writef(cli, "reads=%lu writes=%lu denied=%lu errors=%lu observed=%lu unknown_phy=%lu\r\n",
        (unsigned long)st.reads, (unsigned long)st.writes, (unsigned long)st.denied,
        (unsigned long)st.errors, (unsigned long)st.observed, (unsigned long)st.unknown_phy);
    bvstk_cli_writef(cli, "last: phy=%u reg=%u value=%04X source=%u result=%s\r\n",
        st.last_phy, st.last_reg, st.last_value, st.last_source, bvstk_status_string(st.last_status));
    bvstk_cli_text(cli, "observed counts consumed mailboxes, not all wire frames; exact loss count unavailable.\r\n");
}
static bvstk_status_t select_device(bvstk_mdio_service_t *s, const char *arg, size_t *d)
{
    unsigned long phy;
    if (arg[0] != '@' && bvstk_mdio_service_find_by_name(s, arg, d) == BVSTK_OK) return BVSTK_OK;
    if (!number(arg[0] == '@' ? arg + 1 : arg, 31U, &phy)) return BVSTK_ERR_NOT_FOUND;
    return bvstk_mdio_service_find_by_phy(s, (uint8_t)phy, d);
}
static void cache_show(bvstk_cli_t *cli, bvstk_mdio_service_t *s, size_t d, unsigned first, unsigned end)
{
    for (unsigned r = first; r < end; ++r) {
        bvstk_mdio_cache_entry_t e;
        bvstk_status_t st = bvstk_mdio_service_cache_entry(s, d, (uint8_t)r, &e);
        if (st != BVSTK_OK) { result(cli, st); return; }
        if (!e.valid) bvstk_cli_writef(cli, "reg=%02u invalid (PL response=FFFF)\r\n", r);
        else bvstk_cli_writef(cli, "reg=%02u value=0x%04X age=%lu ms source=%s\r\n",
            r, e.value, (unsigned long)(bvstk_platform_now_ms() - e.updated_ms), e.seeded ? "settings" : "PHY");
    }
}
static bvstk_status_t edit_policy(mdio_phy_config_t *c, int n, char **a)
{
    mdio_rule_entry_t *r;
    size_t *len, i;
    unsigned long reg, val = 0, mask = 65535U;
    bvstk_status_t st;
    if (n == 2 && eq(a[0], "set")) {
        if (eq(a[1], "whitelist")) c->policy = MDIO_POLICY_WHITELIST;
        else if (eq(a[1], "blacklist")) c->policy = MDIO_POLICY_BLACKLIST;
        else return BVSTK_ERR_MALFORMED;
        return BVSTK_OK;
    }
    if (n < 2) return BVSTK_ERR_MALFORMED;
    if (eq(a[0], "whitelist")) { r = c->whitelist; len = &c->whitelist_len; }
    else if (eq(a[0], "blacklist")) { r = c->blacklist; len = &c->blacklist_len; }
    else return BVSTK_ERR_MALFORMED;
    if (n == 2 && eq(a[1], "clear")) { *len = 0; return BVSTK_OK; }
    if (n != 4 && n != 5) return BVSTK_ERR_MALFORMED;
    if (!eq(a[1], "add") && !eq(a[1], "del") && !eq(a[1], "delete")) return BVSTK_ERR_MALFORMED;
    st = bvstk_cli_number(a[2], c->reg_count - 1U, &reg);
    if (st != BVSTK_OK) return st;
    if (!strcmp(a[3], "*")) { if (n != 4) return BVSTK_ERR_MALFORMED; mask = 0; }
    else {
        st = bvstk_cli_number(a[3], 65535U, &val);
        if (st == BVSTK_OK && n == 5) st = bvstk_cli_number(a[4], 65535U, &mask);
        if (st != BVSTK_OK) return st;
    }
    val &= mask;
    for (i = 0; i < *len; ++i)
        if (r[i].reg == reg && r[i].mask == mask && (r[i].val & mask) == val) break;
    if (eq(a[1], "add")) {
        if (i < *len) return BVSTK_OK;
        if (*len == MDIO_CFG_RULES_MAX) return BVSTK_ERR_RANGE;
        r[i].reg = (uint8_t)reg; r[i].val = (uint16_t)val; r[i].mask = (uint16_t)mask; ++*len;
        return BVSTK_OK;
    }
    if (eq(a[1], "del") || eq(a[1], "delete")) {
        if (i == *len) return BVSTK_ERR_NOT_FOUND;
        memmove(r + i, r + i + 1U, (*len - i - 1U) * sizeof(*r)); --*len;
        return BVSTK_OK;
    }
    return BVSTK_ERR_MALFORMED;
}
bool bvstk_mdio_cli(bvstk_cli_t *cli, int n, char **a)
{
    size_t d = 0;
    unsigned long reg, val;
    mdio_phy_config_t c;
    bvstk_mdio_service_t *s;
    bvstk_status_t st;
    if (n && eq(a[0], "stream")) return bvstk_stream_cli(cli, DCP2_STREAM_SERVICE_MDIO, n - 1, a + 1);
    if (!n || (n == 1 && (eq(a[0], "-h") || eq(a[0], "--help")))) { bvstk_mdio_cli_help(cli); return true; }
    s = cli->mdio;
    if (!s) { result(cli, BVSTK_ERR_NOT_READY); return true; }
    if (n == 1 && eq(a[0], "status")) { status_show(cli, s); return true; }
    if (n == 1 && eq(a[0], "list")) {
        bvstk_cli_writef(cli, "mdio: devices=%u\r\n", (unsigned)bvstk_mdio_service_device_count(s));
        for (d = 0; d < bvstk_mdio_service_device_count(s); ++d) {
            st = bvstk_mdio_service_get_config(s, d, &c);
            if (st != BVSTK_OK) { result(cli, st); break; }
            bvstk_cli_writef(cli, "  name=%s phy=%u regs=%u\r\n", c.name, c.phy_addr, c.reg_count);
        }
        return true;
    }
    /* Retain useful r <phy> <reg> syntax, never the old subsystem name. */
    if (n >= 2 && (eq(a[0], "r") || eq(a[0], "w"))) { char *t = a[0]; a[0] = a[1]; a[1] = t; }
    st = select_device(s, a[0], &d);
    if (st == BVSTK_OK) st = bvstk_mdio_service_get_config(s, d, &c);
    if (st != BVSTK_OK) { result(cli, st); return true; }
    if (n == 1 || (n == 2 && eq(a[1], "info"))) { info(cli, &c); return true; }
    if ((eq(a[1], "r") || eq(a[1], "read")) && n == 3) {
        uint16_t v;
        st = bvstk_cli_number(a[2], c.reg_count - 1U, &reg);
        if (st != BVSTK_OK) { result(cli, st); return true; }
        st = bvstk_mdio_service_read_from(s, d, (uint8_t)reg, &v, BVSTK_EVENT_SOURCE_CONSOLE, 100U);
        if (st == BVSTK_OK) bvstk_cli_read_result(cli, (uint8_t)reg, v, 4);
        else result(cli, st);
        return true;
    }
    if ((eq(a[1], "w") || eq(a[1], "write")) && n == 4) {
        st = bvstk_cli_number(a[2], c.reg_count - 1U, &reg);
        if (st == BVSTK_OK) st = bvstk_cli_number(a[3], 65535U, &val);
        if (st != BVSTK_OK) { result(cli, st); return true; }
        result(cli, bvstk_mdio_service_write(s, d, (uint8_t)reg, (uint16_t)val, BVSTK_EVENT_SOURCE_CONSOLE, 100U));
        return true;
    }
    if (eq(a[1], "cache")) {
        if (n == 2 || (n == 3 && eq(a[2], "show"))) cache_show(cli, s, d, 0, c.reg_count);
        else if (n == 4 && eq(a[2], "show") && number(a[3], c.reg_count - 1U, &reg))
            cache_show(cli, s, d, (unsigned)reg, (unsigned)reg + 1U);
        else if (n == 3 && eq(a[2], "invalidate")) result(cli, bvstk_mdio_service_invalidate(s, d));
        else if (n == 3 && eq(a[2], "refresh")) {
            st = c.autopoll_regs_len ? BVSTK_OK : BVSTK_ERR_NOT_READY;
            for (size_t i = 0; i < c.autopoll_regs_len && st == BVSTK_OK; ++i) {
                uint16_t v;
                st = bvstk_mdio_service_read_from(s, d, c.autopoll_regs[i], &v, BVSTK_EVENT_SOURCE_CONSOLE, 100U);
            }
            result(cli, st);
        } else result(cli, BVSTK_ERR_MALFORMED);
        return true;
    }
    if (eq(a[1], "save") && n == 2) {
        st = cli->save ? cli->save(cli->context, true, d) : BVSTK_ERR_UNSUPPORTED;
        if (st != BVSTK_OK) result(cli, st);
        else bvstk_cli_saved(cli, "mdio", c.file_name);
        return true;
    }
    st = BVSTK_ERR_MALFORMED;
    if (eq(a[1], "policy")) {
        bvstk_cli_policy_view_t view;
        if (bvstk_cli_policy_view(n - 2, a + 2, &view)) {
            if (view == BUS_SHELL_POLICY_MODE || view == BUS_SHELL_POLICY_RULES)
                bvstk_cli_policy_mode(cli, c.policy == MDIO_POLICY_BLACKLIST);
            if (view == BUS_SHELL_POLICY_RULES || view == BUS_SHELL_POLICY_WHITELIST)
                rules_show(cli, "whitelist", c.whitelist, c.whitelist_len);
            if (view == BUS_SHELL_POLICY_RULES || view == BUS_SHELL_POLICY_BLACKLIST)
                rules_show(cli, "blacklist", c.blacklist, c.blacklist_len);
            return true;
        }
        st = edit_policy(&c, n - 2, a + 2);
    } else if (eq(a[1], "phy") && n == 3 && number(a[2], 31U, &val)) {
        c.phy_addr = (uint8_t)val; st = BVSTK_OK;
    } else if (eq(a[1], "autopoll")) {
        if (n == 2 || (n == 3 && eq(a[2], "show"))) { info(cli, &c); return true; }
        if (n == 3 && (eq(a[2], "on") || eq(a[2], "off"))) {
            c.autopoll_enabled = eq(a[2], "on"); st = BVSTK_OK;
        } else if (n == 4 && eq(a[2], "reg_delay") && number(a[3], 60000U, &val)) {
            c.autopoll_reg_delay_ms = (uint32_t)val; st = BVSTK_OK;
        } else if (n == 4 && eq(a[2], "cycle_delay") && number(a[3], 600000U, &val)) {
            c.autopoll_cycle_delay_ms = (uint32_t)val; st = BVSTK_OK;
        } else if (n >= 3 && eq(a[2], "regs")) {
            c.autopoll_regs_len = 0; st = BVSTK_OK;
            for (int i = 3; i < n; ++i) {
                if (c.autopoll_regs_len == 32U || !number(a[i], c.reg_count - 1U, &reg)) { st = BVSTK_ERR_RANGE; break; }
                c.autopoll_regs[c.autopoll_regs_len++] = (uint8_t)reg;
            }
        }
    } else if (eq(a[1], "settings")) {
        if (n == 2 || (n == 3 && eq(a[2], "show"))) {
            bvstk_cli_text(cli, "settings: cache seeds only (no PHY writes)\r\n");
            for (size_t i = 0; i < c.settings_len; ++i) bvstk_cli_writef(cli, "reg=%u value=0x%04X\r\n", c.settings[i].reg, c.settings[i].val);
            return true;
        }
        if (n == 3 && eq(a[2], "clear")) { memset(c.settings, 0, sizeof(c.settings)); c.settings_len = 0; st = BVSTK_OK; }
        else if ((n == 4 || n == 5) && number(a[3], c.reg_count - 1U, &reg)) {
            size_t i;
            for (i = 0; i < c.settings_len && c.settings[i].reg != reg; ++i) {}
            if (n == 4 && eq(a[2], "del")) {
                if (i == c.settings_len) st = BVSTK_ERR_NOT_FOUND;
                else {
                    memmove(c.settings + i, c.settings + i + 1U, (c.settings_len - i - 1U) * sizeof(c.settings[0]));
                    memset(&c.settings[--c.settings_len], 0, sizeof(c.settings[0])); st = BVSTK_OK;
                }
            } else if (n == 5 && eq(a[2], "set") && number(a[4], 65535U, &val) && i < MDIO_CFG_SETTINGS_MAX) {
                c.settings[i].reg = (uint8_t)reg; c.settings[i].val = (uint16_t)val;
                if (i == c.settings_len) ++c.settings_len;
                st = BVSTK_OK;
            }
        }
    }
    if (st == BVSTK_OK) st = bvstk_mdio_service_set_config(s, d, &c);
    if (st == BVSTK_OK && cli->mdio_changed) st = cli->mdio_changed(cli->context, d);
    if (st == BVSTK_OK) bvstk_cli_changed(cli);
    else result(cli, st);
    return true;
}
