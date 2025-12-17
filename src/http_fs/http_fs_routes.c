#include "../http/http_server.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ff.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"

#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif

#include "FreeRTOS.h"
#include "task.h"

#include "xstatus.h"

#include "../config/config_store.h"
#include "../bvstk_i2c/bvstk_i2c.h"
#include "../bvstk_smi/bvstk_smi.h"
#include "../fs/fs_devices.h"
#include "../fs/fs_shared.h"
#include "../qspi_fs/qspi_fs.h"
#include "../qspi_fs/qspi_fs_layout.h"
#include "../tar/tar.h"

enum { WEB_INDEX_NAME_MAX = 16 };
static const char *const WEB_ROOT_DIR = "www";

extern struct netif *netif;
extern unsigned char mac_ethernet_address[];
extern size_t xPortGetFreeHeapSize(void);
extern size_t xPortGetMinimumEverFreeHeapSize(void);

static int ctx_lock(const fs_shared_ctx_t *ctx)
{
    if (!ctx || !ctx->mutex || !*(ctx->mutex)) return 1;
    return xSemaphoreTake(*(ctx->mutex), pdMS_TO_TICKS(2000)) == pdTRUE;
}

static void ctx_unlock(const fs_shared_ctx_t *ctx)
{
    if (ctx && ctx->mutex && *(ctx->mutex)) xSemaphoreGive(*(ctx->mutex));
}

static void http_write_str(int fd, const char *s)
{
    (void)lwip_write(fd, s, (int)strlen(s));
}

static void http_reply_simple(int fd, int code, const char *reason, const char *body)
{
    char hdr[256];
    const char *b = body ? body : "";
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %d %s\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        code, reason ? reason : "ERR", (unsigned)strlen(b));
    if (n > 0) lwip_write(fd, hdr, n);
    if (b[0]) http_write_str(fd, b);
}

static void http_reply_json_hdr(int fd, int code, const char *reason)
{
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %d %s\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "\r\n",
        code, reason ? reason : "OK");
    if (n > 0) lwip_write(fd, hdr, n);
}

static void json_write_escaped(int fd, const char *s)
{
    http_write_str(fd, "\"");
    if (!s) { http_write_str(fd, "\""); return; }
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        char c = (char)*p;
        if (c == '\\' || c == '"') {
            char out[3] = {'\\', c, 0};
            http_write_str(fd, out);
        } else if ((unsigned char)c < 0x20) {
            char esc[8];
            int n = snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)(unsigned char)c);
            if (n > 0) http_write_str(fd, esc);
        } else {
            (void)lwip_write(fd, &c, 1);
        }
    }
    http_write_str(fd, "\"");
}

static int is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

static bool url_decode_inplace(char *s)
{
    if (!s) return false;
    char *w = s;
    for (char *p = s; *p; ++p) {
        if (*p == '%' && is_hex_digit((char)p[1]) && is_hex_digit((char)p[2])) {
            int v = (hex_val((char)p[1]) << 4) | hex_val((char)p[2]);
            *w++ = (char)v;
            p += 2;
            continue;
        }
        if (*p == '+') { *w++ = ' '; continue; }
        *w++ = *p;
    }
    *w = '\0';
    return true;
}

static bool starts_with(const char *s, const char *pfx)
{
    return s && pfx && strncmp(s, pfx, strlen(pfx)) == 0;
}

static const char *mime_from_path(const char *path)
{
    if (!path) return "application/octet-stream";
    const char *ext = strrchr(path, '.');
    if (!ext || ext[1] == '\0') return "application/octet-stream";
    ext++;
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, "css") == 0) return "text/css; charset=utf-8";
    if (strcasecmp(ext, "js") == 0) return "application/javascript; charset=utf-8";
    if (strcasecmp(ext, "json") == 0) return "application/json; charset=utf-8";
    if (strcasecmp(ext, "png") == 0) return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcasecmp(ext, "ico") == 0) return "image/x-icon";
    if (strcasecmp(ext, "txt") == 0) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static void strip_query_fragment_inplace(char *s)
{
    if (!s) return;
    for (char *p = s; *p; ++p) {
        if (*p == '?' || *p == '#') { *p = '\0'; return; }
    }
}

static void copy_path_no_query(const char *in, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!in) return;
    strncpy(out, in, out_sz - 1);
    out[out_sz - 1] = '\0';
    strip_query_fragment_inplace(out);
}

static bool query_get_param(const char *path, const char *key, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    if (!path || !key || !key[0]) return false;
    const char *q = strchr(path, '?');
    if (!q) return false;
    q++;
    size_t key_len = strlen(key);
    const char *p = q;
    while (p && *p) {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : (p + strlen(p));
        const char *eq = memchr(p, '=', (size_t)(end - p));
        if (eq && (size_t)(eq - p) == key_len && strncmp(p, key, key_len) == 0) {
            size_t vlen = (size_t)(end - (eq + 1));
            if (vlen + 1 > out_sz) vlen = out_sz - 1;
            memcpy(out, eq + 1, vlen);
            out[vlen] = '\0';
            url_decode_inplace(out);
            return true;
        }
        if (!amp) break;
        p = amp + 1;
    }
    return false;
}

static void ip4_to_str(const ip4_addr_t *a, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!a) return;
    (void)ip4addr_ntoa_r(a, out, (int)out_sz);
}

static void mac_to_str(const uint8_t *mac, size_t mac_len, char *out, size_t out_sz)
{
    if (!out || out_sz < 18) { if (out && out_sz) out[0] = '\0'; return; }
    if (!mac || mac_len < 6) { strncpy(out, "00:00:00:00:00:00", out_sz - 1); out[out_sz - 1] = '\0'; return; }
    (void)snprintf(out, out_sz, "%02x:%02x:%02x:%02x:%02x:%02x",
                   (unsigned)mac[0], (unsigned)mac[1], (unsigned)mac[2],
                   (unsigned)mac[3], (unsigned)mac[4], (unsigned)mac[5]);
}

static void json_write_kv_str(int fd, const char *k, const char *v, bool comma)
{
    json_write_escaped(fd, k);
    http_write_str(fd, ":");
    json_write_escaped(fd, v ? v : "");
    if (comma) http_write_str(fd, ",");
}

static void json_write_kv_u64(int fd, const char *k, uint64_t v, bool comma)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "\"%s\":%llu%s", k, (unsigned long long)v, comma ? "," : "");
    if (n > 0) http_write_str(fd, buf);
}

static void json_write_kv_bool(int fd, const char *k, bool v, bool comma)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "\"%s\":%s%s", k, v ? "true" : "false", comma ? "," : "");
    if (n > 0) http_write_str(fd, buf);
}

static void api_handle_version(int fd)
{
    http_reply_json_hdr(fd, 200, "OK");
    http_write_str(fd, "{");
    json_write_kv_str(fd, "build_date", __DATE__, true);
    json_write_kv_str(fd, "build_time", __TIME__, true);
    json_write_kv_u64(fd, "http_port", http_server_port(), false);
    http_write_str(fd, "}\n");
}

static void api_handle_rtos(int fd)
{
    const uint64_t ticks = (uint64_t)xTaskGetTickCount();
    const uint64_t tick_hz = (uint64_t)configTICK_RATE_HZ;
    const uint64_t uptime_ms = (tick_hz != 0) ? (ticks * 1000ULL / tick_hz) : 0ULL;

    http_reply_json_hdr(fd, 200, "OK");
    http_write_str(fd, "{");
    json_write_kv_u64(fd, "uptime_ms", uptime_ms, true);
    json_write_kv_u64(fd, "tick_rate_hz", (uint64_t)configTICK_RATE_HZ, true);
    json_write_kv_u64(fd, "heap_free", (uint64_t)xPortGetFreeHeapSize(), true);
    json_write_kv_u64(fd, "heap_min_ever", (uint64_t)xPortGetMinimumEverFreeHeapSize(), false);
    http_write_str(fd, "}\n");
}

static void api_handle_net(int fd)
{
    char ip[24], nm[24], gw[24], mac[24];
    ip[0] = nm[0] = gw[0] = mac[0] = '\0';

    bool link_up = false;
    bool up = false;
    const char *mode = "static";
    bool dhcp = false;

    if (netif) {
        up = netif_is_up(netif) != 0;
        link_up = netif_is_link_up(netif) != 0;
        ip4_to_str(netif_ip4_addr(netif), ip, sizeof(ip));
        ip4_to_str(netif_ip4_netmask(netif), nm, sizeof(nm));
        ip4_to_str(netif_ip4_gw(netif), gw, sizeof(gw));
        mac_to_str(netif->hwaddr, (size_t)netif->hwaddr_len, mac, sizeof(mac));
#if LWIP_DHCP
        dhcp = dhcp_supplied_address(netif) != 0;
        mode = dhcp ? "dhcp" : "static";
#endif
    } else {
        mac_to_str(mac_ethernet_address, 6, mac, sizeof(mac));
    }

    http_reply_json_hdr(fd, 200, "OK");
    http_write_str(fd, "{");
    json_write_kv_str(fd, "ip", ip, true);
    json_write_kv_str(fd, "netmask", nm, true);
    json_write_kv_str(fd, "gateway", gw, true);
    json_write_kv_str(fd, "mac", mac, true);
    json_write_kv_str(fd, "mode", mode, true);
    json_write_kv_bool(fd, "dhcp", dhcp, true);
    json_write_kv_bool(fd, "up", up, true);
    json_write_kv_bool(fd, "link_up", link_up, false);
    http_write_str(fd, "}\n");
}

static const char *json_skip_ws(const char *p)
{
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *json_find_key(const char *json, const char *key)
{
    if (!json || !key) return NULL;
    char pat[64];
    int n = snprintf(pat, sizeof(pat), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(pat)) return NULL;
    const char *p = json;
    while (p && *p) {
        p = strstr(p, pat);
        if (!p) return NULL;
        const char *q = p + strlen(pat);
        q = json_skip_ws(q);
        if (q && *q == ':') {
            q++;
            return json_skip_ws(q);
        }
        p = q ? q : (p + 1);
    }
    return NULL;
}

static int json_get_string_val(const char *json, const char *key, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return 0;
    out[0] = '\0';
    const char *p = json_find_key(json, key);
    if (!p || *p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_sz) {
        if (*p == '\\' && p[1]) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (*p == '"') ? 1 : 0;
}

static int json_get_bool_val(const char *json, const char *key, int *out)
{
    if (out) *out = 0;
    const char *p = json_find_key(json, key);
    if (!p) return 0;
    if (strncmp(p, "true", 4) == 0) { if (out) *out = 1; return 1; }
    if (strncmp(p, "false", 5) == 0) { if (out) *out = 0; return 1; }
    return 0;
}

static int json_get_u32_val(const char *json, const char *key, uint32_t *out)
{
    if (out) *out = 0;
    const char *p = json_find_key(json, key);
    if (!p) return 0;
    unsigned long v = strtoul(p, NULL, 0);
    if (out) *out = (uint32_t)v;
    return 1;
}

static int json_parse_u8_array(const char *json, const char *key, uint8_t *out, size_t out_max, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!out || out_max == 0) return 0;
    const char *p = json_find_key(json, key);
    if (!p) return 0;
    p = json_skip_ws(p);
    if (!p || *p != '[') return 0;
    p++;
    size_t n = 0;
    while (p && *p) {
        p = json_skip_ws(p);
        if (*p == ']') {
            if (out_len) *out_len = n;
            return 1;
        }
        if (n >= out_max) return 0;
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);
        if (!end || end == p) return 0;
        if (v > 255UL) return 0;
        out[n++] = (uint8_t)v;
        p = json_skip_ws(end);
        if (*p == ',') { p++; continue; }
        if (*p == ']') {
            if (out_len) *out_len = n;
            return 1;
        }
        return 0;
    }
    return 0;
}

static int json_parse_rules_array(const char *json, const char *key, i2c_rule_entry_t *out, size_t out_max, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!json || !key || !out || out_max == 0) return 0;
    const char *p = json_find_key(json, key);
    if (!p) return 0;
    p = json_skip_ws(p);
    if (!p || *p != '[') return 0;
    p++;
    size_t n = 0;
    while (p && *p) {
        p = json_skip_ws(p);
        if (*p == ']') {
            if (out_len) *out_len = n;
            return 1;
        }
        if (n >= out_max) return 0;
        if (*p != '{') return 0;
        p++;
        bool have_reg = false;
        bool have_val = false;
        uint32_t reg = 0;
        uint32_t val = 0;
        while (p && *p) {
            p = json_skip_ws(p);
            if (*p == '}') { p++; break; }
            if (*p != '"') return 0;
            p++;
            const char *kstart = p;
            while (*p && *p != '"') p++;
            if (*p != '"') return 0;
            size_t klen = (size_t)(p - kstart);
            p++;
            p = json_skip_ws(p);
            if (*p != ':') return 0;
            p++;
            p = json_skip_ws(p);
            char *end = NULL;
            unsigned long num = strtoul(p, &end, 0);
            if (!end || end == p) return 0;
            p = end;
            if (klen == 3 && strncmp(kstart, "reg", 3) == 0) { reg = (uint32_t)num; have_reg = true; }
            else if (klen == 3 && strncmp(kstart, "val", 3) == 0) { val = (uint32_t)num; have_val = true; }
            p = json_skip_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == '}') { p++; break; }
        }
        if (!have_reg || !have_val || reg > 255U || val > 255U) return 0;
        out[n].reg = (uint8_t)reg;
        out[n].val = (uint8_t)val;
        n++;
        p = json_skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') {
            if (out_len) *out_len = n;
            return 1;
        }
    }
    return 0;
}

static int parse_ipv4_str(const char *s, uint32_t *out_ip_be)
{
    if (!s || !out_ip_be) return 0;
    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    *out_ip_be = ((a & 0xffu) << 24) | ((b & 0xffu) << 16) | ((c & 0xffu) << 8) | (d & 0xffu);
    return 1;
}

static uint32_t netmask_be_from_prefix(int pfx, int *ok)
{
    if (ok) *ok = 0;
    if (pfx < 0 || pfx > 32) return 0;
    uint32_t m = (pfx == 0) ? 0u : (0xffffffffu << (32 - pfx));
    if (ok) *ok = 1;
    return m;
}

static int parse_mac_str(const char *s, uint8_t mac[6])
{
    if (!s || !mac) return 0;
    int v[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return 0;
    for (int i = 0; i < 6; ++i) {
        if (v[i] < 0 || v[i] > 255) return 0;
        mac[i] = (uint8_t)v[i];
    }
    return 1;
}

static int read_body_to_buf(http_conn_t *conn, char *buf, size_t buf_sz, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!conn || !buf || buf_sz < 2) return 0;
    size_t got = 0;
    while (got + 1 < buf_sz) {
        int r = conn->read_body(conn->read_user, buf + got, buf_sz - 1 - got);
        if (r < 0) return 0;
        if (r == 0) break;
        got += (size_t)r;
    }
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return 1;
}

static void api_handle_net_put(http_conn_t *conn)
{
    if (!conn) return;
    char body[1024];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }

    char ip_s[32], nm_s[32], gw_s[32], mac_s[32];
    ip_s[0] = nm_s[0] = gw_s[0] = mac_s[0] = '\0';

    (void)json_get_string_val(body, "ip", ip_s, sizeof(ip_s));
    (void)json_get_string_val(body, "netmask", nm_s, sizeof(nm_s));
    (void)json_get_string_val(body, "gateway", gw_s, sizeof(gw_s));
    (void)json_get_string_val(body, "mac", mac_s, sizeof(mac_s));

    int apply = 1;
    (void)json_get_bool_val(body, "apply", &apply);

    int prefix_ok = 0;
    uint32_t ip_be = 0, nm_be = 0, gw_be = 0;
    uint8_t mac[6];

    if (ip_s[0] && strstr(ip_s, "/")) {
        // Support "ip":"a.b.c.d/prefix"
        char tmp[40];
        strncpy(tmp, ip_s, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *slash = strchr(tmp, '/');
        if (!slash) { http_reply_simple(conn->fd, 400, "Bad Request", "bad ip\r\n"); return; }
        *slash = '\0';
        int pfx = atoi(slash + 1);
        nm_be = netmask_be_from_prefix(pfx, &prefix_ok);
        if (!prefix_ok || !parse_ipv4_str(tmp, &ip_be)) {
            http_reply_simple(conn->fd, 400, "Bad Request", "bad ip/prefix\r\n");
            return;
        }
    } else {
        if (!ip_s[0] || !parse_ipv4_str(ip_s, &ip_be)) {
            http_reply_simple(conn->fd, 400, "Bad Request", "missing/bad ip\r\n");
            return;
        }
        if (nm_s[0]) {
            if (!parse_ipv4_str(nm_s, &nm_be)) {
                http_reply_simple(conn->fd, 400, "Bad Request", "bad netmask\r\n");
                return;
            }
        } else {
            // optional: accept numeric prefix
            const char *p = json_find_key(body, "prefix");
            if (p) {
                int pfx = atoi(p);
                nm_be = netmask_be_from_prefix(pfx, &prefix_ok);
                if (!prefix_ok) {
                    http_reply_simple(conn->fd, 400, "Bad Request", "bad prefix\r\n");
                    return;
                }
            } else {
                http_reply_simple(conn->fd, 400, "Bad Request", "missing netmask/prefix\r\n");
                return;
            }
        }
    }

    if (!gw_s[0] || !parse_ipv4_str(gw_s, &gw_be)) {
        http_reply_simple(conn->fd, 400, "Bad Request", "missing/bad gateway\r\n");
        return;
    }
    if (!mac_s[0] || !parse_mac_str(mac_s, mac)) {
        http_reply_simple(conn->fd, 400, "Bad Request", "missing/bad mac\r\n");
        return;
    }

    network_config_t cfg;
    if (!config_store_get_network(&cfg)) memset(&cfg, 0, sizeof(cfg));
    cfg.ip_be = ip_be;
    cfg.netmask_be = nm_be;
    cfg.gateway_be = gw_be;
    cfg.has_ip = true;
    cfg.has_netmask = true;
    cfg.has_gateway = true;
    memcpy(cfg.mac, mac, 6);
    cfg.has_mac = true;

    (void)config_store_set_network(&cfg);
    int saved = config_store_save_network();

    if (apply && netif) {
        ip_addr_t ipaddr, netmask, gw;
        IP4_ADDR(&ipaddr,
                 (cfg.ip_be >> 24) & 0xff, (cfg.ip_be >> 16) & 0xff,
                 (cfg.ip_be >> 8) & 0xff, (cfg.ip_be >> 0) & 0xff);
        IP4_ADDR(&netmask,
                 (cfg.netmask_be >> 24) & 0xff, (cfg.netmask_be >> 16) & 0xff,
                 (cfg.netmask_be >> 8) & 0xff, (cfg.netmask_be >> 0) & 0xff);
        IP4_ADDR(&gw,
                 (cfg.gateway_be >> 24) & 0xff, (cfg.gateway_be >> 16) & 0xff,
                 (cfg.gateway_be >> 8) & 0xff, (cfg.gateway_be >> 0) & 0xff);
        netif_set_ipaddr(netif, &ipaddr);
        netif_set_netmask(netif, &netmask);
        netif_set_gw(netif, &gw);
        memcpy(mac_ethernet_address, cfg.mac, 6);
        if (netif->hwaddr_len >= 6) memcpy(netif->hwaddr, cfg.mac, 6);
    }

    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{");
    json_write_kv_bool(conn->fd, "ok", true, true);
    json_write_kv_bool(conn->fd, "saved", saved != 0, true);
    json_write_kv_bool(conn->fd, "applied", (apply && netif) ? true : false, false);
    http_write_str(conn->fd, "}\n");
}

static bool fs_drive_str_from_root(const char *root, char out[4])
{
    if (!root || !root[0] || root[1] != ':') return false;
    out[0] = root[0];
    out[1] = ':';
    out[2] = '\0';
    return true;
}

static void api_write_fs_one(int fd, const char *name, const fs_device_info_t *dev, bool comma)
{
    http_write_str(fd, "{");
    json_write_kv_str(fd, "name", name, true);
    bool ready = (dev && dev->ctx && fs_shared_is_ready(dev->ctx));
    json_write_kv_bool(fd, "ready", ready, true);

    uint64_t total = 0, freeb = 0;
    if (ready && ctx_lock(dev->ctx)) {
        char drv[4];
        if (fs_drive_str_from_root(dev->ctx->root, drv)) {
            FATFS *fs = NULL;
            DWORD fre_clust = 0;
            if (f_getfree(drv, &fre_clust, &fs) == FR_OK && fs) {
                uint64_t csize = (uint64_t)fs->csize;
                uint64_t ss = 512ULL;
                total = (uint64_t)(fs->n_fatent - 2U) * csize * ss;
                freeb = (uint64_t)fre_clust * csize * ss;
            }
        }
        ctx_unlock(dev->ctx);
    }
    json_write_kv_u64(fd, "total_bytes", total, true);
    json_write_kv_u64(fd, "free_bytes", freeb, false);
    http_write_str(fd, "}");
    if (comma) http_write_str(fd, ",");
}

static void api_handle_fs(int fd)
{
    const fs_device_info_t *sd = fs_device_by_name("sd");
    const fs_device_info_t *flash = fs_device_by_name("flash");

    http_reply_json_hdr(fd, 200, "OK");
    http_write_str(fd, "{");
    http_write_str(fd, "\"volumes\":[");
    api_write_fs_one(fd, "flash", flash, true);
    api_write_fs_one(fd, "sd", sd, false);
    http_write_str(fd, "]}");
    http_write_str(fd, "\n");
}

static void api_handle_qspi(int fd)
{
    http_reply_json_hdr(fd, 200, "OK");
    http_write_str(fd, "{");
    json_write_kv_bool(fd, "ready", qspi_fs_is_ready() != 0, true);
    json_write_kv_u64(fd, "fs_base_bytes", (uint64_t)QSPI_FS_BASE_BYTES, true);
    json_write_kv_u64(fd, "fs_size_bytes", (uint64_t)QSPI_FS_SIZE_BYTES, false);
    http_write_str(fd, "}\n");
}

static void api_write_i2c_rules(int fd, const char *key, const i2c_rule_entry_t *arr, size_t len, bool comma)
{
    http_write_str(fd, "\"");
    http_write_str(fd, key);
    http_write_str(fd, "\":[");
    for (size_t i = 0; i < len; ++i) {
        char b[48];
        int n = snprintf(b, sizeof(b), "%s{\"reg\":%u,\"val\":%u}", (i == 0) ? "" : ",",
                         (unsigned)arr[i].reg, (unsigned)arr[i].val);
        if (n > 0) http_write_str(fd, b);
    }
    http_write_str(fd, "]");
    if (comma) http_write_str(fd, ",");
}

static void api_write_u8_array(int fd, const char *key, const uint8_t *arr, size_t len, bool comma)
{
    http_write_str(fd, "\"");
    http_write_str(fd, key);
    http_write_str(fd, "\":[");
    for (size_t i = 0; i < len; ++i) {
        char b[24];
        int n = snprintf(b, sizeof(b), "%s%u", (i == 0) ? "" : ",", (unsigned)arr[i]);
        if (n > 0) http_write_str(fd, b);
    }
    http_write_str(fd, "]");
    if (comma) http_write_str(fd, ",");
}

static void api_handle_i2c(int fd, const char *name_opt)
{
    http_reply_json_hdr(fd, 200, "OK");
    http_write_str(fd, "{");
    json_write_kv_bool(fd, "ready", config_store_is_ready() != 0, true);
    if (name_opt && name_opt[0]) {
        const i2c_device_config_t *d = config_store_find_i2c_device_by_name(name_opt);
        if (!d) {
            http_write_str(fd, "\"error\":\"unknown device\"}\n");
            return;
        }
        http_write_str(fd, "\"device\":{");
        http_write_str(fd, "\"name\":");
        json_write_escaped(fd, d->name);
        http_write_str(fd, ",\"addr_7b\":");
        {
            char b[16];
            int n = snprintf(b, sizeof(b), "%u", (unsigned)d->addr_7b);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"file_name\":");
        json_write_escaped(fd, d->file_name);
        http_write_str(fd, ",\"policy\":");
        json_write_escaped(fd, (d->policy == I2C_POLICY_BLACKLIST) ? "blacklist" : "whitelist");
        http_write_str(fd, ",\"autopoll_enabled\":");
        http_write_str(fd, d->autopoll_enabled ? "true" : "false");
        http_write_str(fd, ",\"autopoll_reg_delay_ms\":");
        {
            char b[24];
            int n = snprintf(b, sizeof(b), "%lu", (unsigned long)d->autopoll_reg_delay_ms);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"autopoll_cycle_delay_ms\":");
        {
            char b[24];
            int n = snprintf(b, sizeof(b), "%lu", (unsigned long)d->autopoll_cycle_delay_ms);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",");
        api_write_u8_array(fd, "autopoll_regs", d->autopoll_regs, d->autopoll_regs_len, true);
        api_write_i2c_rules(fd, "whitelist", d->whitelist, d->whitelist_len, true);
        api_write_i2c_rules(fd, "blacklist", d->blacklist, d->blacklist_len, false);
        http_write_str(fd, "}}");
        http_write_str(fd, "\n");
        return;
    }

    size_t cnt = config_store_get_i2c_device_count();
    json_write_kv_u64(fd, "count", (uint64_t)cnt, true);
    http_write_str(fd, "\"devices\":[");

    const i2c_device_config_t *devs = config_store_get_i2c_devices();
    for (size_t i = 0; i < cnt; ++i) {
        const i2c_device_config_t *d = &devs[i];
        http_write_str(fd, "{");
        http_write_str(fd, "\"name\":");
        json_write_escaped(fd, d->name);
        http_write_str(fd, ",\"addr_7b\":");
        {
            char b[16];
            int n = snprintf(b, sizeof(b), "%u", (unsigned)d->addr_7b);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"file_name\":");
        json_write_escaped(fd, d->file_name);
        http_write_str(fd, ",\"policy\":");
        json_write_escaped(fd, (d->policy == I2C_POLICY_BLACKLIST) ? "blacklist" : "whitelist");
        http_write_str(fd, ",\"autopoll_enabled\":");
        http_write_str(fd, d->autopoll_enabled ? "true" : "false");
        http_write_str(fd, ",\"autopoll_regs_len\":");
        {
            char b[16];
            int n = snprintf(b, sizeof(b), "%u", (unsigned)d->autopoll_regs_len);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"autopoll_reg_delay_ms\":");
        {
            char b[24];
            int n = snprintf(b, sizeof(b), "%lu", (unsigned long)d->autopoll_reg_delay_ms);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"autopoll_cycle_delay_ms\":");
        {
            char b[24];
            int n = snprintf(b, sizeof(b), "%lu", (unsigned long)d->autopoll_cycle_delay_ms);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"settings_len\":");
        {
            char b[16];
            int n = snprintf(b, sizeof(b), "%u", (unsigned)d->settings_len);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"whitelist_len\":");
        {
            char b[16];
            int n = snprintf(b, sizeof(b), "%u", (unsigned)d->whitelist_len);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, ",\"blacklist_len\":");
        {
            char b[16];
            int n = snprintf(b, sizeof(b), "%u", (unsigned)d->blacklist_len);
            if (n > 0) http_write_str(fd, b);
        }
        http_write_str(fd, "}");
        if (i + 1 < cnt) http_write_str(fd, ",");
    }

    http_write_str(fd, "]}");
    http_write_str(fd, "\n");
}

static void api_handle_i2c_put(http_conn_t *conn)
{
    if (!conn) return;
    if (!config_store_is_ready()) {
        http_reply_simple(conn->fd, 503, "Service Unavailable", "config_store not ready\r\n");
        return;
    }

    char body[2048];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }

    char name[I2C_CFG_NAME_MAX];
    name[0] = '\0';
    if (!json_get_string_val(body, "name", name, sizeof(name)) || !name[0]) {
        http_reply_simple(conn->fd, 400, "Bad Request", "missing name\r\n");
        return;
    }

    const i2c_device_config_t *cur = config_store_find_i2c_device_by_name(name);
    if (!cur) {
        http_reply_simple(conn->fd, 404, "Not Found", "unknown device\r\n");
        return;
    }

    i2c_device_config_t next = *cur;

    char policy[16];
    policy[0] = '\0';
    if (json_get_string_val(body, "policy", policy, sizeof(policy)) && policy[0]) {
        if (strcasecmp(policy, "whitelist") == 0) next.policy = I2C_POLICY_WHITELIST;
        else if (strcasecmp(policy, "blacklist") == 0) next.policy = I2C_POLICY_BLACKLIST;
        else {
            http_reply_simple(conn->fd, 400, "Bad Request", "bad policy\r\n");
            return;
        }
    }

    int ap_en = -1;
    if (json_get_bool_val(body, "autopoll_enabled", &ap_en)) {
        next.autopoll_enabled = ap_en ? true : false;
    }

    uint32_t v = 0;
    if (json_get_u32_val(body, "autopoll_reg_delay_ms", &v)) next.autopoll_reg_delay_ms = v;
    if (json_get_u32_val(body, "autopoll_cycle_delay_ms", &v)) next.autopoll_cycle_delay_ms = v;

    size_t regs_len = 0;
    if (json_find_key(body, "autopoll_regs")) {
        if (!json_parse_u8_array(body, "autopoll_regs", next.autopoll_regs, I2C_CFG_AUTOPOLL_REGS_MAX, &regs_len)) {
            http_reply_simple(conn->fd, 400, "Bad Request", "bad autopoll_regs\r\n");
            return;
        }
        next.autopoll_regs_len = regs_len;
    }

    size_t rules_len = 0;
    if (json_find_key(body, "whitelist")) {
        if (!json_parse_rules_array(body, "whitelist", next.whitelist, I2C_CFG_RULES_MAX, &rules_len)) {
            http_reply_simple(conn->fd, 400, "Bad Request", "bad whitelist\r\n");
            return;
        }
        next.whitelist_len = rules_len;
    }
    if (json_find_key(body, "blacklist")) {
        if (!json_parse_rules_array(body, "blacklist", next.blacklist, I2C_CFG_RULES_MAX, &rules_len)) {
            http_reply_simple(conn->fd, 400, "Bad Request", "bad blacklist\r\n");
            return;
        }
        next.blacklist_len = rules_len;
    }

    int ok = config_store_set_i2c_device(&next);
    int saved = ok ? config_store_save_i2c_device(&next) : 0;
    if (!ok) {
        http_reply_simple(conn->fd, 500, "ERR", "set failed\r\n");
        return;
    }

    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{");
    json_write_kv_bool(conn->fd, "ok", true, true);
    json_write_kv_bool(conn->fd, "saved", saved != 0, false);
    http_write_str(conn->fd, "}\n");
}

static bool i2c_find_idx_and_name(const char *name, uint32_t addr_7b, size_t *out_idx, char out_name[I2C_CFG_NAME_MAX])
{
    if (out_idx) *out_idx = 0;
    if (out_name) out_name[0] = '\0';

    if (name && name[0]) {
        size_t idx = 0;
        if (!i2cdev_find_device_index_by_name(name, &idx)) return false;
        if (out_idx) *out_idx = idx;
        if (out_name) {
            strncpy(out_name, name, I2C_CFG_NAME_MAX - 1);
            out_name[I2C_CFG_NAME_MAX - 1] = '\0';
        }
        return true;
    }

    if (addr_7b <= 0x7Fu) {
        size_t idx = 0;
        if (!i2cdev_find_device_index_by_addr((uint8_t)addr_7b, &idx)) return false;
        if (out_idx) *out_idx = idx;
        if (out_name) {
            i2cdev_device_info_t info;
            if (!i2cdev_device_get_info(idx, &info) || !info.name) return false;
            strncpy(out_name, info.name, I2C_CFG_NAME_MAX - 1);
            out_name[I2C_CFG_NAME_MAX - 1] = '\0';
        }
        return true;
    }
    return false;
}

static void api_diag_i2c_read(http_conn_t *conn)
{
    if (!conn) return;
    char body[512];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }

    char name[I2C_CFG_NAME_MAX];
    name[0] = '\0';
    (void)json_get_string_val(body, "name", name, sizeof(name));
    uint32_t addr = 0xFFFFFFFFu;
    (void)json_get_u32_val(body, "addr_7b", &addr);
    uint32_t reg = 0;
    if (!json_get_u32_val(body, "reg", &reg) || reg > 0xFFu) {
        http_reply_simple(conn->fd, 400, "Bad Request", "missing/bad reg\r\n");
        return;
    }

    size_t idx = 0;
    char dev_name[I2C_CFG_NAME_MAX];
    if (!i2c_find_idx_and_name(name[0] ? name : NULL, addr, &idx, dev_name)) {
        http_reply_simple(conn->fd, 404, "Not Found", "device not found\r\n");
        return;
    }
    uint8_t val = 0;
    if (!i2cdev_read_reg_dev(idx, (uint8_t)reg, &val)) {
        http_reply_simple(conn->fd, 500, "ERR", "i2c read failed\r\n");
        return;
    }

    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{");
    json_write_kv_bool(conn->fd, "ok", true, true);
    http_write_str(conn->fd, "\"name\":");
    json_write_escaped(conn->fd, dev_name);
    http_write_str(conn->fd, ",\"reg\":");
    {
        char b[16];
        int n = snprintf(b, sizeof(b), "%u", (unsigned)reg);
        if (n > 0) http_write_str(conn->fd, b);
    }
    http_write_str(conn->fd, ",\"val\":");
    {
        char b[16];
        int n = snprintf(b, sizeof(b), "%u", (unsigned)val);
        if (n > 0) http_write_str(conn->fd, b);
    }
    http_write_str(conn->fd, "}\n");
}

static void api_diag_i2c_write(http_conn_t *conn)
{
    if (!conn) return;
    char body[512];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }

    char name[I2C_CFG_NAME_MAX];
    name[0] = '\0';
    (void)json_get_string_val(body, "name", name, sizeof(name));
    uint32_t addr = 0xFFFFFFFFu;
    (void)json_get_u32_val(body, "addr_7b", &addr);
    uint32_t reg = 0, val = 0;
    if (!json_get_u32_val(body, "reg", &reg) || reg > 0xFFu) {
        http_reply_simple(conn->fd, 400, "Bad Request", "missing/bad reg\r\n");
        return;
    }
    if (!json_get_u32_val(body, "val", &val) || val > 0xFFu) {
        http_reply_simple(conn->fd, 400, "Bad Request", "missing/bad val\r\n");
        return;
    }

    size_t idx = 0;
    char dev_name[I2C_CFG_NAME_MAX];
    if (!i2c_find_idx_and_name(name[0] ? name : NULL, addr, &idx, dev_name)) {
        http_reply_simple(conn->fd, 404, "Not Found", "device not found\r\n");
        return;
    }
    if (!i2cdev_write_reg_dev(idx, (uint8_t)reg, (uint8_t)val)) {
        http_reply_simple(conn->fd, 403, "Forbidden", "DENIED\r\n");
        return;
    }
    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{\"ok\":true}\n");
}

static void api_diag_smi_read(http_conn_t *conn)
{
    if (!conn) return;
    char body[256];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }
    uint32_t phy = 0, reg = 0;
    if (!json_get_u32_val(body, "phy", &phy) || phy > 31u) { http_reply_simple(conn->fd, 400, "Bad Request", "bad phy\r\n"); return; }
    if (!json_get_u32_val(body, "reg", &reg) || reg > 31u) { http_reply_simple(conn->fd, 400, "Bad Request", "bad reg\r\n"); return; }
    uint16_t val = 0;
    if (!mdio_read_blocking((uint8_t)phy, (uint8_t)reg, &val, pdMS_TO_TICKS(100))) {
        http_reply_simple(conn->fd, 500, "ERR", "smi read failed\r\n");
        return;
    }
    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{");
    json_write_kv_bool(conn->fd, "ok", true, true);
    http_write_str(conn->fd, "\"val\":");
    {
        char b[16];
        int n = snprintf(b, sizeof(b), "%u", (unsigned)val);
        if (n > 0) http_write_str(conn->fd, b);
    }
    http_write_str(conn->fd, "}\n");
}

static void api_diag_smi_write(http_conn_t *conn)
{
    if (!conn) return;
    char body[256];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }
    uint32_t phy = 0, reg = 0, val = 0;
    if (!json_get_u32_val(body, "phy", &phy) || phy > 31u) { http_reply_simple(conn->fd, 400, "Bad Request", "bad phy\r\n"); return; }
    if (!json_get_u32_val(body, "reg", &reg) || reg > 31u) { http_reply_simple(conn->fd, 400, "Bad Request", "bad reg\r\n"); return; }
    if (!json_get_u32_val(body, "val", &val) || val > 0xFFFFu) { http_reply_simple(conn->fd, 400, "Bad Request", "bad val\r\n"); return; }
    mdio_write((uint8_t)phy, (uint8_t)reg, (uint16_t)val);
    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{\"ok\":true}\n");
}

static void api_diag_mem_read(http_conn_t *conn)
{
    if (!conn) return;
    char body[256];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }
    uint32_t addr = 0;
    if (!json_get_u32_val(body, "addr", &addr)) { http_reply_simple(conn->fd, 400, "Bad Request", "bad addr\r\n"); return; }
    uintptr_t a = (uintptr_t)addr;
    http_reply_json_hdr(conn->fd, 200, "OK");
    if ((a & 3U) == 0U) {
        uint32_t v = Xil_In32((UINTPTR)a);
        char out[96];
        int n = snprintf(out, sizeof(out), "{\"ok\":true,\"width\":32,\"val\":%lu}\n", (unsigned long)v);
        if (n > 0) http_write_str(conn->fd, out);
    } else {
        uint8_t v = Xil_In8((UINTPTR)a);
        char out[96];
        int n = snprintf(out, sizeof(out), "{\"ok\":true,\"width\":8,\"val\":%u}\n", (unsigned)v);
        if (n > 0) http_write_str(conn->fd, out);
    }
}

static void api_diag_mem_write(http_conn_t *conn)
{
    if (!conn) return;
    char body[256];
    size_t blen = 0;
    if (!read_body_to_buf(conn, body, sizeof(body), &blen) || blen == 0) {
        http_reply_simple(conn->fd, 400, "Bad Request", "empty body\r\n");
        return;
    }
    int confirm = 0;
    (void)json_get_bool_val(body, "confirm", &confirm);
    if (!confirm) { http_reply_simple(conn->fd, 400, "Bad Request", "confirm required\r\n"); return; }
    uint32_t addr = 0, val = 0;
    if (!json_get_u32_val(body, "addr", &addr)) { http_reply_simple(conn->fd, 400, "Bad Request", "bad addr\r\n"); return; }
    if (!json_get_u32_val(body, "val", &val)) { http_reply_simple(conn->fd, 400, "Bad Request", "bad val\r\n"); return; }
    uintptr_t a = (uintptr_t)addr;
    if ((a & 3U) == 0U) {
        Xil_Out32((UINTPTR)a, (uint32_t)val);
        http_reply_json_hdr(conn->fd, 200, "OK");
        http_write_str(conn->fd, "{\"ok\":true,\"width\":32}\n");
        return;
    }
    if (val > 0xFFu) { http_reply_simple(conn->fd, 400, "Bad Request", "unaligned requires 8-bit val\r\n"); return; }
    Xil_Out8((UINTPTR)a, (uint8_t)val);
    http_reply_json_hdr(conn->fd, 200, "OK");
    http_write_str(conn->fd, "{\"ok\":true,\"width\":8}\n");
}

static bool map_url_to_fatfs(const char *url_path, char *out_fat, size_t out_sz,
                             const fs_device_info_t **out_dev, bool *out_tar_mode)
{
    if (!url_path || !out_fat || out_sz == 0) return false;
    if (out_tar_mode) *out_tar_mode = false;

    const char *p = url_path;
    if (*p != '/') return false;
    p++;

    bool tar_mode = false;
    if (starts_with(p, "tar/")) {
        tar_mode = true;
        p += 4;
    }

    const fs_device_info_t *dev = NULL;
    const char *dev_name = NULL;
    if (starts_with(p, "sd")) {
        dev_name = "sd";
        p += 2;
    } else if (starts_with(p, "flash")) {
        dev_name = "flash";
        p += 5;
    } else {
        return false;
    }

    dev = fs_device_by_name(dev_name);
    if (!dev || !dev->ctx || !dev->ctx->root) return false;
    if (out_dev) *out_dev = dev;
    if (out_tar_mode) *out_tar_mode = tar_mode;

    if (*p == '\0') {
        strncpy(out_fat, dev->ctx->root, out_sz - 1);
        out_fat[out_sz - 1] = '\0';
        return true;
    }
    if (*p != '/') return false;
    while (*p == '/') p++;

    char tmp[FS_PATH_MAX * 2];
    strncpy(tmp, p, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    url_decode_inplace(tmp);

    if (strstr(tmp, "..")) return false;
    if (strchr(tmp, ':')) return false;

    size_t root_len = strlen(dev->ctx->root);
    size_t suf_len = strlen(tmp);
    if (root_len + 1 + suf_len + 1 > out_sz) return false;
    strncpy(out_fat, dev->ctx->root, out_sz - 1);
    out_fat[out_sz - 1] = '\0';
    if (root_len && out_fat[root_len - 1] != '/') strncat(out_fat, "/", out_sz - strlen(out_fat) - 1);
    strncat(out_fat, tmp, out_sz - strlen(out_fat) - 1);
    return true;
}

static bool map_url_to_web_fatfs(const char *url_path, char *out_fat, size_t out_sz, const fs_device_info_t **out_dev)
{
    if (!url_path || !out_fat || out_sz == 0) return false;
    if (url_path[0] != '/') return false;

    const fs_device_info_t *dev = fs_device_by_name("flash");
    if (!dev || !dev->ctx || !dev->ctx->root) return false;
    if (out_dev) *out_dev = dev;

    char tmp[FS_PATH_MAX * 2];
    strncpy(tmp, url_path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    strip_query_fragment_inplace(tmp);

    // tmp starts with '/', skip it.
    const char *p = tmp + 1;
    while (*p == '/') p++;

    // Default document.
    char rel[FS_PATH_MAX * 2];
    if (*p == '\0') {
        strncpy(rel, "index.html", sizeof(rel) - 1);
        rel[sizeof(rel) - 1] = '\0';
    } else {
        strncpy(rel, p, sizeof(rel) - 1);
        rel[sizeof(rel) - 1] = '\0';
    }
    url_decode_inplace(rel);

    if (strstr(rel, "..")) return false;
    if (strchr(rel, ':')) return false;
    if (strchr(rel, '\\')) return false;

    // If path ends with '/', serve index.html in that directory.
    size_t rel_len = strlen(rel);
    if (rel_len && rel[rel_len - 1] == '/') {
        if (rel_len + WEB_INDEX_NAME_MAX >= sizeof(rel)) return false;
        strncat(rel, "index.html", sizeof(rel) - strlen(rel) - 1);
    }

    // Build: <flash-root>/www/<rel>
    size_t root_len = strlen(dev->ctx->root);
    if (root_len + 1 + strlen(WEB_ROOT_DIR) + 1 + strlen(rel) + 1 > out_sz) return false;
    strncpy(out_fat, dev->ctx->root, out_sz - 1);
    out_fat[out_sz - 1] = '\0';
    if (root_len && out_fat[root_len - 1] != '/') strncat(out_fat, "/", out_sz - strlen(out_fat) - 1);
    strncat(out_fat, WEB_ROOT_DIR, out_sz - strlen(out_fat) - 1);
    strncat(out_fat, "/", out_sz - strlen(out_fat) - 1);
    strncat(out_fat, rel, out_sz - strlen(out_fat) - 1);
    return true;
}

static int tar_sock_write(void *user, const void *buf, size_t len)
{
    int fd = *(int *)user;
    int w = lwip_write(fd, buf, (int)len);
    return (w <= 0) ? -1 : w;
}

static int tar_sock_read(void *user, void *buf, size_t len)
{
    http_conn_t *c = (http_conn_t *)user;
    int r = c->read_body(c->read_user, buf, len);
    return r;
}

static int handle_get_file(int fd, const fs_device_info_t *dev, const char *fat_path)
{
    if (!dev || !dev->ctx) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    FILINFO info;
    FRESULT sres = f_stat(fat_path, &info);
    if (sres != FR_OK || (info.fattrib & AM_DIR)) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 404, "Not Found", "not found\r\n");
        return XST_SUCCESS;
    }

    FIL f;
    FRESULT ores = f_open(&f, fat_path, FA_READ);
    if (ores != FR_OK) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 500, "ERR", "open failed\r\n");
        return XST_SUCCESS;
    }

    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",
        (unsigned long)info.fsize);
    if (hn > 0) lwip_write(fd, hdr, hn);

    uint8_t buf[1024];
    UINT br = 0;
    while (1) {
        FRESULT rres = f_read(&f, buf, sizeof(buf), &br);
        if (rres != FR_OK) break;
        if (br == 0) break;
        int wr = lwip_write(fd, buf, (int)br);
        if (wr <= 0) break;
    }
    f_close(&f);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_get_file_typed(int fd, const fs_device_info_t *dev, const char *fat_path, const char *content_type)
{
    if (!dev || !dev->ctx) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    FILINFO info;
    FRESULT sres = f_stat(fat_path, &info);
    if (sres != FR_OK) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 404, "Not Found", "not found\r\n");
        return XST_SUCCESS;
    }

    // If it's a directory, try <dir>/index.html.
    if (info.fattrib & AM_DIR) {
        char idx_path[FS_PATH_MAX * 2];
        strncpy(idx_path, fat_path, sizeof(idx_path) - 1);
        idx_path[sizeof(idx_path) - 1] = '\0';
        size_t n = strlen(idx_path);
        if (n && idx_path[n - 1] != '/') strncat(idx_path, "/", sizeof(idx_path) - strlen(idx_path) - 1);
        strncat(idx_path, "index.html", sizeof(idx_path) - strlen(idx_path) - 1);
        ctx_unlock(dev->ctx);
        return handle_get_file_typed(fd, dev, idx_path, "text/html; charset=utf-8");
    }

    FIL f;
    FRESULT ores = f_open(&f, fat_path, FA_READ);
    if (ores != FR_OK) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 500, "ERR", "open failed\r\n");
        return XST_SUCCESS;
    }

    const char *ct = (content_type && content_type[0]) ? content_type : "application/octet-stream";
    char hdr[320];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",
        ct, (unsigned long)info.fsize);
    if (hn > 0) lwip_write(fd, hdr, hn);

    uint8_t buf[1024];
    UINT br = 0;
    while (1) {
        FRESULT rres = f_read(&f, buf, sizeof(buf), &br);
        if (rres != FR_OK) break;
        if (br == 0) break;
        int wr = lwip_write(fd, buf, (int)br);
        if (wr <= 0) break;
    }
    f_close(&f);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_put_file(int fd, const fs_device_info_t *dev, const char *fat_path, http_conn_t *conn)
{
    if (!dev || !dev->ctx || !conn) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    FIL f;
    FRESULT ores = f_open(&f, fat_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (ores != FR_OK) {
        ctx_unlock(dev->ctx);
        http_reply_simple(fd, 500, "ERR", "open failed\r\n");
        return XST_SUCCESS;
    }

    uint8_t buf[1024];
    while (1) {
        int r = conn->read_body(conn->read_user, buf, sizeof(buf));
        if (r < 0) { http_reply_simple(fd, 400, "Bad Request", "read body failed\r\n"); break; }
        if (r == 0) { http_reply_simple(fd, 200, "OK", "OK\r\n"); break; }
        UINT bw = 0;
        FRESULT wres = f_write(&f, buf, (UINT)r, &bw);
        if (wres != FR_OK || bw != (UINT)r) { http_reply_simple(fd, 500, "ERR", "write failed\r\n"); break; }
    }
    f_close(&f);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_get_tar(int fd, const fs_device_info_t *dev, const char *fat_dir_path)
{
    if (!dev || !dev->ctx) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/x-tar\r\n"
        "\r\n");
    if (hn > 0) lwip_write(fd, hdr, hn);

    int u = fd;
    (void)tar_stream_dir_fatfs(dev->ctx, fat_dir_path, tar_sock_write, &u);
    ctx_unlock(dev->ctx);
    return XST_SUCCESS;
}

static int handle_put_tar(int fd, const fs_device_info_t *dev, const char *fat_dir_path, http_conn_t *conn)
{
    if (!dev || !dev->ctx || !conn) return XST_FAILURE;
    if (fs_device_prepare(dev) != XST_SUCCESS) return XST_FAILURE;
    if (!ctx_lock(dev->ctx)) return XST_FAILURE;

    int rc = tar_extract_into_dir_fatfs(dev->ctx, fat_dir_path, tar_sock_read, conn);
    ctx_unlock(dev->ctx);
    if (rc == XST_SUCCESS) http_reply_simple(fd, 200, "OK", "OK\r\n");
    else http_reply_simple(fd, 400, "Bad Request", "tar extract failed\r\n");
    return XST_SUCCESS;
}

int http_handle_request(const http_request_t *req, http_conn_t *conn)
{
    if (!req || !conn || !req->method || !req->path) return 0;

    // 0) JSON API: /api/...
    if (starts_with(req->path, "/api/")) {
        char api_path[64];
        copy_path_no_query(req->path, api_path, sizeof(api_path));
        char q_name[I2C_CFG_NAME_MAX];
        q_name[0] = '\0';
        (void)query_get_param(req->path, "name", q_name, sizeof(q_name));

        if (strcasecmp(req->method, "GET") == 0) {
            if (strcmp(api_path, "/api/net") == 0) { api_handle_net(conn->fd); return 1; }
            if (strcmp(api_path, "/api/rtos") == 0) { api_handle_rtos(conn->fd); return 1; }
            if (strcmp(api_path, "/api/fs") == 0) { api_handle_fs(conn->fd); return 1; }
            if (strcmp(api_path, "/api/qspi") == 0) { api_handle_qspi(conn->fd); return 1; }
            if (strcmp(api_path, "/api/i2c") == 0) { api_handle_i2c(conn->fd, q_name[0] ? q_name : NULL); return 1; }
            if (strcmp(api_path, "/api/version") == 0) { api_handle_version(conn->fd); return 1; }
            http_reply_simple(conn->fd, 404, "Not Found", "api not found\r\n");
            return 1;
        }
        if (strcasecmp(req->method, "PUT") == 0) {
            if (!(req->has_content_length || req->chunked)) {
                http_reply_simple(conn->fd, 411, "Length Required", "need Content-Length or chunked\r\n");
                return 1;
            }
            if (strcmp(api_path, "/api/net") == 0) { api_handle_net_put(conn); return 1; }
            if (strcmp(api_path, "/api/i2c") == 0) { api_handle_i2c_put(conn); return 1; }
            if (strcmp(api_path, "/api/diag/i2c/read") == 0) { api_diag_i2c_read(conn); return 1; }
            if (strcmp(api_path, "/api/diag/i2c/write") == 0) { api_diag_i2c_write(conn); return 1; }
            if (strcmp(api_path, "/api/diag/smi/read") == 0) { api_diag_smi_read(conn); return 1; }
            if (strcmp(api_path, "/api/diag/smi/write") == 0) { api_diag_smi_write(conn); return 1; }
            if (strcmp(api_path, "/api/diag/mem/read") == 0) { api_diag_mem_read(conn); return 1; }
            if (strcmp(api_path, "/api/diag/mem/write") == 0) { api_diag_mem_write(conn); return 1; }
            http_reply_simple(conn->fd, 404, "Not Found", "api not found\r\n");
            return 1;
        }
        http_reply_simple(conn->fd, 405, "Method Not Allowed", "only GET/PUT\r\n");
        return 1;
    }

    // 1) File/dir transfer API: /sd, /flash, /tar/...
    char fat_path[FS_PATH_MAX * 2];
    const fs_device_info_t *dev = NULL;
    bool tar_mode = false;
    if (map_url_to_fatfs(req->path, fat_path, sizeof(fat_path), &dev, &tar_mode)) {
        if (strcasecmp(req->method, "GET") == 0) {
            if (tar_mode) (void)handle_get_tar(conn->fd, dev, fat_path);
            else (void)handle_get_file(conn->fd, dev, fat_path);
            return 1;
        }
        if (strcasecmp(req->method, "PUT") == 0) {
            if (!(req->has_content_length || req->chunked)) {
                http_reply_simple(conn->fd, 411, "Length Required", "need Content-Length or chunked\r\n");
                return 1;
            }
            if (tar_mode) (void)handle_put_tar(conn->fd, dev, fat_path, conn);
            else (void)handle_put_file(conn->fd, dev, fat_path, conn);
            return 1;
        }
        http_reply_simple(conn->fd, 405, "Method Not Allowed", "only GET/PUT\r\n");
        return 1;
    }

    // 2) Web UI (static files from flash:/www), GET-only.
    if (strcasecmp(req->method, "GET") == 0) {
        const fs_device_info_t *wdev = NULL;
        if (!map_url_to_web_fatfs(req->path, fat_path, sizeof(fat_path), &wdev)) {
            return 0;
        }
        (void)handle_get_file_typed(conn->fd, wdev, fat_path, mime_from_path(fat_path));
        return 1;
    }
    return 0;
}
