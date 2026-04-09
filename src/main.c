#include "main.h"
#include "config/config_store.h"
#include "fs/fs_devices.h"
#include "qspi_fs/qspi_fs.h"
#include "http/http_server.h"

#include "FreeRTOS.h"
#include "task.h"

int main()
{
	xil_printf("Hello from bvstk\n");
	qspi_flash_self_test();
	start_sd_card();
	start_qspi_fs();
	fs_devices_init();
	start_config_store();
	start_lan();
	start_tcp_server();
	start_http_server();
	start_dcp2_server();
	//start_smi();
	start_i2c();
	start_spi();

	vTaskStartScheduler();
	while (1);
	return 0;
}


void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
