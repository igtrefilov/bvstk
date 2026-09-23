#include "shared/protocols/http/bvstk_http_input.h"
#include <ctype.h>
#include <string.h>

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int decode(const char *p, const char *end, char *out, size_t cap)
{
    size_t n = 0;
    while (p < end) {
        unsigned char c = (unsigned char)*p++;
        if (c == '%') {
            if (end - p < 2 || hex(p[0]) < 0 || hex(p[1]) < 0) return 0;
            c = (unsigned char)((hex(p[0]) << 4) | hex(p[1])); p += 2;
        } else if (c == '+') c = ' ';
        if (c < 32U || c == 127U || n + 1 >= cap) return 0;
        out[n++] = (char)c;
    }
    if (!cap) return 0;
    out[n] = 0; return 1;
}
int bvstk_http_query(const char *url, const char *key, char *out, size_t cap)
{
    const char *p = strchr(url, '?');
    int found = 0;
    if (!cap) return -1;
    out[0] = 0;
    if (!p) return 0;
    ++p;
    while (*p) {
        const char *end = strchr(p, '&'), *eq;
        char name[40];
        if (!end) end = p + strlen(p);
        eq = memchr(p, '=', (size_t)(end - p));
        if (!eq || !decode(p, eq, name, sizeof(name))) return -1;
        if (!strcmp(name, key)) {
            if (found || !decode(eq + 1, end, out, cap)) return -1;
            found = 1;
        }
        p = *end ? end + 1 : end;
    }
    return found;
}
int bvstk_http_uint(const char *p, uint32_t max, uint32_t *out)
{
    uint32_t n = 0;
    if (!p || !*p) return 0;
    do {
        uint32_t d;
        if (*p < '0' || *p > '9') return 0;
        d = (uint32_t)(*p++ - '0');
        if (d > max || n > (max - d) / 10U) return 0;
        n = n * 10U + d;
    } while (*p);
    *out = n; return 1;
}
int bvstk_http_path(const char *path)
{
    const char *start, *p;
    if (!path || *path != '/' || strlen(path) > 120U) return 0;
    if (!path[1]) return 1;
    start = p = path + 1;
    for (;;) {
        unsigned char c = (unsigned char)*p;
        if (!c || c == '/') {
            size_t len = (size_t)(p - start);
            if (!len || len > 63U || (len == 1 && *start == '.') ||
                (len == 2 && !strncmp(start, "..", 2)) ||
                p[-1] == ' ' || p[-1] == '.') return 0;
            if (!c) return 1;
            start = p + 1;
        } else if (c < 32U || c == 127U || strchr(":\\*?\"<>|", c)) return 0;
        ++p;
    }
}
static void ws(const char **p) { while (strchr(" \t\r\n", **p) && **p) ++*p; }
static int string(const char **p, const char **start, size_t *len, int key)
{
    const char *s;
    if (*(*p)++ != '"') return 0;
    s = *p;
    while (**p && **p != '"') {
        unsigned char c = (unsigned char)*(*p)++;
        if (c < 32U || (key && c > 126U)) return 0;
        if (c == '\\') {
            if (key || !**p) return 0;
            c = (unsigned char)*(*p)++;
            if (c == 'u') {
                for (int i = 0; i < 4; ++i) {
                    if (!**p || hex(**p) < 0) return 0;
                    ++*p;
                }
            } else if (!strchr("\"\\/bfnrt", c)) return 0;
        }
    }
    if (**p != '"') return 0;
    if (start) *start = s;
    if (len) *len = (size_t)(*p - s);
    ++*p; return 1;
}
static int value(const char **p, unsigned depth, const char *const *allowed,
                 size_t count, uint32_t required)
{
    if (depth > 7U) return 0;
    ws(p);
    if (**p == '{') {
        const char *keys[24]; size_t sizes[24], n = 0;
        uint32_t seen = 0;
        ++*p; ws(p);
        if (**p == '}') { ++*p; return !required; }
        for (;;) {
            size_t len, k;
            const char *key;
            if (**p != '"' || !string(p, &key, &len, 1) || n == 24U) return 0;
            for (k = 0; k < n; ++k)
                if (sizes[k] == len && !memcmp(keys[k], key, len)) return 0;
            keys[n] = key; sizes[n++] = len;
            if (!depth) {
                for (k = 0; k < count; ++k)
                    if (strlen(allowed[k]) == len && !memcmp(allowed[k], key, len)) break;
                if (k == count) return 0;
                seen |= UINT32_C(1) << k;
            }
            ws(p); if (**p != ':') return 0; ++*p;
            if (!value(p, depth + 1U, allowed, count, 0)) return 0;
            ws(p);
            if (**p == '}') { ++*p; return depth || (seen & required) == required; }
            if (**p != ',') return 0;
            ++*p; ws(p);
        }
    }
    if (**p == '[') {
        ++*p; ws(p);
        if (**p == ']') { ++*p; return 1; }
        for (;;) {
            if (!value(p, depth + 1U, allowed, count, 0)) return 0;
            ws(p); if (**p == ']') { ++*p; return 1; }
            if (**p != ',') return 0;
            ++*p;
        }
    }
    if (**p == '"') return string(p, NULL, NULL, 0);
    if (!strncmp(*p, "true", 4)) { *p += 4; return 1; }
    if (!strncmp(*p, "false", 5)) { *p += 5; return 1; }
    if (**p >= '0' && **p <= '9') {
        if (**p == '0' && isdigit((unsigned char)(*p)[1])) return 0;
        while (isdigit((unsigned char)**p)) ++*p;
        return 1;
    }
    return 0;
}
int bvstk_http_json_object(const char *json, const char *const *keys, size_t count, uint32_t required)
{
    const char *p = json;
    if (!json || count > 32U) return 0;
    ws(&p);
    if (*p != '{' || !value(&p, 0, keys, count, required)) return 0;
    ws(&p); return !*p;
}
uint32_t bvstk_http_revision(const char *s)
{
    uint32_t h = UINT32_C(2166136261);
    while (*s) { h ^= (unsigned char)*s++; h *= UINT32_C(16777619); }
    return h;
}
