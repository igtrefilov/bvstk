#ifndef BVSTK_FREERTOS_HTTP_SERVER_H
#define BVSTK_FREERTOS_HTTP_SERVER_H

#include <stdint.h>

#include "shared/protocols/http/bvstk_http.h"

void start_http_server(void);
int http_server_startup_done(void);
uint16_t http_server_port(void);
/* Complete write with an idle deadline; -1 after timeout or connection loss. */
int http_server_write(int fd, const void *data, size_t length);

#endif /* BVSTK_FREERTOS_HTTP_SERVER_H */
