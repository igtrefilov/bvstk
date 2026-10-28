#include "apps/freertos/console/bvstk_storage_freertos.h"
#include <string.h>
#include <stdio.h>
void bvstk_storage_freertos_cwd(const console_session_t *session, char out[BVSTK_FS_PATH_CAPACITY])
{
    const fs_device_info_t *device = fs_device_default();
    for (int i = 0; session && i < fs_device_count(); ++i)
        if (fs_device_at(i)->ctx == session->fs_ctx) { device = fs_device_at(i); break; }
    const char *relative = session ? session->cwd : "";
    const char *root = session ? console_session_get_root(session) : "";
    if (*root && !strncmp(relative, root, strlen(root))) relative += strlen(root);
    while (*relative == '/') ++relative;
    snprintf(out, BVSTK_FS_PATH_CAPACITY, "%s:/%s", device ? device->name : "flash", relative);
}
