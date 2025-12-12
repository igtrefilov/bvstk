#include "qspi_fs.h"

#include "../fs/fs_shared.h"
#include "semphr.h"

#include "../qspi_flash/qspi_flash.h"
#include "xil_printf.h"
#include <stdbool.h>

#define QSPI_TASK_STACK       1024
#define QSPI_TASK_PRIO        (tskIDLE_PRIORITY + 1)

static FATFS qspi_fatfs;
static fs_shared_ctx_t qspi_ctx;
static volatile int qspi_ready = 0;
static SemaphoreHandle_t qspi_mutex = NULL;
static TaskHandle_t qspi_task_handle = NULL;
static bool qspi_flash_initialized = false;

static int qspi_fs_try_mount(void)
{
    if (!qspi_flash_initialized) {
        if (qspi_flash_init() != XST_SUCCESS) {
            xil_printf("QSPI: init failed\r\n");
            return XST_FAILURE;
        }
        qspi_flash_initialized = true;
    }
    if (fs_shared_mount(&qspi_ctx, "QSPI") != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static void qspi_fs_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!qspi_ready) {
            qspi_fs_try_mount();
        }
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
    qspi_flash_initialized = false;
    qspi_fs_try_mount();
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
