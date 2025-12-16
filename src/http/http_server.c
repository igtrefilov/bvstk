#include "http_server.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "task.h"
#include "xil_printf.h"

enum { HTTP_PORT_DEFAULT = 8000 };
enum { HTTP_THREAD_STACK = 2048 };

static uint16_t s_port = HTTP_PORT_DEFAULT;

uint16_t http_server_port(void)
{
    return s_port;
}

__attribute__((weak)) int http_handle_request(const http_request_t *req, http_conn_t *conn)
{
    (void)req;
    (void)conn;
    return 0;
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

static int sock_read_exact(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    while (got < len) {
        int r = lwip_read(fd, p + got, (int)(len - got));
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return (int)got;
}

static int read_line(int fd, char *out, size_t out_sz)
{
    if (!out || out_sz < 2) return -1;
    size_t n = 0;
    while (n + 1 < out_sz) {
        char c = 0;
        int r = lwip_read(fd, &c, 1);
        if (r <= 0) return -1;
        if (c == '\n') {
            out[n] = '\0';
            if (n > 0 && out[n - 1] == '\r') out[n - 1] = '\0';
            return (int)n;
        }
        out[n++] = c;
    }
    out[out_sz - 1] = '\0';
    return (int)n;
}

typedef struct {
    int fd;
    bool chunked;
    uint64_t remaining;
    uint64_t chunk_remaining;
    bool chunk_done;
} http_body_t;

static int http_body_read_impl(void *user, void *buf, size_t len)
{
    http_body_t *b = (http_body_t *)user;
    if (!b || !buf || len == 0) return -1;
    if (!b->chunked) {
        if (b->remaining == 0) return 0;
        size_t want = (b->remaining < (uint64_t)len) ? (size_t)b->remaining : len;
        int r = lwip_read(b->fd, buf, (int)want);
        if (r <= 0) return -1;
        b->remaining -= (uint64_t)r;
        return r;
    }

    if (b->chunk_done) return 0;
    if (b->chunk_remaining == 0) {
        char line[64];
        if (read_line(b->fd, line, sizeof(line)) < 0) return -1;
        unsigned long sz = 0;
        for (char *p = line; *p; ++p) {
            if (*p == ';') { *p = '\0'; break; }
        }
        for (char *p = line; *p; ++p) {
            if (!is_hex_digit(*p)) return -1;
            sz = (sz << 4) | (unsigned long)hex_val(*p);
        }
        b->chunk_remaining = (uint64_t)sz;
        if (b->chunk_remaining == 0) {
            while (1) {
                if (read_line(b->fd, line, sizeof(line)) < 0) return -1;
                if (line[0] == '\0') break;
            }
            b->chunk_done = true;
            return 0;
        }
    }

    size_t want = (b->chunk_remaining < (uint64_t)len) ? (size_t)b->chunk_remaining : len;
    int r = lwip_read(b->fd, buf, (int)want);
    if (r <= 0) return -1;
    b->chunk_remaining -= (uint64_t)r;
    if (b->chunk_remaining == 0) {
        char crlf[2];
        if (sock_read_exact(b->fd, crlf, 2) < 0) return -1;
        if (crlf[0] != '\r' || crlf[1] != '\n') return -1;
    }
    return r;
}

static void handle_client(int fd)
{
    char reqline[256];
    if (read_line(fd, reqline, sizeof(reqline)) <= 0) return;

    char *method = NULL;
    char *path = NULL;
    char *ver = NULL;
    {
        char *save = NULL;
        method = strtok_r(reqline, " \t", &save);
        path = strtok_r(NULL, " \t", &save);
        ver = strtok_r(NULL, " \t", &save);
    }
    if (!method || !path || !ver) {
        http_reply_simple(fd, 400, "Bad Request", "bad request line\r\n");
        return;
    }

    char content_type_buf[96];
    content_type_buf[0] = '\0';

    http_request_t req = {
        .method = method,
        .path = path,
        .version = ver,
        .chunked = false,
        .has_content_length = false,
        .content_length = 0,
        .content_type = NULL,
    };

    char line[256];
    while (1) {
        int n = read_line(fd, line, sizeof(line));
        if (n < 0) return;
        if (line[0] == '\0') break;

        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            const char *p = line + 15;
            while (*p && isspace((unsigned char)*p)) p++;
            req.content_length = (uint64_t)strtoull(p, NULL, 10);
            req.has_content_length = true;
        } else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            if (strstr(line, "chunked")) req.chunked = true;
        } else if (strncasecmp(line, "Content-Type:", 13) == 0) {
            const char *p = line + 13;
            while (*p && isspace((unsigned char)*p)) p++;
            strncpy(content_type_buf, p, sizeof(content_type_buf) - 1);
            content_type_buf[sizeof(content_type_buf) - 1] = '\0';
            req.content_type = content_type_buf;
        }
    }

    http_body_t body = {
        .fd = fd,
        .chunked = req.chunked,
        .remaining = req.has_content_length ? req.content_length : 0,
        .chunk_remaining = 0,
        .chunk_done = false,
    };

    http_conn_t conn = {
        .fd = fd,
        .read_body = http_body_read_impl,
        .read_user = &body,
    };

    if (http_handle_request(&req, &conn)) return;
    http_reply_simple(fd, 404, "Not Found", "not found\r\n");
}

static void http_server_thread(void *p)
{
    (void)p;
    int s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { vTaskDelete(NULL); return; }
    int opt = 1;
    (void)lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (lwip_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lwip_close(s);
        vTaskDelete(NULL);
        return;
    }
    lwip_listen(s, 1);
    xil_printf("HTTP: listening on %u\r\n", (unsigned)s_port);

    for (;;) {
        struct sockaddr_in remote;
        socklen_t rlen = sizeof(remote);
        int c = lwip_accept(s, (struct sockaddr *)&remote, &rlen);
        if (c < 0) continue;
        handle_client(c);
        lwip_close(c);
    }
}

void start_http_server(void)
{
    sys_thread_new("http", http_server_thread, 0, HTTP_THREAD_STACK, tskIDLE_PRIORITY + 1);
}
