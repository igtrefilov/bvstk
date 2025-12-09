#include "sd_card.h"
#include "xil_cache.h"
#include "semphr.h"
#include "lwip/sockets.h"
#include "../bvstk_tcp_server/utils/utils.h"
#include <stdio.h>
#include <string.h>

#define SD_CARD_DEVICE_ID   XPAR_XSDPS_0_DEVICE_ID
#define SD_MOUNT_POINT      "0:/"
#define SD_TASK_STACK       1024
#define SD_TASK_PRIO        (tskIDLE_PRIORITY + 2)

static XSdPs sd_instance;
static FATFS fatfs;
static TaskHandle_t sd_task_handle = NULL;
static SemaphoreHandle_t sd_mutex = NULL;
static volatile int sd_ready = 0;

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
    XSdPs_CardInfo *ci = &sd_instance.CardInfo;
    info->block_size = ci->BlkSize;
    info->block_count = (u64)ci->Capacity;
    info->capacity_bytes = (u64)ci->Capacity * ci->BlkSize;
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
        int n = snprintf(line, sizeof(line), "%c %10lu %s\r\n",
            (fno.fattrib & AM_DIR) ? 'd' : '-',
            (unsigned long)fno.fsize,
            fno.fname);
        lwip_write(fd, line, n);
    }
    f_closedir(&dir);
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
