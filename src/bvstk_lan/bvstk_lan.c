#include "bvstk_lan.h"

#include "../config/config_store.h"

#include <string.h>
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "FreeRTOS.h"
#include "task.h"

struct netif *netif;
struct netif server_netif;

/* Kept as a global symbol: referenced by other modules (e.g. uart_console). */
unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

static void lan_xemacif_irq_thread(void *arg)
{
    struct netif *nif = (struct netif *)arg;
    struct xemac_s *emac = (struct xemac_s *)nif->state;

    while (1) {
        (void)sys_arch_sem_wait(&emac->sem_rx_data_available, 0);
        (void)xemacif_input(nif);
    }
}

void start_lan(void){
    sys_thread_t th;

	th = sys_thread_new("lan_thrd", lan_thread, NULL,
				THREAD_STACKSIZE,
				tskIDLE_PRIORITY + 2);
    (void)th;
}

void lan_thread(void *p)
{
    sys_thread_t in_th;

    (void)p;

    xil_printf("LAN: thread start\r\n");

    network_config_t cfg;
    if (config_store_wait_ready(5000)) {
        xil_printf("LAN: config_store ready\r\n");
        if (config_store_get_network(&cfg)) {
            xil_printf("LAN: config loaded has_ip=%d has_nm=%d has_gw=%d has_mac=%d\r\n",
                       cfg.has_ip ? 1 : 0, cfg.has_netmask ? 1 : 0,
                       cfg.has_gateway ? 1 : 0, cfg.has_mac ? 1 : 0);
            if (cfg.has_mac) {
                memcpy(mac_ethernet_address, cfg.mac, sizeof(mac_ethernet_address));
            }
        } else {
            xil_printf("LAN: config_store_get_network failed\r\n");
        }
    } else {
        xil_printf("LAN: config_store wait timeout, using defaults\r\n");
    }

	xil_printf("LAN: lwip_init()\r\n");
	lwip_init();
    xil_printf("LAN: lwip_init done\r\n");

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
        xil_printf("LAN: using network config from config_store\r\n");
    } else {
        IP4_ADDR(&ipaddr,  192, 168, 0, 10);
        IP4_ADDR(&netmask, 255, 255, 255,  0);
        IP4_ADDR(&gw,      192, 168, 0, 1);
        xil_printf("LAN: using fallback static network config\r\n");
    }

    xil_printf(
        "LAN: MAC=%02X:%02X:%02X:%02X:%02X:%02X base=0x%08lX\r\n",
        (unsigned)mac_ethernet_address[0], (unsigned)mac_ethernet_address[1],
        (unsigned)mac_ethernet_address[2], (unsigned)mac_ethernet_address[3],
        (unsigned)mac_ethernet_address[4], (unsigned)mac_ethernet_address[5],
        (unsigned long)XPAR_XEMACPS_0_BASEADDR);

    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, XPAR_XEMACPS_0_BASEADDR)) {
        xil_printf("LAN: ERROR xemac_add failed\r\n");
		return;
    }
    xil_printf("LAN: xemac_add ok\r\n");

    netif_set_default(netif);
    xil_printf("LAN: netif_set_default done\r\n");

    netif_set_up(netif);

    in_th = sys_thread_new("xemacif_irq_thread", lan_xemacif_irq_thread, netif,
            THREAD_STACKSIZE,
				tskIDLE_PRIORITY + 2);
    (void)in_th;

    vTaskDelete(NULL);

    return;
}
