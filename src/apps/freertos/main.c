#if defined(BVSTK_PL_SPI_DIAGNOSTIC) && BVSTK_PL_SPI_DIAGNOSTIC
#include "apps/freertos/diagnostics/pl_spi_test/tests.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#else
#include "apps/freertos/main.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/services/http/http_server.h"
#include "apps/freertos/services/ssh/bvstk_ssh_server.h"
#include "apps/freertos/services/uart-console/bvstk_uart_console.h"
#include "apps/freertos/storage/fs/fs_devices.h"
#include "apps/freertos/storage/qspi/qspi_fs.h"
#include "apps/freertos/storage/sd-pl/sd_pl_card.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#endif

int main(void)
{
#if defined(BVSTK_PL_SPI_DIAGNOSTIC) && BVSTK_PL_SPI_DIAGNOSTIC
    if (pl_spi_test_start() != 0) {
        xil_printf("PL SPI: unable to create diagnostic task\r\n");
        return 1;
    }
#else
#if !BVSTK_PL_SD_AUTOSTART_FILESYSTEM
    if (bvstk_sd_master_freertos_schedule_start() != BVSTK_OK) {
        xil_printf("SD PL: unable to schedule service startup\r\n");
    }
#endif
    qspi_flash_self_test();
    (void)start_sd_card();
#if BVSTK_PL_HAS_SD_CONTROLLER && BVSTK_PL_SD_AUTOSTART_FILESYSTEM
    if (start_sd_pl_card() != XST_SUCCESS) {
        xil_printf("SD-PL: unable to schedule filesystem startup\r\n");
    }
#endif
    start_qspi_fs();
    fs_devices_init();
    start_config_store();
    start_lan();
    start_tcp_server();
    start_ssh_server();
    start_http_server();
    start_dcp2_server();
#if BVSTK_PL_RUNTIME_ENABLED
    bvstk_runtime_start();
#endif
    if (start_uart_console() != 0) {
        xil_printf("UART: unable to create console task\r\n");
    }
#endif
    vTaskStartScheduler();
    xil_printf("BVSTK: scheduler returned unexpectedly\r\n");
    for (;;) {
    }
}


void vApplicationMallocFailedHook(void)
{
    xil_printf("FREERTOS: malloc failed\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    xil_printf("FREERTOS: stack overflow task=%s\r\n",
               pcTaskName ? pcTaskName : "(null)");
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
