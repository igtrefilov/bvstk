#include "sd_card.h"
#include "../fs/fs_shared.h"
#include "semphr.h"

#include "xsdps.h"
#include "xil_printf.h"
#include <string.h>
#include <stdbool.h>

#define SD_CARD_DEVICE_ID   XPAR_XSDPS_0_DEVICE_ID
#define SD_TASK_STACK       1024
#define SD_TASK_PRIO        (tskIDLE_PRIORITY + 2)

static XSdPs sd_instance;
static FATFS sd_fatfs;
static fs_shared_ctx_t sd_ctx;
static volatile int sd_ready = 0;
static SemaphoreHandle_t sd_mutex = NULL;
static TaskHandle_t sd_task_handle = NULL;

static int sd_hw_init(void)
{
    XSdPs_Config *cfg = XSdPs_LookupConfig(SD_CARD_DEVICE_ID);
    if (!cfg) return XST_FAILURE;
    int status = XSdPs_CfgInitialize(&sd_instance, cfg, cfg->BaseAddress);
    if (status != XST_SUCCESS) return status;
    status = XSdPs_CardInitialize(&sd_instance);
    if (status != XST_SUCCESS) return status;
    return XST_SUCCESS;
}

static bool sd_card_hw_ready = false;

static int sd_card_try_mount(void)
{
    if (!sd_card_hw_ready) {
        if (sd_hw_init() != XST_SUCCESS) {
            return XST_FAILURE;
        }
        sd_card_hw_ready = true;
    }
    if (fs_shared_mount(&sd_ctx, "SD") != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static void sd_card_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!sd_ready) {
            sd_card_try_mount();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int start_sd_card(void)
{
    if (sd_task_handle) return XST_SUCCESS;
    sd_mutex = xSemaphoreCreateMutex();
    if (!sd_mutex) return XST_FAILURE;
    sd_ctx.fatfs = &sd_fatfs;
    sd_ctx.root = SD_ROOT;
    sd_ctx.ready = &sd_ready;
    sd_ctx.mutex = &sd_mutex;
    sd_ready = 0;
    sd_card_hw_ready = false;
    sd_card_try_mount();
    BaseType_t rc = xTaskCreate(sd_card_task, "sd_card", SD_TASK_STACK, NULL, SD_TASK_PRIO, &sd_task_handle);
    return (rc == pdPASS) ? XST_SUCCESS : XST_FAILURE;
}

fs_shared_ctx_t *sd_card_get_context(void)
{
    return &sd_ctx;
}

int sd_card_is_ready(void)
{
    return sd_ready;
}

int sd_card_get_info(sd_card_info_t *info)
{
    if (!sd_ready || !info) return XST_FAILURE;
    FATFS *fs;
    DWORD free_clusters = 0;
    FRESULT res = f_getfree(sd_ctx.root, &free_clusters, &fs);
    if (res != FR_OK || fs == NULL) return XST_FAILURE;
    DWORD total_clusters = fs->n_fatent - 2;
    info->block_size = (u32)fs->csize * 512U;
    info->block_count = (u64)total_clusters * fs->csize;
    info->capacity_bytes = (u64)info->block_count * 512U;
    info->mounted = sd_ready;
    return XST_SUCCESS;
}

int sd_card_ls(int fd)
{
    return fs_shared_fs_ls(&sd_ctx, NULL, fd);
}

int sd_fs_ls(const char *path, int fd)
{
    return fs_shared_fs_ls(&sd_ctx, path, fd);
}

int sd_fs_cat(const char *path, int fd)
{
    return fs_shared_fs_cat(&sd_ctx, path, fd);
}

int sd_fs_touch(const char *path)
{
    return fs_shared_fs_touch(&sd_ctx, path);
}

int sd_fs_mkdir(const char *path)
{
    return fs_shared_fs_mkdir(&sd_ctx, path);
}

int sd_fs_rm(const char *path)
{
    return fs_shared_fs_rm(&sd_ctx, path);
}

int sd_fs_is_dir(const char *path)
{
    if (!sd_ready || !path) return XST_FAILURE;
    return fs_shared_fs_is_dir(&sd_ctx, path);
}

int sd_fs_complete(const char *dir, const char *prefix, char results[][FS_NAME_MAX], int max_results, int *out_count)
{
    if (!sd_ready) return XST_FAILURE;
    return fs_shared_fs_complete(&sd_ctx, dir, prefix, results, max_results, out_count);
}

int sd_card_cat(const char *path, int fd)
{
    return fs_shared_fs_cat(&sd_ctx, path, fd);
}

int sd_card_write_text(const char *path, const char *text, int append)
{
    if (!sd_ready || !path || !text) return XST_FAILURE;
    if (!sd_ctx.mutex || !*(sd_ctx.mutex)) return XST_FAILURE;
    if (xSemaphoreTake(*(sd_ctx.mutex), pdMS_TO_TICKS(200)) != pdTRUE) return XST_FAILURE;
    FIL file;
    BYTE mode = append ? (FA_WRITE | FA_OPEN_ALWAYS) : (FA_WRITE | FA_CREATE_ALWAYS);
    FRESULT res = f_open(&file, path, mode);
    if (res == FR_OK) {
        if (append) f_lseek(&file, f_size(&file));
        UINT bw = 0;
        res = f_write(&file, text, strlen(text), &bw);
        f_close(&file);
        if (res == FR_OK && bw == strlen(text)) {
            xSemaphoreGive(*(sd_ctx.mutex));
            return XST_SUCCESS;
        }
    }
    f_close(&file);
    xSemaphoreGive(*(sd_ctx.mutex));
    return XST_FAILURE;
}
