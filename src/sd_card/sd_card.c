#include "sd_card.h"
#include "xil_cache.h"
#include "semphr.h"
#include "lwip/sockets.h"
#include "../bvstk_tcp_server/utils/utils.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define SD_CARD_DEVICE_ID   XPAR_XSDPS_0_DEVICE_ID
#define SD_MOUNT_POINT      SD_ROOT
#define SD_TASK_STACK       1024
#define SD_TASK_PRIO        (tskIDLE_PRIORITY + 2)
#define SD_PATH_MAX         128

static XSdPs sd_instance;
static FATFS fatfs;
static TaskHandle_t sd_task_handle = NULL;
static SemaphoreHandle_t sd_mutex = NULL;
static volatile int sd_ready = 0;

static int sd_lock(void)
{
    if (!sd_mutex) return 0;
    return xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(200)) == pdTRUE;
}

static void sd_unlock(void)
{
    if (sd_mutex) xSemaphoreGive(sd_mutex);
}

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

static void normalize_name(char *s)
{
    if (!s) return;
    bool has_lower = false, has_upper = false;
    for (char *p = s; *p; ++p) {
        if (islower((unsigned char)*p)) has_lower = true;
        if (isupper((unsigned char)*p)) has_upper = true;
    }
    if (has_upper && !has_lower) {
        for (char *p = s; *p; ++p) {
            *p = (char)tolower((unsigned char)*p);
        }
    }
}

static int sd_mount_fatfs(void)
{
    FRESULT res = f_mount(&fatfs, SD_MOUNT_POINT, 1);
    if (res != FR_OK) {
        /* If card is blank, format it */
        BYTE work[FF_MAX_SS];
        res = f_mkfs(SD_MOUNT_POINT, 0, work, sizeof(work));
        if (res != FR_OK) return XST_FAILURE;
        res = f_mount(&fatfs, SD_MOUNT_POINT, 1);
        if (res != FR_OK) return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static void sd_card_task(void *arg)
{
    (void)arg;
    if (sd_hw_init() == XST_SUCCESS && sd_mount_fatfs() == XST_SUCCESS) {
        sd_ready = 1;
        xil_printf("SD: mounted %s\r\n", SD_MOUNT_POINT);
    }
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int start_sd_card(void)
{
    if (sd_task_handle) return XST_SUCCESS;
    sd_mutex = xSemaphoreCreateMutex();
    if (!sd_mutex) return XST_FAILURE;
    BaseType_t rc = xTaskCreate(sd_card_task, "sd_card", SD_TASK_STACK, NULL, SD_TASK_PRIO, &sd_task_handle);
    return (rc == pdPASS) ? XST_SUCCESS : XST_FAILURE;
}

int sd_card_get_info(sd_card_info_t *info)
{
    if (!sd_ready || !info) return XST_FAILURE;
    FATFS *fs;
    DWORD free_clusters = 0;
    FRESULT res = f_getfree(SD_MOUNT_POINT, &free_clusters, &fs);
    if (res != FR_OK || fs == NULL) return XST_FAILURE;
    DWORD total_clusters = fs->n_fatent - 2; /* FAT entries include lead/EOF */
    info->block_size = (u32)fs->csize * 512U;
    info->block_count = (u64)total_clusters * fs->csize;
    info->capacity_bytes = (u64)info->block_count * 512U;
    info->mounted = sd_ready;
    return XST_SUCCESS;
}

static int ensure_ready(int fd)
{
    if (!sd_ready) {
        write_str(fd, "SD not ready\r\n");
        return 0;
    }
    return 1;
}

int sd_card_ls(int fd)
{
    if (!ensure_ready(fd)) return XST_FAILURE;
    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, SD_MOUNT_POINT);
    if (res != FR_OK) {
        write_str(fd, "f_opendir failed\r\n");
        return XST_FAILURE;
    }
    char line[96];
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        char name[sizeof(fno.fname)];
        strncpy(name, fno.fname, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        normalize_name(name);
        int n = snprintf(line, sizeof(line), "%c %10lu %s\r\n",
            (fno.fattrib & AM_DIR) ? 'd' : '-',
            (unsigned long)fno.fsize,
            name);
        lwip_write(fd, line, n);
    }
    f_closedir(&dir);
    return XST_SUCCESS;
}

int sd_fs_ls(const char *path, int fd)
{
    if (!ensure_ready(fd)) return XST_FAILURE;
    if (!sd_lock()) { write_str(fd, "SD busy\r\n"); return XST_FAILURE; }
    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, path ? path : SD_MOUNT_POINT);
    if (res != FR_OK) {
        write_str(fd, "f_opendir failed\r\n");
        sd_unlock();
        return XST_FAILURE;
    }
    char line[96];
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        char name[sizeof(fno.fname)];
        strncpy(name, fno.fname, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        normalize_name(name);
        int n = snprintf(line, sizeof(line), "%c %10lu %s\r\n",
            (fno.fattrib & AM_DIR) ? 'd' : '-',
            (unsigned long)fno.fsize,
            name);
        lwip_write(fd, line, n);
    }
    f_closedir(&dir);
    sd_unlock();
    return XST_SUCCESS;
}

int sd_fs_cat(const char *path, int fd)
{
    if (!ensure_ready(fd)) return XST_FAILURE;
    if (!sd_lock()) { write_str(fd, "SD busy\r\n"); return XST_FAILURE; }
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        write_str(fd, "open failed\r\n");
        sd_unlock();
        return XST_FAILURE;
    }
    char buf[128];
    UINT br = 0;
    do {
        res = f_read(&file, buf, sizeof(buf), &br);
        if (res != FR_OK) break;
        if (br) lwip_write(fd, buf, br);
    } while (br == sizeof(buf));
    f_close(&file);
    write_str(fd, "\r\n");
    sd_unlock();
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int sd_fs_touch(const char *path)
{
    if (!sd_ready || !path) return XST_FAILURE;
    if (!sd_lock()) return XST_FAILURE;
    FIL file;
    FRESULT res = f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS);
    if (res == FR_OK) {
        res = f_truncate(&file);
        f_close(&file);
    }
    sd_unlock();
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int sd_fs_mkdir(const char *path)
{
    if (!sd_ready || !path) return XST_FAILURE;
    if (!sd_lock()) return XST_FAILURE;
    FRESULT res = f_mkdir(path);
    sd_unlock();
    return (res == FR_OK || res == FR_EXIST) ? XST_SUCCESS : XST_FAILURE;
}

int sd_fs_rm(const char *path)
{
    if (!sd_ready || !path) return XST_FAILURE;
    if (!sd_lock()) return XST_FAILURE;
    FRESULT res = f_unlink(path);
    sd_unlock();
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int sd_fs_is_dir(const char *path)
{
    if (!sd_ready || !path) return XST_FAILURE;
    if (!sd_lock()) return XST_FAILURE;
    if (strcmp(path, SD_ROOT) == 0) { sd_unlock(); return XST_SUCCESS; }
    char norm[SD_PATH_MAX];
    strncpy(norm, path, sizeof(norm) - 1);
    norm[sizeof(norm) - 1] = '\0';
    size_t len = strlen(norm);
    if (len > 1 && norm[len - 1] == '/') { norm[len - 1] = '\0'; }
    FILINFO fno;
    FRESULT res = f_stat(norm, &fno);
    sd_unlock();
    if (res != FR_OK) return XST_FAILURE;
    return (fno.fattrib & AM_DIR) ? XST_SUCCESS : XST_FAILURE;
}

int sd_fs_complete(const char *dir, const char *prefix, char results[][SD_NAME_MAX], int max_results, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!sd_ready || !dir || !results || max_results <= 0) return XST_FAILURE;
    if (!sd_lock()) return XST_FAILURE;
    DIR d;
    FILINFO fno;
    FRESULT res = f_opendir(&d, dir);
    if (res != FR_OK) { sd_unlock(); return XST_FAILURE; }
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    int cnt = 0;
    while (f_readdir(&d, &fno) == FR_OK && fno.fname[0]) {
        char name[sizeof(fno.fname)];
        strncpy(name, fno.fname, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        normalize_name(name);
        if (prefix_len && strncmp(name, prefix, prefix_len) != 0) continue;
        if (cnt < max_results) {
            strncpy(results[cnt], name, SD_NAME_MAX - 2);
            results[cnt][SD_NAME_MAX - 2] = '\0';
            if (fno.fattrib & AM_DIR) {
                size_t nlen = strlen(results[cnt]);
                if (nlen < SD_NAME_MAX - 1) {
                    results[cnt][nlen] = '/';
                    results[cnt][nlen + 1] = '\0';
                }
            }
        }
        cnt++;
    }
    f_closedir(&d);
    sd_unlock();
    if (out_count) *out_count = cnt;
    return XST_SUCCESS;
}

int sd_card_cat(const char *path, int fd)
{
    if (!ensure_ready(fd)) return XST_FAILURE;
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        write_str(fd, "open failed\r\n");
        return XST_FAILURE;
    }
    char buf[128];
    UINT br = 0;
    do {
        res = f_read(&file, buf, sizeof(buf), &br);
        if (res != FR_OK) break;
        if (br) lwip_write(fd, buf, br);
    } while (br == sizeof(buf));
    f_close(&file);
    write_str(fd, "\r\n");
    return (res == FR_OK) ? XST_SUCCESS : XST_FAILURE;
}

int sd_card_write_text(const char *path, const char *text, int append)
{
    if (!sd_ready) return XST_FAILURE;
    FIL file;
    BYTE mode = append ? (FA_WRITE | FA_OPEN_ALWAYS) : (FA_WRITE | FA_CREATE_ALWAYS);
    FRESULT res = f_open(&file, path, mode);
    if (res != FR_OK) return XST_FAILURE;
    if (append) f_lseek(&file, f_size(&file));
    UINT bw = 0;
    res = f_write(&file, text, strlen(text), &bw);
    f_close(&file);
    return (res == FR_OK && bw == strlen(text)) ? XST_SUCCESS : XST_FAILURE;
}
