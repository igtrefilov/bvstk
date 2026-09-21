#include "apps/freertos/storage/sd-pl/sd_pl_card.h"

#include "apps/freertos/storage/fs/fs_shared.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "semphr.h"
#include "task.h"
#include "xstatus.h"
#include "xil_printf.h"

#define SD_PL_TASK_STACK 2048U
#define SD_PL_TASK_PRIORITY (tskIDLE_PRIORITY + 4)

static FATFS s_fatfs;
static fs_shared_ctx_t s_ctx;
static volatile int s_ready;
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;

static void sd_pl_card_task(void *argument)
{
    (void)argument;
    /* GIC belongs to the running scheduler. Never reinitialize media under
     * already-open filesystem handles. Startup makes exactly one attempt. */
    xil_printf("SD-PL: initializing and mounting sd-pl:/...\r\n");
    if (bvstk_sd_master_freertos_start() == BVSTK_OK &&
        fs_shared_mount(&s_ctx, "SD-PL") == XST_SUCCESS) {
        xil_printf("SD-PL: mounted sd-pl:/ (read/write, existing filesystem)\r\n");
    } else {
        xil_printf("SD-PL: sd-pl:/ unavailable; check card and restart; no formatting\r\n");
    }
    vTaskDelete(NULL);
}

int start_sd_pl_card(void)
{
    BaseType_t result;

    if (s_task != NULL) {
        return XST_SUCCESS;
    }
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return XST_FAILURE;
    }
    s_ctx.fatfs = &s_fatfs;
    s_ctx.root = SD_PL_ROOT;
    s_ctx.ready = &s_ready;
    s_ctx.mutex = &s_mutex;
    s_ctx.preserve_media = true;
    s_ctx.media_ready = bvstk_sd_pl_is_ready;
    s_ready = 0;
    result = xTaskCreate(sd_pl_card_task,
                         "sd_pl_card",
                         SD_PL_TASK_STACK,
                         NULL,
                         SD_PL_TASK_PRIORITY,
                         &s_task);
    if (result != pdPASS) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int sd_pl_card_is_ready(void)
{
    return fs_shared_is_ready(&s_ctx);
}

fs_shared_ctx_t *sd_pl_card_get_context(void)
{
    return &s_ctx;
}
