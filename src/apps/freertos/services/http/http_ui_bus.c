#include "apps/freertos/services/http/http_ui.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include "shared/interfaces/bvstk_clock.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

typedef union { i2c_device_config_t i2c; mdio_phy_config_t mdio; } bus_config_t;
typedef struct {
    bus_config_t current, next;
    char json[HTTP_UI_JSON_CAP], scratch[HTTP_UI_JSON_CAP];
} bus_workspace_t;

static bvstk_status_t snapshot(int mdio, size_t id, bus_config_t *out)
{
    if (mdio) return bvstk_mdio_service_get_config(bvstk_runtime_mdio_service(), id, &out->mdio);
    bvstk_status_t st;
    taskENTER_CRITICAL();
    st = bvstk_i2c_master_service_get_config(bvstk_runtime_i2c_master_service(), id, &out->i2c);
    if (st == BVSTK_OK) {
        const i2c_device_config_t *stored = config_store_find_i2c_device_by_name(out->i2c.name);
        if (!stored) st = BVSTK_ERR_NOT_READY;
        else {
            memcpy(out->i2c.file_name, stored->file_name, sizeof(out->i2c.file_name));
            out->i2c.settings_len = stored->settings_len;
            memcpy(out->i2c.settings, stored->settings, sizeof(out->i2c.settings));
            out->i2c.profile = stored->profile;
        }
    }
    taskEXIT_CRITICAL();
    return st;
}
static bvstk_status_t format(int mdio, const bus_config_t *cfg, char *out)
{
    size_t len;
    return mdio ? bvstk_mdio_config_format(&cfg->mdio, out, HTTP_UI_JSON_CAP, &len) :
        bvstk_i2c_config_serialize_json(&cfg->i2c, out, HTTP_UI_JSON_CAP, &len);
}
static bvstk_status_t parse(int mdio, const char *json, bus_config_t *out, int strict)
{
    static const char *const keys[] = {"name", "addr_7b", "reg_count", "max_value_code", "policy", "whitelist", "blacklist", "settings"};
    /* The web API remains a legacy projection. File import supports v2;
     * apply must not replace metadata that this editor cannot display. */
    if (strict) {
        unsigned version;
        if (bvstk_device_profile_version(json, &version) != BVSTK_OK || version)
            return BVSTK_ERR_MALFORMED;
    }
    if (mdio) return bvstk_mdio_config_parse(json, &out->mdio);
    if (strict && !bvstk_http_json_object(json, keys, 8, 255U)) return BVSTK_ERR_MALFORMED;
    return bvstk_i2c_config_parse_json(json, &out->i2c);
}
static const char *file_name(int mdio, const bus_config_t *c)
{
    return mdio ? c->mdio.file_name : c->i2c.file_name;
}
/* Compare the actual persisted config, not a browser-local 'saved' flag.
 * Shell saves and manual file changes are consequently visible after refresh. */
static int persisted(int mdio, bus_workspace_t *w)
{
    const fs_device_info_t *dev = fs_device_by_name("flash");
    char path[FS_PATH_MAX]; FIL f; FRESULT st; UINT n = 0; int result = -1;
    uint32_t revision = bvstk_http_revision(w->json);
    const char *fn = file_name(mdio, &w->current);
    if (!fn[0] || !dev || !fs_shared_is_ready(dev->ctx)) return -1;
    int size = snprintf(path, sizeof(path), "%sconfig/%s/%s", dev->ctx->root, mdio ? "mdio" : "i2c", fn);
    if (size < 0 || (size_t)size >= sizeof(path) || !http_ui_lock(dev->ctx)) return -1;
    st = f_open(&f, path, FA_READ);
    if (st == FR_NO_FILE || st == FR_NO_PATH) result = 0;
    if (st == FR_OK) {
        if (f_size(&f) < sizeof(w->scratch)) {
            st = f_read(&f, w->scratch, sizeof(w->scratch) - 1U, &n);
            if (st == FR_OK && n == f_size(&f)) {
                w->scratch[n] = 0;
                if (parse(mdio, w->scratch, &w->next, 0) == BVSTK_OK &&
                    format(mdio, &w->next, w->scratch) == BVSTK_OK)
                    result = revision == bvstk_http_revision(w->scratch) &&
                        (mdio || !memcmp(&w->next.i2c.endpoint, &w->current.i2c.endpoint,
                                         sizeof(w->current.i2c.endpoint)));
                else result = 0;
            }
        }
        if (f_close(&f) != FR_OK) result = -1;
    }
    http_ui_unlock(dev->ctx); return result;
}
static int online(size_t id)
{
    bvstk_mdio_cache_entry_t a, b;
    bvstk_mdio_service_t *s = bvstk_runtime_mdio_service();
    return bvstk_mdio_service_cache_entry(s, id, 2, &a) == BVSTK_OK &&
        bvstk_mdio_service_cache_entry(s, id, 3, &b) == BVSTK_OK &&
        a.valid && b.valid && !a.seeded && !b.seeded &&
        a.value != 0xffffU && b.value != 0xffffU && (a.value || b.value) &&
        bvstk_platform_now_ms() - a.updated_ms < 3000U &&
        bvstk_platform_now_ms() - b.updated_ms < 3000U;
}
static int device_presence(int mdio, size_t id)
{
    bvstk_i2c_presence_t presence;
    if (mdio) return online(id);
    if (bvstk_i2c_master_service_device_presence(bvstk_runtime_i2c_master_service(),
            id, &presence) != BVSTK_OK || presence == BVSTK_I2C_PRESENCE_UNKNOWN)
        return -1;
    return presence == BVSTK_I2C_PRESENCE_RESPONDING;
}
static void write_cache_array(int fd, int mdio, size_t id,
                              unsigned reg_count)
{
    for (unsigned reg = 0U; reg < reg_count; ++reg) {
        uint16_t value = 0U;
        uint8_t valid = 0U;
        uint8_t seeded = 0U;
        uint64_t updated_ms = 0U;
        uint32_t read_id = 0U, read_duration_ms = 0U;
        if (mdio) {
            bvstk_mdio_cache_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            (void)bvstk_mdio_service_cache_entry(
                bvstk_runtime_mdio_service(), id, (uint8_t)reg, &entry);
            value = entry.value;
            valid = entry.valid;
            seeded = entry.seeded;
            updated_ms = entry.updated_ms;
        } else {
            bvstk_i2c_cache_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            (void)bvstk_i2c_master_service_cache_entry(
                bvstk_runtime_i2c_master_service(), id, (uint8_t)reg, &entry);
            value = entry.value;
            valid = entry.valid;
            seeded = entry.seeded;
            updated_ms = entry.updated_ms;
            read_id = entry.read_id;
            read_duration_ms = entry.read_duration_ms;
        }
        http_ui_printf(fd,
            "%s{\"reg\":%u,\"val\":%u,\"valid\":%s,\"source\":\"%s\",\"updated_ms\":%llu",
            reg ? "," : "", reg, (unsigned)value, valid ? "true" : "false",
            seeded ? "settings" : (mdio ? "PHY" : "I2C"),
            (unsigned long long)(valid ? updated_ms : 0U));
        if (valid && !seeded && read_id)
            http_ui_printf(fd, ",\"read_id\":%lu,\"read_duration_ms\":%lu",
                (unsigned long)read_id, (unsigned long)read_duration_ms);
        http_ui_text(fd, "}");
    }
}
static const char *presence_name(int presence);
/* Profile-driven routing context for snapshot-only device views. No bus I/O
 * and no Flash reads: metadata comes from the already loaded profiles. */
static void write_routing(int fd, size_t id)
{
    bvstk_i2c_master_service_t *service = bvstk_runtime_i2c_master_service();
    bvstk_i2c_device_t parent, child;
    bvstk_i2c_switch_observation_t observation;
    if (bvstk_i2c_master_service_device_info(service, id, &parent) != BVSTK_OK ||
        !parent.endpoint.router.channel_count ||
        bvstk_i2c_master_service_switch_observation(service, id, &observation) != BVSTK_OK)
        return;
    const bvstk_i2c_switch_t *router = &parent.endpoint.router;
    http_ui_printf(fd, ",\"routing\":{\"control_reg\":%u,\"select_mask\":%u,\"channels\":[",
        router->reg, router->select_mask);
    for (unsigned i = 0; i < router->channel_count; ++i)
        http_ui_printf(fd, "%s{\"index\":%u,\"select\":%u,\"match_mask\":%u}",
            i ? "," : "", i, router->channels[i].select, router->channels[i].match_mask);
    http_ui_text(fd, "],\"last_read\":");
    if (observation.valid)
        http_ui_printf(fd, "{\"reg\":%u,\"val\":%u,\"updated_ms\":%llu}",
            router->reg, observation.value, (unsigned long long)observation.updated_ms);
    else http_ui_text(fd, "null");
    http_ui_text(fd, ",\"children\":[");
    int comma = 0;
    for (size_t i = 0; i < bvstk_i2c_master_service_device_count(service); ++i) {
        if (bvstk_i2c_master_service_device_info(service, i, &child) != BVSTK_OK ||
            strcmp(child.endpoint.parent, parent.name)) continue;
        char label[97] = {0}, model[65] = {0}, device_class[16] = {0};
        taskENTER_CRITICAL();
        const i2c_device_config_t *stored = config_store_find_i2c_device_by_name(child.name);
        if (stored) {
            memcpy(label, stored->profile.label, sizeof(label) - 1U);
            memcpy(model, stored->profile.model, sizeof(model) - 1U);
            memcpy(device_class, stored->profile.device_class, sizeof(device_class) - 1U);
        }
        taskEXIT_CRITICAL();
        http_ui_text(fd, comma++ ? ",{\"name\":" : "{\"name\":");
        http_ui_string(fd, child.name);
        http_ui_text(fd, ",\"label\":"); http_ui_string(fd, label);
        http_ui_text(fd, ",\"model\":"); http_ui_string(fd, model);
        http_ui_text(fd, ",\"class\":"); http_ui_string(fd, device_class);
        http_ui_printf(fd, ",\"channel\":%u,\"presence\":\"%s\",\"addresses\":[",
            child.endpoint.channel, presence_name(device_presence(0, i)));
        if (!child.endpoint.space_count) http_ui_printf(fd, "%u", child.addr_7b);
        for (unsigned j = 0; j < child.endpoint.space_count; ++j)
            http_ui_printf(fd, "%s%u", j ? "," : "", child.endpoint.spaces[j].address);
        http_ui_text(fd, "]}");
    }
    http_ui_text(fd, "]}");
}

static void cache_detail(int fd, int mdio, size_t id)
{
    unsigned reg_count = 0U;
    bvstk_status_t status;

    if (mdio) {
        bvstk_mdio_device_info_t device;
        status = bvstk_mdio_service_device_info(
            bvstk_runtime_mdio_service(), id, &device);
        if (status == BVSTK_OK) reg_count = device.reg_count;
    } else {
        bvstk_i2c_device_t device;
        status = bvstk_i2c_master_service_device_info(
            bvstk_runtime_i2c_master_service(), id, &device);
        if (status == BVSTK_OK) reg_count = device.reg_count;
    }
    if (status != BVSTK_OK) {
        http_ui_status(fd, status);
        return;
    }
    http_ui_header(fd, "application/json", -1);
    http_ui_printf(fd, "{\"uptime_ms\":%llu,\"presence\":\"%s\",\"cache\":[",
        (unsigned long long)bvstk_platform_now_ms(), presence_name(device_presence(mdio, id)));
    write_cache_array(fd, mdio, id, reg_count);
    http_ui_text(fd, "]");
    if (mdio) {
        bvstk_mdio_stats_t stats;
        if (bvstk_mdio_service_stats(bvstk_runtime_mdio_service(), &stats) == BVSTK_OK)
            http_ui_printf(fd,
                ",\"stats\":{\"reads\":%lu,\"writes\":%lu,\"denied\":%lu,\"errors\":%lu,\"observed\":%lu}",
                (unsigned long)stats.reads, (unsigned long)stats.writes,
                (unsigned long)stats.denied, (unsigned long)stats.errors,
                (unsigned long)stats.observed);
    } else write_routing(fd, id);
    http_ui_text(fd, "}\n");
}
static const char *presence_name(int presence)
{
    return presence < 0 ? "unknown" : presence ? "responding" : "unavailable";
}
static void topology(int fd, const bvstk_i2c_endpoint_t *endpoint)
{
    http_ui_text(fd, ",\"topology\":{\"controller\":\"pl_i2c_0\",\"parent\":");
    http_ui_string(fd, endpoint->parent);
    http_ui_printf(fd, ",\"channel\":%u,\"channels\":%u}", endpoint->channel,
        (unsigned)endpoint->router.channel_count);
    if (endpoint->space_count) {
        http_ui_text(fd, ",\"address_spaces\":[");
        for (unsigned i = 0; i < endpoint->space_count; ++i) {
            const bvstk_address_space_t *s = &endpoint->spaces[i];
            http_ui_text(fd, i ? ",{\"id\":" : "{\"id\":");
            http_ui_string(fd, s->id);
            http_ui_printf(fd, ",\"address\":%u,\"first\":%u,\"register_count\":%u}",
                s->address, s->first, s->count);
        }
        http_ui_text(fd, "]");
    }
}
static void list(int fd, int mdio)
{
    int ready = mdio ? bvstk_runtime_mdio_ready() : bvstk_runtime_i2c_ready();
    size_t count = mdio ? bvstk_mdio_service_device_count(bvstk_runtime_mdio_service()) :
        bvstk_i2c_master_service_device_count(bvstk_runtime_i2c_master_service());
    http_ui_header(fd, "application/json", -1);
    http_ui_printf(fd, "{\"ready\":%s,\"devices\":[", ready ? "true" : "false");
    int comma = 0;
    for (size_t i = 0; ready && i < count; ++i) {
        char name[I2C_CFG_NAME_MAX]; unsigned address; int white, available;
        bvstk_i2c_endpoint_t endpoint = {0};
        if (mdio) {
            bvstk_mdio_device_info_t d;
            if (bvstk_mdio_service_device_info(bvstk_runtime_mdio_service(), i, &d) != BVSTK_OK) continue;
            memcpy(name, d.name, sizeof(name)); address = d.phy_addr;
            white = d.policy == MDIO_POLICY_WHITELIST; available = device_presence(mdio, i);
        } else {
            bvstk_i2c_device_t d;
            bvstk_i2c_policy_entry_t pol;
            if (bvstk_i2c_master_service_device_info(bvstk_runtime_i2c_master_service(), i, &d) != BVSTK_OK ||
                bvstk_i2c_master_service_get_policy(bvstk_runtime_i2c_master_service(), i, &pol) != BVSTK_OK) continue;
            memcpy(name, d.name, sizeof(name)); address = d.addr_7b;
            endpoint = d.endpoint;
            white = pol.mode == I2C_POLICY_WHITELIST; available = device_presence(mdio, i);
        }
        http_ui_text(fd, comma++ ? ",{\"name\":" : "{\"name\":"); http_ui_string(fd, name);
        http_ui_printf(fd, ",\"address\":%u,\"available\":%s,\"presence\":\"%s\",\"policy\":\"%s\"", address,
            available > 0 ? "true" : "false", presence_name(available), white ? "whitelist" : "blacklist");
        if (!mdio) topology(fd, &endpoint);
        http_ui_text(fd, "}");
    }
    http_ui_text(fd, "]}\n");
}
static void detail(int fd, int mdio, size_t id, bus_workspace_t *w)
{
    int saved = persisted(mdio, w), presence = device_presence(mdio, id);
    http_ui_header(fd, "application/json", -1);
    http_ui_text(fd, "{\"config\":"); http_ui_text(fd, w->json);
    http_ui_printf(fd, ",\"revision\":\"%08lx\",\"saved\":%s,\"available\":%s,\"presence\":\"%s\",\"file_name\":",
        (unsigned long)bvstk_http_revision(w->json), saved < 0 ? "null" : saved ? "true" : "false",
        presence > 0 ? "true" : "false", presence_name(presence));
    http_ui_string(fd, file_name(mdio, &w->current));
    http_ui_text(fd, ",\"register_map\":");
    http_ui_string(fd, mdio ? w->current.mdio.profile.register_map : w->current.i2c.profile.register_map);
    http_ui_text(fd, ",\"view\":");
    http_ui_string(fd, mdio ? w->current.mdio.profile.view : w->current.i2c.profile.view);
    http_ui_text(fd, ",\"icon\":");
    http_ui_string(fd, mdio ? w->current.mdio.profile.icon : w->current.i2c.profile.icon);
    http_ui_printf(fd, ",\"uptime_ms\":%llu", (unsigned long long)bvstk_platform_now_ms());
    if (mdio) {
        bvstk_mdio_stats_t stats;
        http_ui_text(fd, ",\"cache\":[");
        write_cache_array(fd, mdio, id, w->current.mdio.reg_count);
        http_ui_text(fd, "]");
        if (bvstk_mdio_service_stats(bvstk_runtime_mdio_service(), &stats) == BVSTK_OK)
            http_ui_printf(fd, ",\"stats\":{\"reads\":%lu,\"writes\":%lu,\"denied\":%lu,\"errors\":%lu,\"observed\":%lu}",
                (unsigned long)stats.reads, (unsigned long)stats.writes, (unsigned long)stats.denied,
                (unsigned long)stats.errors, (unsigned long)stats.observed);
    } else {
        topology(fd, &w->current.i2c.endpoint);
        http_ui_text(fd, ",\"cache\":[");
        write_cache_array(fd, mdio, id, w->current.i2c.reg_count);
        http_ui_text(fd, "]");
        write_routing(fd, id);
    }
    http_ui_text(fd, "}\n");
}
static void operate(const http_request_t *r, http_conn_t *c, int mdio, size_t id, const char *action)
{
    char field[24], confirm[8]; uint32_t reg, val = 0; bvstk_status_t st;
    if (mdio && !strcmp(action, "invalidate")) {
        http_ui_status(c->fd, bvstk_mdio_service_invalidate(bvstk_runtime_mdio_service(), id)); return;
    }
    if (strcmp(action, "read") && strcmp(action, "write")) { http_ui_error(c->fd, 400, "unknown action"); return; }
    if (!http_ui_param(r, c, "reg", field, sizeof(field))) return;
    if (!bvstk_http_uint(field, mdio ? 31U : 255U, &reg)) { http_ui_error(c->fd, 400, "invalid register"); return; }
    char space[32];
    int space_query = bvstk_http_query(r->path, "space", space, sizeof(space));
    if (space_query) {
        bvstk_i2c_device_t device;
        if (mdio || space_query != 1 ||
            bvstk_i2c_master_service_device_info(bvstk_runtime_i2c_master_service(), id, &device) != BVSTK_OK) {
            http_ui_error(c->fd, 400, "invalid address space"); return;
        }
        unsigned i;
        for (i = 0; i < device.endpoint.space_count; ++i)
            if (!strcmp(space, device.endpoint.spaces[i].id)) break;
        if (i == device.endpoint.space_count || reg >= device.endpoint.spaces[i].count) {
            http_ui_error(c->fd, 400, "unknown address space or register"); return;
        }
        reg += device.endpoint.spaces[i].first;
    }
    if (!strcmp(action, "write")) {
        if (!http_ui_param(r, c, "val", field, sizeof(field))) return;
        if (!bvstk_http_uint(field, mdio ? 65535U : 255U, &val) ||
            bvstk_http_query(r->path, "confirm", confirm, sizeof(confirm)) != 1 || strcmp(confirm, "true")) {
            http_ui_error(c->fd, 400, "invalid value or missing confirmation"); return;
        }
        st = mdio ? bvstk_mdio_service_write(bvstk_runtime_mdio_service(), id, (uint8_t)reg,
                (uint16_t)val, BVSTK_EVENT_SOURCE_WEB, 100U) :
            bvstk_i2c_master_service_write(bvstk_runtime_i2c_master_service(), id, (uint8_t)reg,
                (uint8_t)val, BVSTK_EVENT_SOURCE_WEB, 100U);
        if (!mdio && st == BVSTK_OK) (void)bvstk_runtime_i2c_sync_device(id);
        http_ui_status(c->fd, st); return;
    }
    if (mdio) {
        uint16_t v = 0;
        st = bvstk_mdio_service_read_from(bvstk_runtime_mdio_service(), id, (uint8_t)reg, &v, BVSTK_EVENT_SOURCE_WEB, 100U); val = v;
    } else {
        uint8_t v = 0;
        st = bvstk_i2c_master_service_read_from(bvstk_runtime_i2c_master_service(), id, (uint8_t)reg, &v, BVSTK_EVENT_SOURCE_WEB, 100U); val = v;
    }
    if (st != BVSTK_OK) { http_ui_status(c->fd, st); return; }
    http_ui_header(c->fd, "application/json", -1);
    http_ui_printf(c->fd, "{\"ok\":true,\"reg\":%lu,\"val\":%lu}\n", (unsigned long)reg, (unsigned long)val);
}
int http_ui_bus(const http_request_t *r, http_conn_t *c)
{
    char bus[8], name[32], action[20], revision[16], current_revision[16];
    size_t id; bvstk_status_t st; bus_workspace_t *w; int mdio, q, action_q;
    if (strncmp(r->path, "/api/ui/bus", 11) || (r->path[11] && r->path[11] != '?')) return 0;
    if (!http_ui_param(r, c, "bus", bus, sizeof(bus))) return 1;
    mdio = !strcmp(bus, "mdio");
    if (!mdio && strcmp(bus, "i2c")) { http_ui_error(c->fd, 400, "unknown bus"); return 1; }
    q = bvstk_http_query(r->path, "name", name, sizeof(name));
    if (!q && !strcmp(r->method, "GET")) { list(c->fd, mdio); return 1; }
    if (q != 1 || !name[0]) { http_ui_error(c->fd, 400, "invalid device name"); return 1; }
    st = mdio ? bvstk_mdio_service_find_by_name(bvstk_runtime_mdio_service(), name, &id) :
        bvstk_i2c_master_service_find_by_name(bvstk_runtime_i2c_master_service(), name, &id);
    if (st != BVSTK_OK) { http_ui_status(c->fd, st); return 1; }
    if (!strcmp(r->method, "GET")) {
        action_q = bvstk_http_query(r->path, "action", action, sizeof(action));
        if (action_q == 1) {
            if (strcmp(action, "cache")) { http_ui_error(c->fd, 400, "unknown action"); return 1; }
            cache_detail(c->fd, mdio, id);
            return 1;
        }
    }
    if (!strcmp(r->method, "PUT")) {
        if (!http_ui_param(r, c, "action", action, sizeof(action))) return 1;
        if (strcmp(action, "apply") && strcmp(action, "save")) { operate(r, c, mdio, id, action); return 1; }
    } else action[0] = 0;
    w = pvPortMalloc(sizeof(*w));
    if (!w) { http_ui_error(c->fd, 503, "out of memory"); return 1; }
    st = snapshot(mdio, id, &w->current);
    if (st == BVSTK_OK) st = format(mdio, &w->current, w->json);
    if (st != BVSTK_OK) { http_ui_status(c->fd, st); goto done; }
    if (!action[0]) { detail(c->fd, mdio, id, w); goto done; }
    snprintf(current_revision, sizeof(current_revision), "%08lx", (unsigned long)bvstk_http_revision(w->json));
    if (!http_ui_param(r, c, "revision", revision, sizeof(revision))) goto done;
    if (strcmp(revision, current_revision)) { http_ui_error(c->fd, 409, "configuration changed; refresh before applying"); goto done; }
    if (!strcmp(action, "save")) {
        int ok = mdio ? config_store_save_mdio_device(&w->current.mdio) : config_store_save_i2c_device(&w->current.i2c);
        http_ui_status(c->fd, ok ? BVSTK_OK : BVSTK_ERR_IO); goto done;
    }
    if (!http_ui_body(r, c, w->scratch, sizeof(w->scratch))) goto done;
    st = parse(mdio, w->scratch, &w->next, 1);
    if (st != BVSTK_OK) { http_ui_status(c->fd, st); goto done; }
    if (strcmp(name, mdio ? w->next.mdio.name : w->next.i2c.name)) { http_ui_error(c->fd, 400, "device identity cannot change"); goto done; }
    /* Recheck after receiving/parsing the body: it may arrive slowly. */
    st = snapshot(mdio, id, &w->current);
    if (st == BVSTK_OK) st = format(mdio, &w->current, w->json);
    snprintf(current_revision, sizeof(current_revision), "%08lx", (unsigned long)bvstk_http_revision(w->json));
    if (st != BVSTK_OK || strcmp(revision, current_revision)) { http_ui_error(c->fd, 409, "configuration changed; refresh before applying"); goto done; }
    if (mdio) {
        if (w->next.mdio.reg_count != w->current.mdio.reg_count) {
            http_ui_error(c->fd, 400, "device limits are read-only"); goto done;
        }
        memcpy(w->next.mdio.file_name, w->current.mdio.file_name, sizeof(w->next.mdio.file_name));
        w->next.mdio.profile = w->current.mdio.profile;
        st = bvstk_mdio_service_set_config(bvstk_runtime_mdio_service(), id, &w->next.mdio);
    } else {
        /* Limits and boot settings are read-only here. Do not silently alter cache seeds. */
        if (w->next.i2c.reg_count != w->current.i2c.reg_count || w->next.i2c.max_value_code != w->current.i2c.max_value_code ||
            w->next.i2c.settings_len != w->current.i2c.settings_len ||
            memcmp(w->next.i2c.settings, w->current.i2c.settings, w->current.i2c.settings_len * sizeof(i2c_rule_entry_t))) {
            http_ui_error(c->fd, 400, "device limits and boot settings are read-only"); goto done;
        }
        size_t other;
        if (bvstk_i2c_master_service_find_by_addr(bvstk_runtime_i2c_master_service(), w->next.i2c.addr_7b, &other) == BVSTK_OK && other != id) {
            http_ui_error(c->fd, 409, "address is already assigned to another device"); goto done;
        }
        memcpy(w->next.i2c.file_name, w->current.i2c.file_name, sizeof(w->next.i2c.file_name));
        w->next.i2c.profile = w->current.i2c.profile;
        w->next.i2c.endpoint = w->current.i2c.endpoint;
        st = bvstk_runtime_i2c_apply_config(&w->next.i2c) && config_store_set_i2c_device(&w->next.i2c) ? BVSTK_OK : BVSTK_ERR_IO;
    }
    http_ui_status(c->fd, st);
done:
    vPortFree(w); return 1;
}
