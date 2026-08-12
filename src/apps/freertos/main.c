#include "apps/freertos/main.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/services/http/http_server.h"
#include "apps/freertos/services/ssh/bvstk_ssh_server.h"
#include "apps/freertos/storage/fs/fs_devices.h"
#include "apps/freertos/storage/qspi/qspi_fs.h"

#include "FreeRTOS.h"
#include "task.h"

int main()
{
	xil_printf("Hello from bvstk\r\n");
	qspi_flash_self_test();
	start_sd_card();
	start_qspi_fs();
	fs_devices_init();
	start_config_store();
	start_lan();
	start_tcp_server();
	start_ssh_server();
	start_http_server();
	start_dcp2_server();
	//start_smi();
	start_i2c();
	bvstk_runtime_start();

	vTaskStartScheduler();
	while (1);
	return 0;
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
