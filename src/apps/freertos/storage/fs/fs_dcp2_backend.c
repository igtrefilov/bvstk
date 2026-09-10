#include "apps/freertos/storage/fs/fs_dcp2_backend.h"

#include <string.h>
#include "apps/freertos/storage/fs/fs_devices.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"

typedef struct { const fs_device_info_t *device; FIL file; } fs_dcp2_file_t;
typedef struct { const fs_device_info_t *device; DIR dir; } fs_dcp2_dir_t;

static bvstk_fs_result_t result(FRESULT value)
{
    switch (value) {
    case FR_OK: return BVSTK_FS_OK;
    case FR_NO_FILE: case FR_NO_PATH: return BVSTK_FS_NOT_FOUND;
    case FR_NOT_READY: case FR_NOT_ENABLED: case FR_NO_FILESYSTEM: return BVSTK_FS_NOT_READY;
    case FR_INVALID_NAME: case FR_INVALID_PARAMETER: return BVSTK_FS_INVALID;
    case FR_NOT_ENOUGH_CORE: return BVSTK_FS_BUSY;
    default: return BVSTK_FS_IO;
    }
}

static bool supported(const fs_device_info_t *device)
{
#if !BVSTK_PL_SD_CAN_FILESYSTEM
    if (!strcmp(device->name, "sd-pl")) return false;
#endif
    (void)device;
    return true;
}

static size_t volume_count(void *context)
{
    (void)context;
    return (size_t)fs_device_count();
}

static bvstk_fs_result_t volume(void *context, size_t index, bvstk_fs_volume_t *out)
{
    const fs_device_info_t *device = fs_device_at((int)index);
    (void)context;
    if (!device || strlen(device->name) >= sizeof(out->name)) return BVSTK_FS_RANGE;
    strcpy(out->name, device->name);
    out->state = !supported(device) ? 2 : (uint8_t)(device->ctx && fs_shared_is_ready(device->ctx));
    return BVSTK_FS_OK;
}

/* Preserve the BSP's existing OEM namespace; conversion is confined to FS API. */
static bvstk_fs_result_t utf8_to_native(const char *input, char *out, size_t capacity)
{
    size_t length = strlen(input);
#if FF_USE_LFN && FF_LFN_UNICODE == 2
    if (length >= capacity) return BVSTK_FS_RANGE;
    memcpy(out, input, length + 1);
#else
    size_t pos = 0, offset = 0;
    while (pos < length) {
        size_t used;
        uint32_t cp;
        WCHAR native;
        if (!bvstk_fs_utf8_decode((const uint8_t *)input + pos, length - pos, &used, &cp)) return BVSTK_FS_INVALID;
        pos += used;
        native = cp < 128 ? (WCHAR)cp : ff_uni2oem(cp, FF_CODE_PAGE);
        if (!native) return BVSTK_FS_RANGE;
        if (offset + (native > 255 ? 2U : 1U) >= capacity) return BVSTK_FS_RANGE;
        if (native > 255) out[offset++] = (char)(native >> 8);
        out[offset++] = (char)native;
    }
    out[offset] = '\0';
#endif
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t native_to_utf8(const char *input, char *out, size_t capacity)
{
#if FF_USE_LFN && FF_LFN_UNICODE == 2
    size_t length = strlen(input);
    if (length >= capacity) return BVSTK_FS_RANGE;
    memcpy(out, input, length + 1);
#else
    const uint8_t *p = (const uint8_t *)input;
    size_t offset = 0;
    while (*p) {
        uint32_t cp;
        WCHAR native = *p++;
        if (native < 128) cp = native;
        else {
            cp = ff_oem2uni(native, FF_CODE_PAGE);
            if (!cp && *p) {
                native = (WCHAR)((native << 8) | *p++);
                cp = ff_oem2uni(native, FF_CODE_PAGE);
            }
            if (!cp) return BVSTK_FS_IO;
        }
        if (offset + (cp < 128 ? 1U : cp < 2048 ? 2U : 3U) >= capacity) return BVSTK_FS_RANGE;
        if (cp < 128) out[offset++] = (char)cp;
        else if (cp < 2048) {
            out[offset++] = (char)(0xC0 | (cp >> 6));
            out[offset++] = (char)(0x80 | (cp & 63));
        } else {
            out[offset++] = (char)(0xE0 | (cp >> 12));
            out[offset++] = (char)(0x80 | ((cp >> 6) & 63));
            out[offset++] = (char)(0x80 | (cp & 63));
        }
    }
    out[offset] = '\0';
#endif
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t resolve(const char *path, const fs_device_info_t **device, char *native, size_t capacity)
{
    const char *colon = strchr(path, ':');
    char alias[16];
    size_t length, root_length;
    if (!colon || colon[1] != '/') return BVSTK_FS_INVALID;
    length = (size_t)(colon - path);
    if (!length || length >= sizeof(alias)) return BVSTK_FS_INVALID;
    memcpy(alias, path, length); alias[length] = '\0';
    *device = fs_device_by_name(alias);
    if (!*device) return BVSTK_FS_NOT_FOUND;
    if (!supported(*device)) return BVSTK_FS_UNSUPPORTED;
    /* Read-only requests must never invoke fs_device_prepare / auto-format. */
    if (!(*device)->ctx || !fs_shared_is_ready((*device)->ctx)) return BVSTK_FS_NOT_READY;
    root_length = strlen((*device)->ctx->root);
    if (root_length >= capacity) return BVSTK_FS_RANGE;
    memcpy(native, (*device)->ctx->root, root_length);
    return utf8_to_native(colon + 2, native + root_length, capacity - root_length);
}

static void convert_info(const FILINFO *native, bvstk_fs_info_t *info)
{
    info->type = native->fattrib & AM_DIR ? BVSTK_FS_ENTRY_DIR : BVSTK_FS_ENTRY_FILE;
    info->size = info->type == BVSTK_FS_ENTRY_FILE ? native->fsize : 0;
    info->stamp = ((uint32_t)native->fdate << 16) | native->ftime;
    info->attributes = (uint8_t)((native->fattrib & (AM_RDO | AM_HID | AM_SYS)) |
                                ((native->fattrib & AM_ARC) ? 8 : 0));
}

static bvstk_fs_result_t stat_path(void *context, const char *path, bvstk_fs_info_t *out)
{
    const fs_device_info_t *device;
    char native[BVSTK_FS_PATH_CAPACITY];
    FILINFO info;
    bvstk_fs_result_t rc = resolve(path, &device, native, sizeof(native));
    (void)context;
    if (rc != BVSTK_FS_OK) return rc;
    if (!strcmp(native, device->ctx->root)) {
        memset(out, 0, sizeof(*out)); out->type = BVSTK_FS_ENTRY_DIR;
        return BVSTK_FS_OK;
    }
    rc = result(fs_shared_file_stat(device->ctx, native, &info));
    if (rc == BVSTK_FS_OK) convert_info(&info, out);
    return rc;
}

static bvstk_fs_result_t file_open(void *context, const char *path, void **out, uint64_t *size)
{
    const fs_device_info_t *device;
    char native[BVSTK_FS_PATH_CAPACITY];
    uint32_t file_size = 0;
    fs_dcp2_file_t *file;
    bvstk_fs_result_t rc = resolve(path, &device, native, sizeof(native));
    (void)context;
    *out = NULL;
    if (rc != BVSTK_FS_OK) return rc;
    file = pvPortMalloc(sizeof(*file));
    if (!file) return BVSTK_FS_BUSY;
    file->device = device;
    rc = result(fs_shared_file_open_read(device->ctx, native, &file->file, &file_size));
    if (rc != BVSTK_FS_OK) { vPortFree(file); return rc; }
    *size = file_size; *out = file;
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t file_read(void *context, void *handle, uint8_t *data, size_t capacity, size_t *size)
{
    fs_dcp2_file_t *file = handle;
    uint32_t count = 0;
    bvstk_fs_result_t rc;
    (void)context;
    rc = result(fs_shared_file_read(file->device->ctx, &file->file, data, (uint32_t)capacity, &count));
    *size = count;
    return rc;
}

static void file_close(void *context, void *handle)
{
    fs_dcp2_file_t *file = handle;
    (void)context;
    (void)fs_shared_file_close(file->device->ctx, &file->file);
    vPortFree(file);
}

static bvstk_fs_result_t dir_open(void *context, const char *path, void **out)
{
    const fs_device_info_t *device;
    char native[BVSTK_FS_PATH_CAPACITY];
    fs_dcp2_dir_t *dir;
    bvstk_fs_result_t rc = resolve(path, &device, native, sizeof(native));
    (void)context;
    *out = NULL;
    if (rc != BVSTK_FS_OK) return rc;
    dir = pvPortMalloc(sizeof(*dir));
    if (!dir) return BVSTK_FS_BUSY;
    dir->device = device;
    rc = result(fs_shared_dir_open(device->ctx, native, &dir->dir));
    if (rc != BVSTK_FS_OK) { vPortFree(dir); return rc; }
    *out = dir;
    return BVSTK_FS_OK;
}

static bvstk_fs_result_t dir_read(void *context, void *handle, char *name, size_t capacity, bvstk_fs_info_t *out)
{
    fs_dcp2_dir_t *dir = handle;
    FILINFO info;
    bvstk_fs_result_t rc;
    (void)context;
    do {
        rc = result(fs_shared_dir_read(dir->device->ctx, &dir->dir, &info));
        if (rc != BVSTK_FS_OK) return rc;
    } while (!strcmp(info.fname, ".") || !strcmp(info.fname, ".."));
    if (!info.fname[0]) { name[0] = '\0'; memset(out, 0, sizeof(*out)); return BVSTK_FS_OK; }
    convert_info(&info, out);
    return native_to_utf8(info.fname, name, capacity);
}

static void dir_close(void *context, void *handle)
{
    fs_dcp2_dir_t *dir = handle;
    (void)context;
    (void)fs_shared_dir_close(dir->device->ctx, &dir->dir);
    vPortFree(dir);
}

const bvstk_fs_backend_t *fs_dcp2_backend(void)
{
    static const bvstk_fs_backend_t backend = {
        NULL, volume_count, volume, stat_path, file_open, file_read, file_close, dir_open, dir_read, dir_close
    };
    return &backend;
}
