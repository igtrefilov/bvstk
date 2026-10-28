#ifndef BVSTK_FREERTOS_HTTP_UI_H
#define BVSTK_FREERTOS_HTTP_UI_H
#include "protocols/http/bvstk_http_ui.h"
#include "apps/freertos/storage/fs/fs_devices.h"
int http_ui_lock(const fs_shared_ctx_t *);
void http_ui_unlock(const fs_shared_ctx_t *);
#endif
