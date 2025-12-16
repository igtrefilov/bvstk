#ifndef BVSTK_HTTP_SERVER_H
#define BVSTK_HTTP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *method;   /* "GET", "PUT", ... (points into internal buffer) */
    const char *path;     /* "/..." (points into internal buffer) */
    const char *version;  /* "HTTP/1.0" (points into internal buffer) */

    bool chunked;
    bool has_content_length;
    uint64_t content_length;

    const char *content_type; /* optional, points into internal buffer */
} http_request_t;

typedef int (*http_read_cb)(void *user, void *buf, size_t len);

typedef struct {
    int fd;
    http_read_cb read_body;
    void *read_user;
} http_conn_t;

/*
 * User hook: return 1 if handled (you wrote the response), 0 if not handled.
 * Default weak implementation lives in http_server.c; override by defining this
 * function in any other compilation unit.
 */
int http_handle_request(const http_request_t *req, http_conn_t *conn);

void start_http_server(void);
uint16_t http_server_port(void);

#endif /* BVSTK_HTTP_SERVER_H */
