#include "apps/freertos/storage/sd-pl/sd_pl_card.h"

#include "apps/freertos/storage/fs/fs_shared.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "semphr.h"
#include "task.h"
#include "xil_printf.h"
#include "xstatus.h"

#define SD_PL_TASK_STACK 1024U
#define SD_PL_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

static FATFS s_fatfs;
static fs_shared_ctx_t s_ctx;
static volatile int s_ready;
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;

static int sd_pl_card_try_mount(void)
{
    if (fs_shared_mount(&s_ctx, "SD-PL") != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static void sd_pl_card_task(void *argument)
{
    (void)argument;
    for (;;) {
        if (!s_ready) {
            if (sd_pl_card_try_mount() != XST_SUCCESS) {
                xil_printf("SD-PL: mount retry\r\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
    return s_ready;
}

fs_shared_ctx_t *sd_pl_card_get_context(void)
{
    return &s_ctx;
}
