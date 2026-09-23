#include "apps/freertos/services/http/http_server.h"

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

enum { HTTP_PORT_DEFAULT = 80 };
/* Bounded JSON validation and FAT directory operations need more than the
 * original telemetry-only handler. Large bodies are allocated on the heap. */
enum { HTTP_THREAD_STACK = 4096 };
enum { HTTP_IO_IDLE_MS = 30000, HTTP_HEADER_IDLE_MS = 5000 };

static uint16_t s_port = HTTP_PORT_DEFAULT;
static volatile int s_startup_done;
/* The server handles one client at a time. Once a connection fails, later
 * fragments must fail immediately instead of waiting once per JSON field. */
static int s_failed_fd = -1;
static unsigned s_read_idle_ms = HTTP_HEADER_IDLE_MS;

static int socket_wait(int fd, int writing, unsigned ms)
{
    fd_set set;
    struct timeval timeout = {(long)(ms / 1000U), (long)(ms % 1000U) * 1000L};
    if (s_failed_fd == fd) return 0;
    FD_ZERO(&set); FD_SET(fd, &set);
    int n = lwip_select(fd + 1, writing ? NULL : &set, writing ? &set : NULL, NULL, &timeout);
    if (n <= 0) s_failed_fd = fd;
    return n > 0;
}

static int socket_read(int fd, void *data, size_t length)
{
    if (!socket_wait(fd, 0, s_read_idle_ms)) return -1;
    int n = lwip_recv(fd, data, length, MSG_DONTWAIT);
    if (n <= 0) s_failed_fd = fd;
    return n;
}

int http_server_write(int fd, const void *data, size_t length)
{
    const char *p = data;
    size_t sent = 0;
    uint32_t progress = sys_now();
    while (sent < length) {
        uint32_t elapsed = sys_now() - progress;
        if (elapsed >= HTTP_IO_IDLE_MS || !socket_wait(fd, 1, HTTP_IO_IDLE_MS - elapsed)) goto fail;
        size_t chunk = length - sent;
        if (chunk > 2048U) chunk = 2048U;
        int n = lwip_send(fd, p + sent, chunk, MSG_DONTWAIT);
        if (n < 0 && (errno == EWOULDBLOCK || errno == EINTR)) continue;
        if (n <= 0) goto fail;
        sent += (size_t)n; progress = sys_now();
    }
    return (int)sent;
fail:
    s_failed_fd = fd;
    return -1;
}

int http_server_startup_done(void)
{
    return s_startup_done != 0;
}

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
    (void)http_server_write(fd, s, strlen(s));
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
    if (n > 0) http_server_write(fd, hdr, (size_t)n);
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
        int r = socket_read(fd, p + got, len - got);
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
        int r = socket_read(fd, &c, 1);
        if (r <= 0 || !c) return -1;
        if (c == '\n') {
            out[n] = '\0';
            if (n > 0 && out[n - 1] == '\r') out[n - 1] = '\0';
            return (int)n;
        }
        out[n++] = c;
    }
    out[out_sz - 1] = '\0';
    return -1; /* Do not parse a truncated request/header as another line. */
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
        int r = socket_read(b->fd, buf, want);
        if (r <= 0) return -1;
        b->remaining -= (uint64_t)r;
        return r;
    }

    if (b->chunk_done) return 0;
    if (b->chunk_remaining == 0) {
        char line[64];
        if (read_line(b->fd, line, sizeof(line)) < 0) return -1;
        uint64_t sz = 0;
        for (char *p = line; *p; ++p) {
            if (*p == ';') { *p = '\0'; break; }
        }
        if (!line[0]) return -1;
        for (char *p = line; *p; ++p) {
            if (!is_hex_digit(*p) || sz > (UINT64_MAX - (unsigned)hex_val(*p)) / 16U) return -1;
            sz = sz * 16U + (unsigned)hex_val(*p);
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
    int r = socket_read(b->fd, buf, want);
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
    s_failed_fd = -1;
    s_read_idle_ms = HTTP_HEADER_IDLE_MS;
    char reqline[1024];
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
            if (req.has_content_length || !isdigit((unsigned char)*p)) {
                http_reply_simple(fd, 400, "Bad Request", "invalid content length\r\n"); return;
            }
            for (; isdigit((unsigned char)*p); ++p) {
                unsigned digit = (unsigned)(*p - '0');
                if (req.content_length > (UINT64_MAX - digit) / 10U) {
                    http_reply_simple(fd, 400, "Bad Request", "content length overflow\r\n"); return;
                }
                req.content_length = req.content_length * 10U + digit;
            }
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) { http_reply_simple(fd, 400, "Bad Request", "invalid content length\r\n"); return; }
            req.has_content_length = true;
        } else if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            const char *p = line + 18;
            while (*p && isspace((unsigned char)*p)) p++;
            if (req.chunked || strcasecmp(p, "chunked")) {
                http_reply_simple(fd, 400, "Bad Request", "unsupported transfer encoding\r\n"); return;
            }
            req.chunked = true;
        } else if (strncasecmp(line, "Content-Type:", 13) == 0) {
            const char *p = line + 13;
            while (*p && isspace((unsigned char)*p)) p++;
            strncpy(content_type_buf, p, sizeof(content_type_buf) - 1);
            content_type_buf[sizeof(content_type_buf) - 1] = '\0';
            req.content_type = content_type_buf;
        }
    }

    if (req.chunked && req.has_content_length) {
        http_reply_simple(fd, 400, "Bad Request", "ambiguous body length\r\n"); return;
    }
    s_read_idle_ms = HTTP_IO_IDLE_MS;

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
    if (s < 0) { s_startup_done = 1; vTaskDelete(NULL); return; }
    int opt = 1;
    (void)lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (lwip_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lwip_close(s);
        s_startup_done = 1;
        vTaskDelete(NULL);
        return;
    }
    lwip_listen(s, 6);
    xil_printf("HTTP: listening on %u\r\n", (unsigned)s_port);
    s_startup_done = 1;

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
    sys_thread_t thread;
    s_startup_done = 0;
    thread = sys_thread_new("http", http_server_thread, 0, HTTP_THREAD_STACK, tskIDLE_PRIORITY + 1);
    if (thread == NULL) s_startup_done = 1;
}
