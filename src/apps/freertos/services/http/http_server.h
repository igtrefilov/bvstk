#ifndef BVSTK_FREERTOS_HTTP_SERVER_H
#define BVSTK_FREERTOS_HTTP_SERVER_H

#include <stdint.h>

#include "shared/protocols/http/bvstk_http.h"

void start_http_server(void);
uint16_t http_server_port(void);

#endif /* BVSTK_FREERTOS_HTTP_SERVER_H */
