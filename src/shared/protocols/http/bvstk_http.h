#ifndef BVSTK_SHARED_HTTP_H
#define BVSTK_SHARED_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *method;
    const char *path;
    const char *version;
    bool chunked;
    bool has_content_length;
    uint64_t content_length;
    const char *content_type;
} http_request_t;

typedef int (*http_read_cb)(void *user, void *buf, size_t len);

typedef struct {
    int fd;
    http_read_cb read_body;
    void *read_user;
} http_conn_t;

int http_handle_request(const http_request_t *req, http_conn_t *conn);

#endif /* BVSTK_SHARED_HTTP_H */
