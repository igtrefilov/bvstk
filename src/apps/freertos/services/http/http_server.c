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

#include "protocols/http/bvstk_http_parser.h"

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
