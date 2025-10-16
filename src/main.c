#include "main.h"

int main()
{
	xil_printf("Hello from bvstk\n");
	start_lan();
	start_tcp_server();
	start_smi();
	start_i2c();
	
	vTaskStartScheduler();
	while (1);
	return 0;
}
