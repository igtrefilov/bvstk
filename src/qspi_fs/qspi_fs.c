#include "qspi_fs.h"

#include "../fs/fs_shared.h"
#include "semphr.h"

#include "../qspi_flash/qspi_flash.h"
#include "xil_printf.h"

#define QSPI_TASK_STACK       1024
#define QSPI_TASK_PRIO        (tskIDLE_PRIORITY + 1)

static FATFS qspi_fatfs;
static fs_shared_ctx_t qspi_ctx;
static volatile int qspi_ready = 0;
static SemaphoreHandle_t qspi_mutex = NULL;
static TaskHandle_t qspi_task_handle = NULL;

static void qspi_fs_task(void *arg)
{
    (void)arg;
    qspi_ctx.fatfs = &qspi_fatfs;
    qspi_ctx.root = QSPI_ROOT;
    qspi_ctx.ready = &qspi_ready;
    qspi_ctx.mutex = &qspi_mutex;
    qspi_ready = 0;
    if (qspi_flash_init() == XST_SUCCESS) {
        fs_shared_mount(&qspi_ctx, "QSPI");
    } else {
        xil_printf("QSPI: init failed\r\n");
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int start_qspi_fs(void)
{
    if (qspi_task_handle) return XST_SUCCESS;
    qspi_mutex = xSemaphoreCreateMutex();
    if (!qspi_mutex) return XST_FAILURE;
    qspi_ctx.fatfs = &qspi_fatfs;
    qspi_ctx.root = QSPI_ROOT;
    qspi_ctx.ready = &qspi_ready;
    qspi_ctx.mutex = &qspi_mutex;
    qspi_ready = 0;
    BaseType_t rc = xTaskCreate(qspi_fs_task, "qspi_fs", QSPI_TASK_STACK, NULL, QSPI_TASK_PRIO, &qspi_task_handle);
    return (rc == pdPASS) ? XST_SUCCESS : XST_FAILURE;
}

fs_shared_ctx_t *qspi_fs_get_context(void)
{
    return &qspi_ctx;
}

int qspi_fs_is_ready(void)
{
    return qspi_ready;
}
