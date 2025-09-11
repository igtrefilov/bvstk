#include "uart_console.h"

#define UART_DEVICE_ID XPAR_XUARTPS_0_DEVICE_ID
XUartPs Uart_Ps;

extern struct netif *netif;
extern unsigned char mac_ethernet_address[];

void TaskConsole(void *pvParameters) {
    XUartPs_Config *Config;
    char input_buffer[100];
    int received_bytes = 0;

    Config = XUartPs_LookupConfig(UART_DEVICE_ID);
    if (Config == NULL) {
        xil_printf("Ошибка: конфигурация UART не найдена.\n");
        return;
    }

    if (XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress) != XST_SUCCESS) {
        xil_printf("Ошибка: инициализация UART не удалась.\n");
        return;
    }

    XUartPs_SetBaudRate(&Uart_Ps, 115200);

    xil_printf("Zynq> ");
    while (1) {
        int byte_received = XUartPs_Recv(&Uart_Ps, (u8 *)&input_buffer[received_bytes], 1);
        if (byte_received > 0) {
            char ch = input_buffer[received_bytes];
            xil_printf("%c", ch);
            if (ch == '\r') {
                xil_printf("\r\n");
                input_buffer[received_bytes] = '\0';
                command(input_buffer);
                received_bytes = 0;
                xil_printf("Zynq> ");
            } else if (received_bytes < sizeof(input_buffer) - 1) {
                received_bytes++;
            } else {
                xil_printf("\nБуфер переполнен\r\n");
                received_bytes = 0;
                xil_printf("Zynq> ");
            }
        }
    }
}

void command(char *command_str) {
    if (strcmp(command_str, "show ip") == 0) {
        print_ip_address(netif);
    } else if (strcmp(command_str, "show netmask") == 0) {
    	print_netmask_address(netif);
    } else if (strcmp(command_str, "show gw") == 0) {
        	print_gw_address(netif);
    } else if (strcmp(command_str, "show mac") == 0) {
            	print_mac(mac_ethernet_address);
    } else if (strcmp(command_str, "show iface") == 0) {
            	print_iface(netif);
    } else if (strncmp(command_str, "set ip ", 7) == 0) {
        char *ip_str = command_str + 7;
        set_ip(ip_str);
    } else if (strncmp(command_str, "set netmask ", 12) == 0) {
            char *ip_str = command_str + 12;
            set_netmask(ip_str);
    } else if (strncmp(command_str, "set gw ", 7) == 0) {
                char *ip_str = command_str + 7;
                set_gw(ip_str);
    } else {
        xil_printf("Неизвестная команда: %s\r\n", command_str);
    }
}

void print_ip_address(struct netif *netif) {
    if (netif != NULL) {
        ip_addr_t ip = netif->ip_addr;
        u8_t *ip_bytes = (u8_t *)&ip.addr;
        xil_printf("IP Address: %d.%d.%d.%d\r\n",
                   ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    } else {
        xil_printf("Netif is not initialized.\r\n");
    }
}

void print_netmask_address(struct netif *netif) {
    if (netif != NULL) {
        ip_addr_t ip = netif->netmask;
        u8_t *ip_bytes = (u8_t *)&ip.addr;
        xil_printf("Netmask Address: %d.%d.%d.%d\r\n",
                   ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    } else {
        xil_printf("Netif is not initialized.\r\n");
    }
}

void print_gw_address(struct netif *netif) {
    if (netif != NULL) {
        ip_addr_t ip = netif->gw;
        u8_t *ip_bytes = (u8_t *)&ip.addr;
        xil_printf("Gateway Address: %d.%d.%d.%d\r\n",
                   ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    } else {
        xil_printf("Netif is not initialized.\r\n");
    }
}

void print_mac(unsigned char* mac_ethernet_address) {
    xil_printf("MAC-адрес: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
               mac_ethernet_address[0], mac_ethernet_address[1],
               mac_ethernet_address[2], mac_ethernet_address[3],
               mac_ethernet_address[4], mac_ethernet_address[5]);
}

void print_iface(struct netif *netif) {
    if (netif != NULL) {
        ip_addr_t ip = netif->ip_addr;
        ip_addr_t netmask = netif->netmask;
        ip_addr_t gw = netif->gw;
        u8_t *ip_bytes = (u8_t *)&ip.addr;
        u8_t *netmask_bytes = (u8_t *)&netmask.addr;
        u8_t *gw_bytes = (u8_t *)&gw.addr;
        xil_printf("IP Address: %d.%d.%d.%d\r\n",
                   ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
        xil_printf("Netmask: %d.%d.%d.%d\r\n",
        		netmask_bytes[0], netmask_bytes[1], netmask_bytes[2], netmask_bytes[3]);
        xil_printf("Gateway: %d.%d.%d.%d\r\n",
        		gw_bytes[0], gw_bytes[1], gw_bytes[2], gw_bytes[3]);
    } else {
        xil_printf("Netif is not initialized.\r\n");
    }
}

void set_ip(char *ip_str) {
    ip_addr_t new_ip;

    if (inet_pton(AF_INET, ip_str, &new_ip.addr) == 1) {
        netif_set_ipaddr(netif, &new_ip);
        xil_printf("IP-адрес установлен: %s\r\n", ip_str);
    } else {
        xil_printf("Ошибка: неверный формат IP-адреса: %s\r\n", ip_str);
    }
}

void set_netmask(char *ip_str) {
    ip_addr_t new_ip;

    if (inet_pton(AF_INET, ip_str, &new_ip.addr) == 1) {
    	netif_set_netmask(netif, &new_ip);
        xil_printf("Netmask-адрес установлен: %s\r\n", ip_str);
    } else {
        xil_printf("Ошибка: неверный формат Netmask-адреса: %s\r\n", ip_str);
    }
}

void set_gw(char *ip_str) {
    ip_addr_t new_ip;

    if (inet_pton(AF_INET, ip_str, &new_ip.addr) == 1) {
    	netif_set_gw(netif, &new_ip);
        xil_printf("Gateway-адрес установлен: %s\r\n", ip_str);
    } else {
        xil_printf("Ошибка: неверный формат Gateway-адреса: %s\r\n", ip_str);
    }
}

