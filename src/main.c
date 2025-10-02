#include "main.h"

int main()
{
	xil_printf("Hello from bvstk\n");
	start_lan();
	start_tcp_server();
	start_smi();
	
	vTaskStartScheduler();
	while (1);
	return 0;
}
