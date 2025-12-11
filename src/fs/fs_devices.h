#ifndef FS_DEVICES_H
#define FS_DEVICES_H

#include "fs_shared.h"

typedef struct {
    const char *name;
    const char *label;
    fs_shared_ctx_t *ctx;
} fs_device_info_t;

void fs_devices_init(void);
int fs_device_prepare(const fs_device_info_t *dev);
const fs_device_info_t *fs_device_by_name(const char *name);
const fs_device_info_t *fs_device_default(void);
int fs_device_count(void);
const fs_device_info_t *fs_device_at(int index);

#endif // FS_DEVICES_H
