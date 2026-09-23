#ifndef BVSTK_HTTP_UI_H
#define BVSTK_HTTP_UI_H
#include "shared/protocols/http/bvstk_http.h"
#include "shared/base/bvstk_status.h"
#include "apps/freertos/storage/fs/fs_devices.h"
#define HTTP_UI_JSON_CAP 65537U
int http_ui_handle(const http_request_t *, http_conn_t *);
int http_ui_bus(const http_request_t *, http_conn_t *);
int http_ui_files(const http_request_t *, http_conn_t *);
int http_ui_observe(const http_request_t *, http_conn_t *);
int http_ui_send(int fd, const void *data, size_t size);
void http_ui_text(int fd, const char *text);
void http_ui_printf(int fd, const char *format, ...);
void http_ui_string(int fd, const char *text);
void http_ui_header(int fd, const char *type, long long length);
void http_ui_error(int fd, int code, const char *message);
void http_ui_status(int fd, bvstk_status_t status);
int http_ui_body(const http_request_t *, http_conn_t *, char *, size_t);
int http_ui_param(const http_request_t *, http_conn_t *, const char *, char *, size_t);
int http_ui_lock(const fs_shared_ctx_t *);
void http_ui_unlock(const fs_shared_ctx_t *);
#endif
