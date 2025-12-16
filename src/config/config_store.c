#include "config_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ff.h"
#include "xil_printf.h"

#include "task.h"

#include "../qspi_fs/qspi_fs.h"

#include "default_configs.h"

/* Primary on-device config directory is flash:/config (legacy: flash:/configs). */
#define CONFIG_DIR_PRIMARY "config"
#define CONFIG_DIR_FALLBACK "configs"
#define NETWORK_CFG_NAME "network.json"
#define I2C_CFG_SUBDIR_NAME "i2c"

static void build_flash_path(char *out, size_t out_sz, const char *suffix_no_slash);
static int file_exists(const char *path);
static int read_file_to_buf(const char *path, char *buf, size_t buf_sz, size_t *out_len);

static void build_cfg_path(char *out_primary, size_t out_primary_sz,
                           char *out_fallback, size_t out_fallback_sz,
                           const char *rel_suffix)
{
    char p1[160];
    char p2[160];
    if (!rel_suffix) rel_suffix = "";
    if (rel_suffix[0] == '/') rel_suffix++;
    if (rel_suffix[0]) {
        snprintf(p1, sizeof(p1), "%s/%s", CONFIG_DIR_PRIMARY, rel_suffix);
        snprintf(p2, sizeof(p2), "%s/%s", CONFIG_DIR_FALLBACK, rel_suffix);
    } else {
        snprintf(p1, sizeof(p1), "%s", CONFIG_DIR_PRIMARY);
        snprintf(p2, sizeof(p2), "%s", CONFIG_DIR_FALLBACK);
    }
    build_flash_path(out_primary, out_primary_sz, p1);
    build_flash_path(out_fallback, out_fallback_sz, p2);
}

static int dir_exists(const char *path)
{
    FILINFO info;
    FRESULT r = f_stat(path, &info);
    return (r == FR_OK) && ((info.fattrib & AM_DIR) != 0);
}

static int read_file_first_available(const char *p1, const char *p2, char *buf, size_t buf_sz, size_t *out_len)
{
    if (p1 && file_exists(p1) && read_file_to_buf(p1, buf, buf_sz, out_len)) return 1;
    if (p2 && file_exists(p2) && read_file_to_buf(p2, buf, buf_sz, out_len)) return 1;
    return 0;
}

#define CONFIG_TASK_STACK 2048
#define CONFIG_TASK_PRIO (tskIDLE_PRIORITY + 3)

static TaskHandle_t s_task = NULL;
static volatile int s_ready = 0;
static network_config_t s_net_cfg;
static i2c_device_config_t s_i2c_cfgs[I2C_CFG_MAX_DEVICES];
static size_t s_i2c_cfg_count = 0;
static char s_cfg_buf[16384];
static char s_i2c_save_buf[16384];

static void config_apply_defaults(network_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->mac[0] = 0x00;
    cfg->mac[1] = 0x0a;
    cfg->mac[2] = 0x35;
    cfg->mac[3] = 0x00;
    cfg->mac[4] = 0x01;
    cfg->mac[5] = 0x02;
    cfg->has_mac = true;
}

static void build_flash_path(char *out, size_t out_sz, const char *suffix_no_slash)
{
    if (!out || out_sz == 0) return;
    const char *root = QSPI_ROOT;
    size_t root_len = strlen(root);
    size_t suf_len = suffix_no_slash ? strlen(suffix_no_slash) : 0;
    if (root_len + suf_len + 2 >= out_sz) {
        out[0] = '\0';
        return;
    }
    strncpy(out, root, out_sz - 1);
    out[out_sz - 1] = '\0';
    if (root_len == 0 || out[root_len - 1] != '/') {
        strncat(out, "/", out_sz - strlen(out) - 1);
    }
    if (suffix_no_slash && suffix_no_slash[0] == '/') suffix_no_slash++;
    if (suffix_no_slash && suffix_no_slash[0]) {
        strncat(out, suffix_no_slash, out_sz - strlen(out) - 1);
    }
}

static int ensure_dir_exists(const char *path)
{
    FRESULT r = f_mkdir(path);
    if (r == FR_OK || r == FR_EXIST) return 1;
    return 0;
}

static int file_exists(const char *path)
{
    FILINFO info;
    FRESULT r = f_stat(path, &info);
    return (r == FR_OK) && ((info.fattrib & AM_DIR) == 0);
}

static int write_file_atomic(const char *final_path, const void *data, size_t len)
{
    if (!final_path || !data) return 0;
    char tmp[128];
    size_t n = strlen(final_path);
    if (n + 5 >= sizeof(tmp)) return 0;
    snprintf(tmp, sizeof(tmp), "%s.tmp", final_path);

    FIL f;
    FRESULT r = f_open(&f, tmp, FA_WRITE | FA_CREATE_ALWAYS);
    if (r != FR_OK) return 0;
    UINT bw = 0;
    r = f_write(&f, data, (UINT)len, &bw);
    (void)f_sync(&f);
    (void)f_close(&f);
    if (r != FR_OK || bw != (UINT)len) {
        (void)f_unlink(tmp);
        return 0;
    }
    (void)f_unlink(final_path);
    r = f_rename(tmp, final_path);
    if (r != FR_OK) {
        (void)f_unlink(tmp);
        return 0;
    }
    return 1;
}

static int copy_file_atomic(const char *src_path, const char *dst_path, char *buf, size_t buf_sz)
{
    if (!src_path || !dst_path || !buf || buf_sz < 2) return 0;
    size_t blen = 0;
    if (!read_file_to_buf(src_path, buf, buf_sz, &blen)) return 0;
    return write_file_atomic(dst_path, buf, blen) ? 1 : 0;
}

static int read_file_to_buf(const char *path, char *buf, size_t buf_sz, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!path || !buf || buf_sz < 2) return 0;
    FIL f;
    FRESULT r = f_open(&f, path, FA_READ);
    if (r != FR_OK) return 0;
    UINT br = 0;
    r = f_read(&f, buf, (UINT)(buf_sz - 1), &br);
    (void)f_close(&f);
    if (r != FR_OK) return 0;
    buf[br] = '\0';
    if (out_len) *out_len = (size_t)br;
    return 1;
}

static const char *skip_ws(const char *p)
{
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *find_key(const char *json, const char *key)
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
        q = skip_ws(q);
        if (q && *q == ':') {
            q++;
            return skip_ws(q);
        }
        /* This was a string value match (e.g. policy:"whitelist"), not a key. Keep searching. */
        p = q ? q : (p + 1);
    }
    return NULL;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return 0;
    out[0] = '\0';
    const char *p = find_key(json, key);
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

static int json_get_bool(const char *json, const char *key, int *out)
{
    if (out) *out = 0;
    const char *p = find_key(json, key);
    if (!p) return 0;
    if (strncmp(p, "true", 4) == 0) { if (out) *out = 1; return 1; }
    if (strncmp(p, "false", 5) == 0) { if (out) *out = 0; return 1; }
    return 0;
}

static const char *parse_json_string_inplace(const char *p, char *out, size_t out_sz, int *ok)
{
    if (ok) *ok = 0;
    if (!out || out_sz == 0) return p;
    out[0] = '\0';
    p = skip_ws(p);
    if (!p || *p != '"') return p;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_sz) {
        if (*p == '\\' && p[1]) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p != '"') return p;
    if (ok) *ok = 1;
    return p + 1;
}

static const char *parse_json_u32_inplace(const char *p, uint32_t *out, int *ok)
{
    if (ok) *ok = 0;
    if (out) *out = 0;
    p = skip_ws(p);
    if (!p || !*p) return p;

    if (*p == '"') {
        char tmp[32];
        int sok = 0;
        const char *e = parse_json_string_inplace(p, tmp, sizeof(tmp), &sok);
        if (!sok) return p;
        char *end = NULL;
        unsigned long v = strtoul(tmp, &end, 0);
        if (!end || *end != '\0') return p;
        if (out) *out = (uint32_t)v;
        if (ok) *ok = 1;
        return e;
    }

    char *end = NULL;
    unsigned long v = strtoul(p, &end, 0);
    if (!end || end == p) return p;
    if (out) *out = (uint32_t)v;
    if (ok) *ok = 1;
    return end;
}

static const char *skip_json_value_simple(const char *p, int *ok)
{
    if (ok) *ok = 0;
    p = skip_ws(p);
    if (!p || !*p) return p;
    if (*p == '"') {
        char tmp[8];
        int sok = 0;
        const char *e = parse_json_string_inplace(p, tmp, sizeof(tmp), &sok);
        if (ok) *ok = sok;
        return e;
    }
    if (strncmp(p, "true", 4) == 0) { if (ok) *ok = 1; return p + 4; }
    if (strncmp(p, "false", 5) == 0) { if (ok) *ok = 1; return p + 5; }
    if (strncmp(p, "null", 4) == 0) { if (ok) *ok = 1; return p + 4; }
    uint32_t dummy = 0;
    int nok = 0;
    const char *e = parse_json_u32_inplace(p, &dummy, &nok);
    if (ok) *ok = nok;
    return e;
}

static int json_parse_num_array_u8(const char *p, uint8_t *out, size_t out_max, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!p || !out || out_max == 0) return 0;
    p = skip_ws(p);
    if (*p != '[') return 0;
    p++;
    size_t n = 0;
    for (;;) {
        p = skip_ws(p);
        if (!*p) return 0;
        if (*p == ']') { p++; break; }
        uint32_t v = 0;
        int ok = 0;
        p = parse_json_u32_inplace(p, &v, &ok);
        if (!ok) return 0;
        if (v > 0xFFu) return 0;
        if (n < out_max) out[n++] = (uint8_t)(v & 0xFFu);
        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; break; }
        return 0;
    }
    if (out_len) *out_len = n;
    return 1;
}

static int json_parse_rules_array(const char *p, i2c_rule_entry_t *out, size_t out_max, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!p || !out || out_max == 0) return 0;
    p = skip_ws(p);
    if (*p != '[') return 0;
    p++;
    size_t n = 0;
    for (;;) {
        p = skip_ws(p);
        if (!*p) return 0;
        if (*p == ']') { p++; break; }
        if (*p != '{') return 0;
        p++;
        int have_reg = 0, have_val = 0;
        uint32_t reg = 0, val = 0;
        for (;;) {
            p = skip_ws(p);
            if (!*p) return 0;
            if (*p == '}') { p++; break; }
            char key[24];
            int okk = 0;
            p = parse_json_string_inplace(p, key, sizeof(key), &okk);
            if (!okk) return 0;
            p = skip_ws(p);
            if (*p != ':') return 0;
            p++;
            if (strcmp(key, "reg") == 0) {
                int okn = 0;
                p = parse_json_u32_inplace(p, &reg, &okn);
                have_reg = okn;
            } else if (strcmp(key, "val") == 0) {
                int okn = 0;
                p = parse_json_u32_inplace(p, &val, &okn);
                have_val = okn;
            } else {
                int okv = 0;
                p = skip_json_value_simple(p, &okv);
                if (!okv) return 0;
            }

            p = skip_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == '}') { p++; break; }
            return 0;
        }
        if (have_reg && have_val && n < out_max) {
            if (reg > 0xFFu || val > 0xFFu) return 0;
            out[n].reg = (uint8_t)(reg & 0xFFu);
            out[n].val = (uint8_t)(val & 0xFFu);
            n++;
        }
        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; break; }
        return 0;
    }
    if (out_len) *out_len = n;
    return 1;
}

static int parse_mac(const char *s, uint8_t out_mac[6])
{
    if (!s || !out_mac) return 0;
    int vals[6] = {0};
    int got = sscanf(s, "%x:%x:%x:%x:%x:%x",
                     &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]);
    if (got != 6) return 0;
    for (int i = 0; i < 6; ++i) {
        if (vals[i] < 0 || vals[i] > 255) return 0;
        out_mac[i] = (uint8_t)vals[i];
    }
    return 1;
}

static uint32_t parse_ipv4_be(const char *s, int *ok)
{
    if (ok) *ok = 0;
    if (!s) return 0;
    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    if (ok) *ok = 1;
    return ((a & 0xffu) << 24) | ((b & 0xffu) << 16) | ((c & 0xffu) << 8) | (d & 0xffu);
}

static int parse_network_json(const char *json, network_config_t *cfg)
{
    if (!json || !cfg) return 0;
    config_apply_defaults(cfg);

    char ip[32], netmask[32], gw[32], mac[32];
    ip[0] = netmask[0] = gw[0] = mac[0] = '\0';

    if (json_get_string(json, "ip", ip, sizeof(ip))) {
        int ok = 0;
        cfg->ip_be = parse_ipv4_be(ip, &ok);
        cfg->has_ip = ok;
    }
    if (json_get_string(json, "netmask", netmask, sizeof(netmask))) {
        int ok = 0;
        cfg->netmask_be = parse_ipv4_be(netmask, &ok);
        cfg->has_netmask = ok;
    }
    if (json_get_string(json, "gateway", gw, sizeof(gw))) {
        int ok = 0;
        cfg->gateway_be = parse_ipv4_be(gw, &ok);
        cfg->has_gateway = ok;
    }
    if (json_get_string(json, "mac", mac, sizeof(mac))) {
        cfg->has_mac = parse_mac(mac, cfg->mac);
    }

    return cfg->has_ip && cfg->has_netmask && cfg->has_gateway && cfg->has_mac;
}

static void i2c_cfg_clear(i2c_device_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->policy = I2C_POLICY_WHITELIST;
    cfg->autopoll_enabled = false;
    cfg->autopoll_cycle_delay_ms = 1000u;
}

static int parse_i2c_device_json(const char *json, i2c_device_config_t *cfg)
{
    if (!json || !cfg) return 0;
    i2c_cfg_clear(cfg);

    const char *p = find_key(json, "name");
    if (!p) return 0;
    {
        int ok = 0;
        (void)parse_json_string_inplace(p, cfg->name, sizeof(cfg->name), &ok);
        if (!ok || !cfg->name[0]) return 0;
    }

    p = find_key(json, "addr_7b");
    if (!p) return 0;
    {
        uint32_t v = 0;
        int ok = 0;
        (void)parse_json_u32_inplace(p, &v, &ok);
        if (!ok || v > 0x7Fu) return 0;
        cfg->addr_7b = (uint8_t)v;
    }

    p = find_key(json, "reg_count");
    if (!p) return 0;
    {
        uint32_t v = 0;
        int ok = 0;
        (void)parse_json_u32_inplace(p, &v, &ok);
        if (!ok || v == 0 || v > 256u) return 0;
        cfg->reg_count = (uint16_t)v;
    }

    p = find_key(json, "max_value_code");
    if (!p) return 0;
    {
        uint32_t v = 0;
        int ok = 0;
        (void)parse_json_u32_inplace(p, &v, &ok);
        if (!ok || v > 64u) return 0;
        cfg->max_value_code = (uint8_t)v;
    }

    p = find_key(json, "policy");
    if (!p) return 0;
    {
        char pol[16];
        int ok = 0;
        (void)parse_json_string_inplace(p, pol, sizeof(pol), &ok);
        if (!ok) return 0;
        if (strcasecmp(pol, "whitelist") == 0) cfg->policy = I2C_POLICY_WHITELIST;
        else if (strcasecmp(pol, "blacklist") == 0) cfg->policy = I2C_POLICY_BLACKLIST;
        else return 0;
    }

    int b = 0;
    if (find_key(json, "autopoll_enabled")) {
        if (!json_get_bool(json, "autopoll_enabled", &b)) return 0;
        cfg->autopoll_enabled = (b != 0);
    }

    p = find_key(json, "autopoll_reg_delay_ms");
    if (p) {
        uint32_t v = 0;
        int ok = 0;
        (void)parse_json_u32_inplace(p, &v, &ok);
        if (!ok) return 0;
        cfg->autopoll_reg_delay_ms = v;
    }
    p = find_key(json, "autopoll_cycle_delay_ms");
    if (p) {
        uint32_t v = 0;
        int ok = 0;
        (void)parse_json_u32_inplace(p, &v, &ok);
        if (!ok) return 0;
        cfg->autopoll_cycle_delay_ms = v;
    }

    p = find_key(json, "autopoll_regs");
    if (p) {
        size_t n = 0;
        if (!json_parse_num_array_u8(p, cfg->autopoll_regs, I2C_CFG_AUTOPOLL_REGS_MAX, &n)) return 0;
        cfg->autopoll_regs_len = n;
    }

    p = find_key(json, "whitelist");
    if (p) {
        size_t n = 0;
        if (!json_parse_rules_array(p, cfg->whitelist, I2C_CFG_RULES_MAX, &n)) return 0;
        cfg->whitelist_len = n;
    }
    p = find_key(json, "blacklist");
    if (p) {
        size_t n = 0;
        if (!json_parse_rules_array(p, cfg->blacklist, I2C_CFG_RULES_MAX, &n)) return 0;
        cfg->blacklist_len = n;
    }
    p = find_key(json, "settings");
    if (p) {
        size_t n = 0;
        if (!json_parse_rules_array(p, cfg->settings, I2C_CFG_SETTINGS_MAX, &n)) return 0;
        cfg->settings_len = n;
    }

    for (size_t i = 0; i < cfg->autopoll_regs_len; ++i) {
        if (cfg->autopoll_regs[i] >= cfg->reg_count) return 0;
    }
    for (size_t i = 0; i < cfg->whitelist_len; ++i) {
        if (cfg->whitelist[i].reg >= cfg->reg_count) return 0;
        if (cfg->whitelist[i].val > cfg->max_value_code) return 0;
    }
    for (size_t i = 0; i < cfg->blacklist_len; ++i) {
        if (cfg->blacklist[i].reg >= cfg->reg_count) return 0;
        if (cfg->blacklist[i].val > cfg->max_value_code) return 0;
    }
    for (size_t i = 0; i < cfg->settings_len; ++i) {
        if (cfg->settings[i].reg >= cfg->reg_count) return 0;
    }

    return 1;
}

static void config_task(void *arg)
{
    (void)arg;

    /* Give QSPI FatFs enough time to mount before we decide to fall back to embedded defaults. */
    for (int i = 0; i < 300; ++i) {
        if (qspi_fs_is_ready()) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    char dir_path[96], dir_path_fb[96];
    char net_cfg_path[128], net_cfg_path_fb[128];
    char i2c_dir_path[128], i2c_dir_path_fb[128];
    build_cfg_path(dir_path, sizeof(dir_path), dir_path_fb, sizeof(dir_path_fb), "");
    build_cfg_path(net_cfg_path, sizeof(net_cfg_path), net_cfg_path_fb, sizeof(net_cfg_path_fb), NETWORK_CFG_NAME);
    build_cfg_path(i2c_dir_path, sizeof(i2c_dir_path), i2c_dir_path_fb, sizeof(i2c_dir_path_fb), I2C_CFG_SUBDIR_NAME);

    if (!dir_path[0] || !dir_path_fb[0] || !net_cfg_path[0] || !net_cfg_path_fb[0] || !i2c_dir_path[0] || !i2c_dir_path_fb[0]) {
        xil_printf("CFG: path build failed\r\n");
        goto done;
    }

    if (qspi_fs_is_ready()) {
        (void)ensure_dir_exists(dir_path);
        if (!file_exists(net_cfg_path) && !file_exists(net_cfg_path_fb)) {
            if (!write_file_atomic(net_cfg_path, DEFAULT_NETWORK_JSON, DEFAULT_NETWORK_JSON_LEN)) {
                xil_printf("CFG: failed to write default %s\r\n", net_cfg_path);
            } else {
                xil_printf("CFG: wrote default %s\r\n", net_cfg_path);
            }
        }
        (void)ensure_dir_exists(i2c_dir_path);
        for (unsigned i = 0; i < DEFAULT_I2C_CONFIG_FILES_COUNT; ++i) {
            const default_json_file_t *d = &DEFAULT_I2C_CONFIG_FILES[i];
            char cfg_path[160], cfg_path_fb[160];
            char suf[96];
            snprintf(suf, sizeof(suf), "%s/%s", I2C_CFG_SUBDIR_NAME, d->file_name);
            build_cfg_path(cfg_path, sizeof(cfg_path), cfg_path_fb, sizeof(cfg_path_fb), suf);
            if (!cfg_path[0] || !cfg_path_fb[0]) continue;
            if (!file_exists(cfg_path) && !file_exists(cfg_path_fb)) {
                if (!write_file_atomic(cfg_path, d->json, d->json_len)) {
                    xil_printf("CFG: failed to write default %s\r\n", cfg_path);
                } else {
                    xil_printf("CFG: wrote default %s\r\n", cfg_path);
                }
            }
        }

        /* One-time migration: if legacy configs exist but primary is empty/missing, copy into flash:/config/. */
        if (!file_exists(net_cfg_path) && file_exists(net_cfg_path_fb)) {
            if (copy_file_atomic(net_cfg_path_fb, net_cfg_path, s_cfg_buf, sizeof(s_cfg_buf))) {
                xil_printf("CFG: migrated %s -> %s\r\n", net_cfg_path_fb, net_cfg_path);
            }
        }
        if (!dir_exists(i2c_dir_path) && dir_exists(i2c_dir_path_fb)) {
            (void)ensure_dir_exists(i2c_dir_path);
        }
        if (dir_exists(i2c_dir_path_fb)) {
            DIR d;
            FILINFO fno;
            if (f_opendir(&d, i2c_dir_path_fb) == FR_OK) {
                for (;;) {
                    if (f_readdir(&d, &fno) != FR_OK) break;
                    if (!fno.fname[0]) break;
                    if (fno.fattrib & AM_DIR) continue;
                    const char *fn = fno.fname;
                    size_t fl = strlen(fn);
                    if (fl < 6) continue;
                    if (strcasecmp(fn + fl - 5, ".json") != 0) continue;

                    char src_p[200], dst_p[200];
                    int sn = snprintf(src_p, sizeof(src_p), "%s/%s", i2c_dir_path_fb, fn);
                    int dn = snprintf(dst_p, sizeof(dst_p), "%s/%s", i2c_dir_path, fn);
                    if (sn <= 0 || dn <= 0) continue;
                    if ((size_t)sn >= sizeof(src_p) || (size_t)dn >= sizeof(dst_p)) continue;
                    if (file_exists(dst_p)) continue;
                    if (copy_file_atomic(src_p, dst_p, s_cfg_buf, sizeof(s_cfg_buf))) {
                        xil_printf("CFG: migrated %s -> %s\r\n", src_p, dst_p);
                    }
                }
                (void)f_closedir(&d);
            }
        }
    } else {
        xil_printf("CFG: QSPI not ready; using embedded defaults\r\n");
    }

    size_t blen = 0;
    int loaded = 0;
    if (qspi_fs_is_ready()) {
        if (read_file_first_available(net_cfg_path, net_cfg_path_fb, s_cfg_buf, sizeof(s_cfg_buf), &blen)) {
            loaded = parse_network_json(s_cfg_buf, &s_net_cfg);
        }
    }
    if (!loaded) {
        (void)parse_network_json(DEFAULT_NETWORK_JSON, &s_net_cfg);
    }

    s_i2c_cfg_count = 0;
    if (qspi_fs_is_ready()) {
        for (int pass = 0; pass < 2; ++pass) {
            const char *scan_dir = (pass == 0) ? i2c_dir_path : i2c_dir_path_fb;
            if (!scan_dir || !scan_dir[0] || !dir_exists(scan_dir)) continue;
            DIR d;
            FILINFO fno;
            if (f_opendir(&d, scan_dir) != FR_OK) continue;
            for (;;) {
                if (f_readdir(&d, &fno) != FR_OK) break;
                if (!fno.fname[0]) break;
                if (fno.fattrib & AM_DIR) continue;
                const char *fn = fno.fname;
                size_t fl = strlen(fn);
                if (fl < 6) continue;
                if (strcasecmp(fn + fl - 5, ".json") != 0) continue;
                if (s_i2c_cfg_count >= I2C_CFG_MAX_DEVICES) break;

                char pth[200];
                int pn = snprintf(pth, sizeof(pth), "%s/%s", scan_dir, fn);
                if (pn <= 0 || (size_t)pn >= sizeof(pth)) continue;
                if (!read_file_to_buf(pth, s_cfg_buf, sizeof(s_cfg_buf), &blen)) continue;
                i2c_device_config_t tmp;
                if (!parse_i2c_device_json(s_cfg_buf, &tmp)) continue;

                /* Avoid duplicates by device name (primary dir wins). */
                bool dup = false;
                for (size_t j = 0; j < s_i2c_cfg_count; ++j) {
                    if (strcasecmp(s_i2c_cfgs[j].name, tmp.name) == 0) { dup = true; break; }
                }
                if (dup) continue;

                strncpy(tmp.file_name, fn, sizeof(tmp.file_name) - 1);
                tmp.file_name[sizeof(tmp.file_name) - 1] = '\0';
                s_i2c_cfgs[s_i2c_cfg_count++] = tmp;
            }
            (void)f_closedir(&d);
        }
    }
    if (s_i2c_cfg_count == 0) {
        for (unsigned i = 0; i < DEFAULT_I2C_CONFIG_FILES_COUNT && s_i2c_cfg_count < I2C_CFG_MAX_DEVICES; ++i) {
            const default_json_file_t *d = &DEFAULT_I2C_CONFIG_FILES[i];
            i2c_device_config_t tmp;
            if (!parse_i2c_device_json(d->json, &tmp)) continue;
            strncpy(tmp.file_name, d->file_name, sizeof(tmp.file_name) - 1);
            tmp.file_name[sizeof(tmp.file_name) - 1] = '\0';
            s_i2c_cfgs[s_i2c_cfg_count++] = tmp;
        }
    }

done:
    s_ready = 1;
    vTaskDelete(NULL);
}

int start_config_store(void)
{
    if (s_task) return 1;
    s_ready = 0;
    memset(&s_net_cfg, 0, sizeof(s_net_cfg));
    memset(s_i2c_cfgs, 0, sizeof(s_i2c_cfgs));
    s_i2c_cfg_count = 0;
    BaseType_t rc = xTaskCreate(config_task, "cfg", CONFIG_TASK_STACK, NULL, CONFIG_TASK_PRIO, &s_task);
    return (rc == pdPASS) ? 1 : 0;
}

int config_store_is_ready(void)
{
    return s_ready != 0;
}

int config_store_wait_ready(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t deadline = pdMS_TO_TICKS(timeout_ms);
    while (!config_store_is_ready()) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((xTaskGetTickCount() - start) > deadline) return 0;
    }
    return 1;
}

int config_store_get_network(network_config_t *out)
{
    if (!out) return 0;
    *out = s_net_cfg;
    return config_store_is_ready();
}

int config_store_set_network(const network_config_t *cfg)
{
    if (!cfg) return 0;
    s_net_cfg = *cfg;
    return 1;
}

size_t config_store_get_i2c_device_count(void)
{
    return config_store_is_ready() ? s_i2c_cfg_count : 0u;
}

const i2c_device_config_t *config_store_get_i2c_devices(void)
{
    return config_store_is_ready() ? s_i2c_cfgs : NULL;
}

const i2c_device_config_t *config_store_find_i2c_device_by_name(const char *name)
{
    if (!config_store_is_ready() || !name || !name[0]) return NULL;
    for (size_t i = 0; i < s_i2c_cfg_count; ++i) {
        if (strcasecmp(s_i2c_cfgs[i].name, name) == 0) return &s_i2c_cfgs[i];
    }
    return NULL;
}

const i2c_device_config_t *config_store_find_i2c_device_by_addr(uint8_t addr_7b)
{
    if (!config_store_is_ready()) return NULL;
    for (size_t i = 0; i < s_i2c_cfg_count; ++i) {
        if (s_i2c_cfgs[i].addr_7b == (addr_7b & 0x7Fu)) return &s_i2c_cfgs[i];
    }
    return NULL;
}

int config_store_set_i2c_device(const i2c_device_config_t *cfg)
{
    if (!config_store_is_ready() || !cfg || !cfg->name[0]) return 0;
    for (size_t i = 0; i < s_i2c_cfg_count; ++i) {
        if (strcasecmp(s_i2c_cfgs[i].name, cfg->name) == 0) {
            char fn[I2C_CFG_FILE_NAME_MAX];
            strncpy(fn, s_i2c_cfgs[i].file_name, sizeof(fn) - 1);
            fn[sizeof(fn) - 1] = '\0';
            s_i2c_cfgs[i] = *cfg;
            strncpy(s_i2c_cfgs[i].file_name, fn, sizeof(s_i2c_cfgs[i].file_name) - 1);
            s_i2c_cfgs[i].file_name[sizeof(s_i2c_cfgs[i].file_name) - 1] = '\0';
            return 1;
        }
    }
    if (s_i2c_cfg_count < I2C_CFG_MAX_DEVICES) {
        s_i2c_cfgs[s_i2c_cfg_count++] = *cfg;
        return 1;
    }
    return 0;
}

static void ipv4_to_str(uint32_t be, char out[16])
{
    unsigned a = (be >> 24) & 0xffu;
    unsigned b = (be >> 16) & 0xffu;
    unsigned c = (be >> 8) & 0xffu;
    unsigned d = (be >> 0) & 0xffu;
    snprintf(out, 16, "%u.%u.%u.%u", a, b, c, d);
}

static void mac_to_str(const uint8_t mac[6], char out[18])
{
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int config_store_save_network(void)
{
    if (!config_store_is_ready()) return 0;

    char dir_path[96], dir_path_fb[96];
    char cfg_path[128], cfg_path_fb[128];
    build_cfg_path(dir_path, sizeof(dir_path), dir_path_fb, sizeof(dir_path_fb), "");
    build_cfg_path(cfg_path, sizeof(cfg_path), cfg_path_fb, sizeof(cfg_path_fb), NETWORK_CFG_NAME);
    if (!dir_path[0] || !dir_path_fb[0] || !cfg_path[0] || !cfg_path_fb[0]) return 0;

    if (!qspi_fs_is_ready()) return 0;
    if (!ensure_dir_exists(dir_path)) return 0;

    char ip[16], nm[16], gw[16], mac[18];
    ipv4_to_str(s_net_cfg.ip_be, ip);
    ipv4_to_str(s_net_cfg.netmask_be, nm);
    ipv4_to_str(s_net_cfg.gateway_be, gw);
    mac_to_str(s_net_cfg.mac, mac);

    char json[256];
    int n = snprintf(json, sizeof(json),
        "{\n"
        "  \"ipv4\": {\n"
        "    \"ip\": \"%s\",\n"
        "    \"netmask\": \"%s\",\n"
        "    \"gateway\": \"%s\"\n"
        "  },\n"
        "  \"mac\": \"%s\"\n"
        "}\n",
        ip, nm, gw, mac);
    if (n <= 0) return 0;
    size_t len = (size_t)n;
    if (len >= sizeof(json)) return 0;

    int ok = write_file_atomic(cfg_path, json, len) ? 1 : 0;
    if (ok && dir_exists(dir_path_fb)) {
        (void)ensure_dir_exists(dir_path_fb);
        (void)write_file_atomic(cfg_path_fb, json, len);
    }
    return ok;
}

int config_store_save_i2c_device(const i2c_device_config_t *cfg)
{
    if (!config_store_is_ready() || !cfg || !cfg->name[0]) return 0;

    if (!qspi_fs_is_ready()) return 0;

    char root_dir_path[96], root_dir_path_fb[96];
    char i2c_dir_path[128], i2c_dir_path_fb[128];
    build_cfg_path(root_dir_path, sizeof(root_dir_path), root_dir_path_fb, sizeof(root_dir_path_fb), "");
    build_cfg_path(i2c_dir_path, sizeof(i2c_dir_path), i2c_dir_path_fb, sizeof(i2c_dir_path_fb), I2C_CFG_SUBDIR_NAME);
    if (!root_dir_path[0] || !root_dir_path_fb[0] || !i2c_dir_path[0] || !i2c_dir_path_fb[0]) return 0;
    if (!ensure_dir_exists(root_dir_path)) return 0;
    if (!ensure_dir_exists(i2c_dir_path)) return 0;

    char *json = s_i2c_save_buf;
    const size_t json_cap = sizeof(s_i2c_save_buf);
    size_t pos = 0;
    const char *pol = (cfg->policy == I2C_POLICY_BLACKLIST) ? "blacklist" : "whitelist";
    int n = snprintf(json + pos, json_cap - pos,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"addr_7b\": %u,\n"
        "  \"reg_count\": %u,\n"
        "  \"max_value_code\": %u,\n"
        "  \"policy\": \"%s\",\n"
        "  \"autopoll_enabled\": %s,\n"
        "  \"autopoll_reg_delay_ms\": %u,\n"
        "  \"autopoll_cycle_delay_ms\": %u,\n"
        "  \"autopoll_regs\": [",
        cfg->name,
        (unsigned)cfg->addr_7b,
        (unsigned)cfg->reg_count,
        (unsigned)cfg->max_value_code,
        pol,
        cfg->autopoll_enabled ? "true" : "false",
        (unsigned)cfg->autopoll_reg_delay_ms,
        (unsigned)cfg->autopoll_cycle_delay_ms);
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= json_cap) return 0;

    for (size_t i = 0; i < cfg->autopoll_regs_len; ++i) {
        n = snprintf(json + pos, json_cap - pos, "%s%u",
                     (i == 0) ? "" : ", ",
                     (unsigned)cfg->autopoll_regs[i]);
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= json_cap) return 0;
    }
    n = snprintf(json + pos, json_cap - pos, "],\n  \"whitelist\": [\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= json_cap) return 0;

    for (size_t i = 0; i < cfg->whitelist_len; ++i) {
        n = snprintf(json + pos, json_cap - pos,
                     "    { \"reg\": %u, \"val\": %u }%s\n",
                     (unsigned)cfg->whitelist[i].reg,
                     (unsigned)cfg->whitelist[i].val,
                     (i + 1 == cfg->whitelist_len) ? "" : ",");
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= json_cap) return 0;
    }
    n = snprintf(json + pos, json_cap - pos, "  ],\n  \"blacklist\": [\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= json_cap) return 0;

    for (size_t i = 0; i < cfg->blacklist_len; ++i) {
        n = snprintf(json + pos, json_cap - pos,
                     "    { \"reg\": %u, \"val\": %u }%s\n",
                     (unsigned)cfg->blacklist[i].reg,
                     (unsigned)cfg->blacklist[i].val,
                     (i + 1 == cfg->blacklist_len) ? "" : ",");
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= json_cap) return 0;
    }
    n = snprintf(json + pos, json_cap - pos, "  ],\n  \"settings\": [\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= json_cap) return 0;

    for (size_t i = 0; i < cfg->settings_len; ++i) {
        n = snprintf(json + pos, json_cap - pos,
                     "    { \"reg\": %u, \"val\": %u }%s\n",
                     (unsigned)cfg->settings[i].reg,
                     (unsigned)cfg->settings[i].val,
                     (i + 1 == cfg->settings_len) ? "" : ",");
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= json_cap) return 0;
    }

    n = snprintf(json + pos, json_cap - pos, "  ]\n}\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= json_cap) return 0;

    char cfg_path[160], cfg_path_fb[160];
    char suf[96];
    const char *fn = (cfg->file_name[0] ? cfg->file_name : NULL);
    char tmp_fn[I2C_CFG_FILE_NAME_MAX];
    if (!fn) {
        snprintf(tmp_fn, sizeof(tmp_fn), "%s.json", cfg->name);
        fn = tmp_fn;
    }
    snprintf(suf, sizeof(suf), "%s/%s", I2C_CFG_SUBDIR_NAME, fn);
    build_cfg_path(cfg_path, sizeof(cfg_path), cfg_path_fb, sizeof(cfg_path_fb), suf);
    if (!cfg_path[0] || !cfg_path_fb[0]) return 0;

    int ok = write_file_atomic(cfg_path, json, pos) ? 1 : 0;
    if (ok && dir_exists(root_dir_path_fb)) {
        (void)ensure_dir_exists(root_dir_path_fb);
        (void)ensure_dir_exists(i2c_dir_path_fb);
        (void)write_file_atomic(cfg_path_fb, json, pos);
    }
    return ok;
}
