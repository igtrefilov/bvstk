/* Exercise the real parser and transport with deterministic short I/O and
 * timeout injection. No socket or board is opened by this test. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/apps/freertos/services/http/http_server.c"

static const char *input;
static char output[4096];
static size_t used;
static int select_timeout, would_block, selects, sends;
static unsigned timeout_seen;
static uint32_t now;
uint32_t sys_now(void) { return now++; }
sys_thread_t sys_thread_new(const char *n, void (*fn)(void *), void *a, int s, int p)
{ (void)n; (void)fn; (void)a; (void)s; (void)p; return NULL; }
void vTaskDelete(void *t) { (void)t; }
int xil_printf(const char *fmt, ...) { (void)fmt; return 0; }
int lwip_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    (void)n; (void)r; (void)w; (void)e; selects++;
    timeout_seen = (unsigned)t->tv_sec * 1000U + (unsigned)t->tv_usec / 1000U;
    return select_timeout ? 0 : 1;
}
int lwip_recv(int fd, void *p, size_t n, int flags)
{
    (void)fd; assert(flags == MSG_DONTWAIT);
    size_t left = strlen(input); if (n > left) n = left; if (n > 7U) n = 7U;
    memcpy(p, input, n); input += n; return (int)n;
}
int lwip_send(int fd, const void *p, size_t n, int flags)
{
    (void)fd; assert(flags == MSG_DONTWAIT); sends++;
    if (would_block) { would_block = 0; errno = EWOULDBLOCK; return -1; }
    if (n > 11U) n = 11U;
    assert(used + n < sizeof(output)); memcpy(output + used, p, n); used += n; output[used] = 0;
    return (int)n;
}
static void reset(const char *wire)
{
    input = wire; used = 0; output[0] = 0;
    select_timeout = would_block = selects = sends = 0; now = 0; s_failed_fd = -1;
}
static void bad_request(const char *headers)
{
    char req[1024]; snprintf(req, sizeof(req), "PUT /api/ui/file HTTP/1.1\r\n%s\r\n", headers);
    reset(req); handle_client(3); assert(strstr(output, "HTTP/1.0 400 "));
}
int main(void)
{
    reset(""); would_block = 1;
    const char *message = "A response split into several short socket writes.";
    assert(http_server_write(3, message, strlen(message)) == (int)strlen(message));
    assert(!strcmp(output, message) && sends > 4);
    reset(""); select_timeout = 1;
    assert(http_server_write(3, message, strlen(message)) == -1);
    assert(http_server_write(3, message, strlen(message)) == -1);
    assert(selects == 1 && sends == 0); /* failure latched across response fragments */
    reset(""); select_timeout = 1; handle_client(3);
    assert(selects == 1 && timeout_seen == HTTP_HEADER_IDLE_MS);
    reset("GET /api/version HTTP/1.1\r\nHost: test\r\n\r\n"); handle_client(3);
    assert(strstr(output, "HTTP/1.0 404 ")); /* no route override in this fixture */
    bad_request("Content-Length: -1\r\n");
    bad_request("Content-Length: 3x\r\n");
    bad_request("Content-Length: 18446744073709551616\r\n");
    bad_request("Content-Length: 1\r\nContent-Length: 1\r\n");
    bad_request("Transfer-Encoding: gzip\r\n");
    bad_request("Transfer-Encoding: chunked\r\nContent-Length: 1\r\n");
    char b[64];
    reset("abcdef\r\n"); assert(read_line(3, b, 4) == -1);
    reset("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n");
    s_read_idle_ms = HTTP_IO_IDLE_MS; /* headers have been consumed */
    http_body_t body = {.fd=3, .chunked=true};
    assert(http_body_read_impl(&body, b, sizeof(b)) == 4 && !memcmp(b, "Wiki", 4));
    assert(http_body_read_impl(&body, b, sizeof(b)) == 5 && !memcmp(b, "pedia", 5));
    assert(http_body_read_impl(&body, b, sizeof(b)) == 0);
    reset("10000000000000000\r\n"); body = (http_body_t){.fd=3,.chunked=true};
    assert(http_body_read_impl(&body, b, sizeof(b)) == -1);
    reset("\r\n"); body = (http_body_t){.fd=3,.chunked=true};
    assert(http_body_read_impl(&body, b, sizeof(b)) == -1);
    reset(""); select_timeout = 1; body = (http_body_t){.fd=3,.remaining=1};
    assert(http_body_read_impl(&body, b, sizeof(b)) == -1);
    assert(timeout_seen == HTTP_IO_IDLE_MS);
    puts("HTTP transport tests passed: partial sends, idle deadlines, lengths, chunk framing");
    return 0;
}
