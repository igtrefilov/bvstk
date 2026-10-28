#include "apps/freertos/services/http/http_ui.h"
int http_ui_lock(const fs_shared_ctx_t *ctx)
{
    return ctx && (!ctx->mutex || !*ctx->mutex || xSemaphoreTake(*ctx->mutex, pdMS_TO_TICKS(2000)) == pdTRUE);
}
void http_ui_unlock(const fs_shared_ctx_t *ctx)
{
    if (ctx && ctx->mutex && *ctx->mutex) xSemaphoreGive(*ctx->mutex);
}
