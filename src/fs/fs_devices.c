#include "fs_devices.h"

#include "../sd_card/sd_card.h"
#include "../qspi_fs/qspi_fs.h"

#include "xstatus.h"
#include "task.h"

#include <string.h>
#include <strings.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static fs_device_info_t s_devices[] = {
    {"sd", "SD", NULL},
    {"flash", "FLASH", NULL},
};

void fs_devices_init(void)
{
    s_devices[0].ctx = sd_card_get_context();
    s_devices[1].ctx = qspi_fs_get_context();
}

int fs_device_prepare(const fs_device_info_t *dev)
{
    if (!dev || !dev->ctx) return XST_FAILURE;
    if (fs_shared_is_ready(dev->ctx)) return XST_SUCCESS;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (fs_shared_mount(dev->ctx, dev->label) == XST_SUCCESS) return XST_SUCCESS;
        if (fs_shared_is_ready(dev->ctx)) return XST_SUCCESS;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return fs_shared_is_ready(dev->ctx) ? XST_SUCCESS : XST_FAILURE;
}

const fs_device_info_t *fs_device_by_name(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < ARRAY_SIZE(s_devices); ++i) {
        if (strcasecmp(name, s_devices[i].name) == 0 && s_devices[i].ctx) {
            return &s_devices[i];
        }
    }
    return NULL;
}

const fs_device_info_t *fs_device_default(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(s_devices); ++i) {
        if (s_devices[i].ctx) {
            return &s_devices[i];
        }
    }
    return &s_devices[0];
}

int fs_device_count(void)
{
    return (int)ARRAY_SIZE(s_devices);
}

const fs_device_info_t *fs_device_at(int index)
{
    if (index < 0 || index >= (int)ARRAY_SIZE(s_devices)) return NULL;
    return &s_devices[index];
}

const fs_device_info_t *fs_device_for_path(const char *path)
{
    if (!path || path[0] == '\0' || path[1] != ':') return NULL;
    for (size_t i = 0; i < ARRAY_SIZE(s_devices); ++i) {
        const fs_shared_ctx_t *ctx = s_devices[i].ctx;
        if (!ctx || !ctx->root) continue;
        size_t root_len = strlen(ctx->root);
        if (root_len == 0) continue;
        if (strncasecmp(path, ctx->root, root_len) == 0) return &s_devices[i];
    }
    return NULL;
}
