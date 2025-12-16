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

#define CONFIG_DIR_NAME "configs"
#define NETWORK_CFG_NAME "network.json"
#define AXP15060_CFG_NAME "axp15060.json"

#define CONFIG_TASK_STACK 2048
#define CONFIG_TASK_PRIO (tskIDLE_PRIORITY + 3)

static TaskHandle_t s_task = NULL;
static volatile int s_ready = 0;
static network_config_t s_net_cfg;
static axp15060_config_t s_axp_cfg;
static char s_cfg_buf[4096];

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
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    p = skip_ws(p);
    if (!p || *p != ':') return NULL;
    p++;
    return skip_ws(p);
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

static int json_parse_rules_array(const char *p, axp_rule_entry_t *out, size_t out_max, size_t *out_len)
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

static void axp_apply_defaults(axp15060_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->policy = AXP_POLICY_WHITELIST;
    cfg->autopoll_enabled = true;
    cfg->autopoll_reg_delay_ms = 5u;
    cfg->autopoll_cycle_delay_ms = 200u;
    cfg->autopoll_regs_len = 5u;
    cfg->autopoll_regs[0] = 0u;
    cfg->autopoll_regs[1] = 0x31u;
    cfg->autopoll_regs[2] = 0x32u;
    cfg->autopoll_regs[3] = 0x48u;
    cfg->autopoll_regs[4] = 0x49u;
    cfg->whitelist_len = 4u;
    cfg->whitelist[0] = (axp_rule_entry_t){ .reg = 0x13u, .val = 16u };
    cfg->whitelist[1] = (axp_rule_entry_t){ .reg = 0x13u, .val = 17u };
    cfg->whitelist[2] = (axp_rule_entry_t){ .reg = 0x13u, .val = 18u };
    cfg->whitelist[3] = (axp_rule_entry_t){ .reg = 0x13u, .val = 19u };
    cfg->blacklist_len = 0u;
}

static int parse_axp15060_json(const char *json, axp15060_config_t *cfg)
{
    if (!json || !cfg) return 0;
    axp_apply_defaults(cfg);

    const char *p = find_key(json, "policy");
    if (p) {
        char pol[16];
        int ok = 0;
        (void)parse_json_string_inplace(p, pol, sizeof(pol), &ok);
        if (!ok) return 0;
        if (strcasecmp(pol, "whitelist") == 0) cfg->policy = AXP_POLICY_WHITELIST;
        else if (strcasecmp(pol, "blacklist") == 0) cfg->policy = AXP_POLICY_BLACKLIST;
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
        if (!json_parse_num_array_u8(p, cfg->autopoll_regs, AXP15060_AUTOPOLL_REGS_MAX, &n)) return 0;
        cfg->autopoll_regs_len = n;
    }

    p = find_key(json, "whitelist");
    if (p) {
        size_t n = 0;
        if (!json_parse_rules_array(p, cfg->whitelist, AXP15060_RULES_MAX, &n)) return 0;
        cfg->whitelist_len = n;
    }
    p = find_key(json, "blacklist");
    if (p) {
        size_t n = 0;
        if (!json_parse_rules_array(p, cfg->blacklist, AXP15060_RULES_MAX, &n)) return 0;
        cfg->blacklist_len = n;
    }

    return 1;
}

static void config_task(void *arg)
{
    (void)arg;

    for (int i = 0; i < 50; ++i) {
        if (qspi_fs_is_ready()) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    char dir_path[96];
    char net_cfg_path[128];
    char axp_cfg_path[128];
    build_flash_path(dir_path, sizeof(dir_path), CONFIG_DIR_NAME);
    build_flash_path(net_cfg_path, sizeof(net_cfg_path), CONFIG_DIR_NAME "/" NETWORK_CFG_NAME);
    build_flash_path(axp_cfg_path, sizeof(axp_cfg_path), CONFIG_DIR_NAME "/" AXP15060_CFG_NAME);

    if (!dir_path[0] || !net_cfg_path[0] || !axp_cfg_path[0]) {
        xil_printf("CFG: path build failed\r\n");
        goto done;
    }

    if (qspi_fs_is_ready()) {
        (void)ensure_dir_exists(dir_path);
        if (!file_exists(net_cfg_path)) {
            if (!write_file_atomic(net_cfg_path, DEFAULT_NETWORK_JSON, DEFAULT_NETWORK_JSON_LEN)) {
                xil_printf("CFG: failed to write default %s\r\n", net_cfg_path);
            } else {
                xil_printf("CFG: wrote default %s\r\n", net_cfg_path);
            }
        }
        if (!file_exists(axp_cfg_path)) {
            if (!write_file_atomic(axp_cfg_path, DEFAULT_AXP15060_JSON, DEFAULT_AXP15060_JSON_LEN)) {
                xil_printf("CFG: failed to write default %s\r\n", axp_cfg_path);
            } else {
                xil_printf("CFG: wrote default %s\r\n", axp_cfg_path);
            }
        }
    } else {
        xil_printf("CFG: QSPI not ready; using embedded defaults\r\n");
    }

    size_t blen = 0;
    int loaded = 0;
    if (qspi_fs_is_ready() && file_exists(net_cfg_path) && read_file_to_buf(net_cfg_path, s_cfg_buf, sizeof(s_cfg_buf), &blen)) {
        loaded = parse_network_json(s_cfg_buf, &s_net_cfg);
    }
    if (!loaded) {
        (void)parse_network_json(DEFAULT_NETWORK_JSON, &s_net_cfg);
    }

    loaded = 0;
    if (qspi_fs_is_ready() && file_exists(axp_cfg_path) && read_file_to_buf(axp_cfg_path, s_cfg_buf, sizeof(s_cfg_buf), &blen)) {
        loaded = parse_axp15060_json(s_cfg_buf, &s_axp_cfg);
    }
    if (!loaded) {
        if (!parse_axp15060_json(DEFAULT_AXP15060_JSON, &s_axp_cfg)) {
            axp_apply_defaults(&s_axp_cfg);
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

int config_store_get_axp15060(axp15060_config_t *out)
{
    if (!out) return 0;
    *out = s_axp_cfg;
    return config_store_is_ready();
}

const axp15060_config_t *config_store_peek_axp15060(void)
{
    if (!config_store_is_ready()) return NULL;
    return &s_axp_cfg;
}

int config_store_set_axp15060(const axp15060_config_t *cfg)
{
    if (!cfg) return 0;
    s_axp_cfg = *cfg;
    return 1;
}

int config_store_set_axp15060_defaults(void)
{
    if (!parse_axp15060_json(DEFAULT_AXP15060_JSON, &s_axp_cfg)) {
        axp_apply_defaults(&s_axp_cfg);
    }
    return 1;
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

    char dir_path[96];
    char cfg_path[128];
    build_flash_path(dir_path, sizeof(dir_path), CONFIG_DIR_NAME);
    build_flash_path(cfg_path, sizeof(cfg_path), CONFIG_DIR_NAME "/" NETWORK_CFG_NAME);
    if (!dir_path[0] || !cfg_path[0]) return 0;

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

    return write_file_atomic(cfg_path, json, len) ? 1 : 0;
}

int config_store_save_axp15060(void)
{
    if (!config_store_is_ready()) return 0;

    char dir_path[96];
    char cfg_path[128];
    build_flash_path(dir_path, sizeof(dir_path), CONFIG_DIR_NAME);
    build_flash_path(cfg_path, sizeof(cfg_path), CONFIG_DIR_NAME "/" AXP15060_CFG_NAME);
    if (!dir_path[0] || !cfg_path[0]) return 0;

    if (!qspi_fs_is_ready()) return 0;
    if (!ensure_dir_exists(dir_path)) return 0;

    char json[8192];
    size_t pos = 0;
    const char *pol = (s_axp_cfg.policy == AXP_POLICY_BLACKLIST) ? "blacklist" : "whitelist";
    int n = snprintf(json + pos, sizeof(json) - pos,
        "{\n"
        "  \"policy\": \"%s\",\n"
        "  \"autopoll_enabled\": %s,\n"
        "  \"autopoll_reg_delay_ms\": %u,\n"
        "  \"autopoll_cycle_delay_ms\": %u,\n"
        "  \"autopoll_regs\": [",
        pol,
        s_axp_cfg.autopoll_enabled ? "true" : "false",
        (unsigned)s_axp_cfg.autopoll_reg_delay_ms,
        (unsigned)s_axp_cfg.autopoll_cycle_delay_ms);
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= sizeof(json)) return 0;

    for (size_t i = 0; i < s_axp_cfg.autopoll_regs_len; ++i) {
        n = snprintf(json + pos, sizeof(json) - pos, "%s%u",
                     (i == 0) ? "" : ", ",
                     (unsigned)s_axp_cfg.autopoll_regs[i]);
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= sizeof(json)) return 0;
    }
    n = snprintf(json + pos, sizeof(json) - pos, "],\n  \"whitelist\": [\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= sizeof(json)) return 0;

    for (size_t i = 0; i < s_axp_cfg.whitelist_len; ++i) {
        n = snprintf(json + pos, sizeof(json) - pos,
                     "    { \"reg\": %u, \"val\": %u }%s\n",
                     (unsigned)s_axp_cfg.whitelist[i].reg,
                     (unsigned)s_axp_cfg.whitelist[i].val,
                     (i + 1 == s_axp_cfg.whitelist_len) ? "" : ",");
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= sizeof(json)) return 0;
    }
    n = snprintf(json + pos, sizeof(json) - pos, "  ],\n  \"blacklist\": [\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= sizeof(json)) return 0;

    for (size_t i = 0; i < s_axp_cfg.blacklist_len; ++i) {
        n = snprintf(json + pos, sizeof(json) - pos,
                     "    { \"reg\": %u, \"val\": %u }%s\n",
                     (unsigned)s_axp_cfg.blacklist[i].reg,
                     (unsigned)s_axp_cfg.blacklist[i].val,
                     (i + 1 == s_axp_cfg.blacklist_len) ? "" : ",");
        if (n <= 0) return 0;
        pos += (size_t)n;
        if (pos >= sizeof(json)) return 0;
    }
    n = snprintf(json + pos, sizeof(json) - pos, "  ]\n}\n");
    if (n <= 0) return 0;
    pos += (size_t)n;
    if (pos >= sizeof(json)) return 0;

    return write_file_atomic(cfg_path, json, pos) ? 1 : 0;
}
