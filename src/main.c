#include "main.h"

int main()
{
	start_lan();
	start_tcp_server();
	start_smi();
	
	vTaskStartScheduler();
	while (1);
	return 0;
}
