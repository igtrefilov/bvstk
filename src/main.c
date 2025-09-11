#include <stdio.h>
#include "xparameters.h"
#include "xsdps.h"
#include "netif/xadapter.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "gpio_proc/gpio_processing.h"
#include "dma_proc/dma_processing.h"
#include "uart_console/uart_console.h"
#include "mqtt_proc/mqtt_processing.h"
#include "sntp_proc/sntp_processing.h"
#include "sd_card/sd_card.h"

#include "xil_cache.h"
#include "xstatus.h"

#define TIME_SYNC_TASK_STACK_SIZE 2048

extern TaskHandle_t xTaskButtonHandle;
extern TaskHandle_t DMAInstance;
extern XSdPs SD_Ptr;

struct netif *netif;
/* the mac address of the board. this should be unique per board */
unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

int main_thread();
void echo_application_thread(void *);
void time_sync_task(void *params);

void init_ddr_eth_system(void);
void lwip_init();

static struct netif server_netif;
struct netif *echo_netif;

int main()
{
	/*int SD_Status;*/

	Xil_ICacheEnable();
	Xil_DCacheEnable();

	/*xil_printf("test\n");

	SD_Status = sd_init(SD_Ptr);
	if(SD_Status != XST_SUCCESS){
		xil_printf("SD Init failed\r\n");
		return 0;
	}*/


	sys_thread_new("main_thrd", (void(*)(void*))main_thread, 0,
					THREAD_STACKSIZE,
					tskIDLE_PRIORITY);
	xTaskCreate(TaskButton, "TaskButton", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xTaskButtonHandle);
	xTaskCreate(TaskConsole, "TaskConsole", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xTaskButtonHandle);
	init_ddr_eth_system();
	xTaskCreate(init_dma_intr, "init_dma_intr", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &DMAInstance);
	//void init_dma_intr();
	vTaskStartScheduler();

	while (1);

	return 0;
}

void network_thread(void *p)
{
    ip_addr_t ipaddr, netmask, gw;
    netif = &server_netif;

    /* initialize IP addresses to be used */
    IP4_ADDR(&ipaddr,  192, 168, 0, 10);
    IP4_ADDR(&netmask, 255, 255, 255,  0);
    IP4_ADDR(&gw,      192, 168, 0, 1);

    /* Add network interface to the netif_list, and set it as default */
    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, PLATFORM_EMAC_BASEADDR)) {
	xil_printf("Error adding N/W interface\r\n");
	return;
    }

    netif_set_default(netif);

    /* specify that the network if is up */
    netif_set_up(netif);

    /* start packet receive thread - required for lwIP operation */
    sys_thread_new("xemacif_input_thread", (void(*)(void*))xemacif_input_thread, netif,
            THREAD_STACKSIZE,
			tskIDLE_PRIORITY);

    xTaskCreate(time_sync_task, "TimeSync", TIME_SYNC_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    sys_thread_new("mqtt_thrd", (void(*)(void*))mqtt_init, NULL,
                   THREAD_STACKSIZE,
                   tskIDLE_PRIORITY);

    sys_thread_new("echod", echo_application_thread, 0,
		THREAD_STACKSIZE,
		tskIDLE_PRIORITY);

    vTaskDelete(NULL);

    return;
}

int main_thread()
{
    lwip_init();

    /* any thread using lwIP should be created using sys_thread_new */
    sys_thread_new("NW_THRD", network_thread, NULL,
		THREAD_STACKSIZE,
		tskIDLE_PRIORITY);

    sys_thread_new("blink_LED", blink_LED, NULL,
    		THREAD_STACKSIZE,
			tskIDLE_PRIORITY);

    vTaskDelete(NULL);
    return 0;
}
