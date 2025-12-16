#include "bvstk_lan.h"

#include "../config/config_store.h"

#include <string.h>

struct netif *netif;
struct netif server_netif;

/* Kept as a global symbol: referenced by other modules (e.g. uart_console). */
unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

void start_lan(void){
	sys_thread_new("lan_thrd", lan_thread, NULL,
			THREAD_STACKSIZE,
			tskIDLE_PRIORITY + 2);
}

void lan_thread(void *p)
{
    network_config_t cfg;
    if (config_store_wait_ready(5000)) {
        if (config_store_get_network(&cfg)) {
            if (cfg.has_mac) {
                memcpy(mac_ethernet_address, cfg.mac, sizeof(mac_ethernet_address));
            }
        }
    }

	lwip_init();

    ip_addr_t ipaddr, netmask, gw;
    netif = &server_netif;

    if (config_store_is_ready() && config_store_get_network(&cfg) &&
        cfg.has_ip && cfg.has_netmask && cfg.has_gateway) {
        IP4_ADDR(&ipaddr,
                 (cfg.ip_be >> 24) & 0xff, (cfg.ip_be >> 16) & 0xff,
                 (cfg.ip_be >> 8) & 0xff, (cfg.ip_be >> 0) & 0xff);
        IP4_ADDR(&netmask,
                 (cfg.netmask_be >> 24) & 0xff, (cfg.netmask_be >> 16) & 0xff,
                 (cfg.netmask_be >> 8) & 0xff, (cfg.netmask_be >> 0) & 0xff);
        IP4_ADDR(&gw,
                 (cfg.gateway_be >> 24) & 0xff, (cfg.gateway_be >> 16) & 0xff,
                 (cfg.gateway_be >> 8) & 0xff, (cfg.gateway_be >> 0) & 0xff);
    } else {
        IP4_ADDR(&ipaddr,  192, 168, 0, 10);
        IP4_ADDR(&netmask, 255, 255, 255,  0);
        IP4_ADDR(&gw,      192, 168, 0, 1);
    }

    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, XPAR_XEMACPS_0_BASEADDR)) {
	xil_printf("Error adding N/W interface\r\n");
	return;
    }

    netif_set_default(netif);

    netif_set_up(netif);

    sys_thread_new("xemacif_input_thread", (void(*)(void*))xemacif_input_thread, netif,
            THREAD_STACKSIZE,
			tskIDLE_PRIORITY + 2);

    vTaskDelete(NULL);

    return;
}
