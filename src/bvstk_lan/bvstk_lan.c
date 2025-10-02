#include "bvstk_lan.h"

struct netif *netif;
struct netif server_netif;

unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

void start_lan(void){
	sys_thread_new("lan_thrd", lan_thread, NULL,
			THREAD_STACKSIZE,
			tskIDLE_PRIORITY);
}

void lan_thread(void *p)
{
	lwip_init();

    ip_addr_t ipaddr, netmask, gw;
    netif = &server_netif;

    IP4_ADDR(&ipaddr,  192, 168, 0, 10);
    IP4_ADDR(&netmask, 255, 255, 255,  0);
    IP4_ADDR(&gw,      192, 168, 0, 1);

    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, XPAR_XEMACPS_0_BASEADDR)) {
	xil_printf("Error adding N/W interface\r\n");
	return;
    }

    netif_set_default(netif);

    netif_set_up(netif);

    sys_thread_new("xemacif_input_thread", (void(*)(void*))xemacif_input_thread, netif,
            THREAD_STACKSIZE,
			tskIDLE_PRIORITY);

    vTaskDelete(NULL);

    return;
}
