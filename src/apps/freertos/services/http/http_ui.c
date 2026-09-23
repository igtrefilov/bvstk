#include "apps/freertos/services/http/http_ui.h"
#include "apps/freertos/services/http/http_server.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int http_ui_send(int fd, const void *data, size_t size)
{
    const char *p = data;
    while (size) {
        int n = http_server_write(fd, p, size);
        if (n <= 0) return 0;
        p += n; size -= (size_t)n;
    }
    return 1;
}
void http_ui_text(int fd, const char *s) { (void)http_ui_send(fd, s, strlen(s)); }
void http_ui_printf(int fd, const char *fmt, ...)
{
    char b[512]; va_list ap; int n;
    va_start(ap, fmt); n = vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    if (n > 0 && (size_t)n < sizeof(b)) (void)http_ui_send(fd, b, (size_t)n);
}
void http_ui_string(int fd, const char *s)
{
    const char *p = s, *start = s;
    http_ui_text(fd, "\"");
    for (; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < 32U || c == '"' || c == '\\') {
            (void)http_ui_send(fd, start, (size_t)(p - start));
            if (c < 32U) http_ui_printf(fd, "\\u%04x", (unsigned)c);
            else { char b[2] = {'\\', (char)c}; (void)http_ui_send(fd, b, 2); }
            start = p + 1;
        }
    }
    (void)http_ui_send(fd, start, (size_t)(p - start));
    http_ui_text(fd, "\"");
}
void http_ui_header(int fd, const char *type, long long length)
{
    http_ui_text(fd, "HTTP/1.0 200 OK\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n");
    http_ui_printf(fd, "Content-Type: %s\r\n", type);
    if (length >= 0) http_ui_printf(fd, "Content-Length: %llu\r\n", (unsigned long long)length);
    http_ui_text(fd, "\r\n");
}
void http_ui_error(int fd, int code, const char *message)
{
    http_ui_printf(fd, "HTTP/1.0 %d Error\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Type: application/json; charset=utf-8\r\n\r\n{\"ok\":false,\"error\":", code);
    http_ui_string(fd, message); http_ui_text(fd, "}\n");
}
void http_ui_status(int fd, bvstk_status_t st)
{
    if (st == BVSTK_OK) {
        http_ui_header(fd, "application/json", -1); http_ui_text(fd, "{\"ok\":true}\n");
    } else http_ui_error(fd, st == BVSTK_ERR_DENIED ? 403 :
        st == BVSTK_ERR_NOT_FOUND ? 404 : st == BVSTK_ERR_TIMEOUT ? 504 :
        st == BVSTK_ERR_BUSY ? 423 :
        st == BVSTK_ERR_NOT_READY ? 503 :
        st == BVSTK_ERR_RANGE || st == BVSTK_ERR_MALFORMED ? 400 : 500,
        bvstk_status_string(st));
}
int http_ui_body(const http_request_t *r, http_conn_t *c, char *out, size_t cap)
{
    size_t n = 0;
    if (!r->has_content_length && !r->chunked) { http_ui_error(c->fd, 411, "length required"); return 0; }
    if (r->has_content_length && r->content_length >= cap) { http_ui_error(c->fd, 413, "body too large"); return 0; }
    for (;;) {
        int got;
        char extra;
        if (n == cap - 1U) {
            got = c->read_body(c->read_user, &extra, 1);
            if (got != 0) { http_ui_error(c->fd, got < 0 ? 400 : 413, "body too large or incomplete"); return 0; }
            break;
        }
        got = c->read_body(c->read_user, out + n, cap - 1U - n);
        if (got < 0) { http_ui_error(c->fd, 400, "incomplete body"); return 0; }
        if (!got) break;
        if (memchr(out + n, 0, (size_t)got)) { http_ui_error(c->fd, 400, "NUL in JSON"); return 0; }
        n += (size_t)got;
    }
    out[n] = 0;
    if (!n) { http_ui_error(c->fd, 400, "empty body"); return 0; }
    return 1;
}
int http_ui_param(const http_request_t *r, http_conn_t *c, const char *key, char *out, size_t cap)
{
    if (bvstk_http_query(r->path, key, out, cap) != 1 || !out[0]) {
        http_ui_error(c->fd, 400, "missing or malformed query parameter"); return 0;
    }
    return 1;
}
int http_ui_lock(const fs_shared_ctx_t *ctx)
{
    return ctx && (!ctx->mutex || !*ctx->mutex || xSemaphoreTake(*ctx->mutex, pdMS_TO_TICKS(2000)) == pdTRUE);
}
void http_ui_unlock(const fs_shared_ctx_t *ctx)
{
    if (ctx && ctx->mutex && *ctx->mutex) xSemaphoreGive(*ctx->mutex);
}
int http_ui_handle(const http_request_t *r, http_conn_t *c)
{
    if (strncmp(r->path, "/api/ui/", 8)) return 0;
    if (strcmp(r->method, "GET") && strcmp(r->method, "PUT")) {
        http_ui_error(c->fd, 405, "only GET and PUT"); return 1;
    }
    if (http_ui_observe(r, c) || http_ui_bus(r, c) || http_ui_files(r, c)) return 1;
    http_ui_error(c->fd, 404, "unknown UI endpoint"); return 1;
}
