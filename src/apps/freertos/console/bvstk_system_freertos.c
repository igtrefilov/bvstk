#include "apps/freertos/console/bvstk_system_freertos.h"
#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/interfaces/bvstk_clock.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lwip/netif.h"
#include "lwip/def.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
extern struct netif *netif;
extern unsigned char mac_ethernet_address[];
static bvstk_system_t service;
static SemaphoreHandle_t mutex;
static bvstk_status_t current(void *ctx,network_config_t *c,bool *up,bool *link)
{
    (void)ctx;if (!netif) return BVSTK_ERR_NOT_READY;
    memset(c,0,sizeof(*c));
    c->ip_be=lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(&netif->ip_addr)));
    c->netmask_be=lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(&netif->netmask)));
    c->gateway_be=lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(&netif->gw)));
    memcpy(c->mac,mac_ethernet_address,6);
    c->has_ip=c->has_netmask=c->has_gateway=c->has_mac=true;
    *up=netif_is_up(netif)!=0;*link=netif_is_link_up(netif)!=0;return BVSTK_OK;
}
static bvstk_status_t load(void *ctx,network_config_t *c)
{ (void)ctx;return config_store_get_network(c)?BVSTK_OK:BVSTK_ERR_NOT_READY; }
static bvstk_status_t save(void *ctx,const network_config_t *c)
{
    (void)ctx;
    if (!config_store_set_network(c)) return BVSTK_ERR_NOT_READY;
    return config_store_save_network()?BVSTK_OK:BVSTK_ERR_IO;
}
static bvstk_status_t apply(void *ctx,const network_config_t *c)
{
    (void)ctx;if (!netif) return BVSTK_ERR_NOT_READY;
    if (memcmp(mac_ethernet_address,c->mac,6)) return BVSTK_ERR_UNSUPPORTED;
    ip_addr_t ip,mask,gw;
    ip4_addr_set_u32(ip_2_ip4(&ip),lwip_htonl(c->ip_be));
    ip4_addr_set_u32(ip_2_ip4(&mask),lwip_htonl(c->netmask_be));
    ip4_addr_set_u32(ip_2_ip4(&gw),lwip_htonl(c->gateway_be));
    netif_set_ipaddr(netif,&ip);netif_set_netmask(netif,&mask);netif_set_gw(netif,&gw);
    memcpy(mac_ethernet_address,c->mac,6);
    if (netif->hwaddr_len>=6) memcpy(netif->hwaddr,c->mac,6);
    return BVSTK_OK;
}
static bvstk_status_t reboot_system(void *ctx,unsigned delay)
{ (void)ctx;return bvstk_freertos_schedule_reboot(delay); }
static uint64_t uptime(void *ctx) { (void)ctx;return bvstk_platform_now_ms(); }
static int details(void *ctx,char *out,size_t cap,bool *i2c,bool *mdio)
{
    (void)ctx;*i2c=bvstk_runtime_i2c_ready()!=0;*mdio=bvstk_runtime_mdio_ready()!=0;
    return snprintf(out,cap,"{\"tick_rate_hz\":%u,\"heap_free\":%u,\"heap_min_ever\":%u}",
        (unsigned)configTICK_RATE_HZ,(unsigned)xPortGetFreeHeapSize(),(unsigned)xPortGetMinimumEverFreeHeapSize());
}
bvstk_system_t *bvstk_system_freertos(void)
{
    /* Only RAM configuration is read while publishing the immutable port. */
    taskENTER_CRITICAL();
    if (!service.port.os) {
        bvstk_system_port_t port={0};port.os="freertos";port.current=current;port.load=load;port.save=save;
        port.apply=apply;port.reboot=reboot_system;port.uptime=uptime;port.details=details;
        port.capabilities=(bvstk_capabilities_t){BVSTK_PL_HAS_I2C_CORE,BVSTK_PL_HAS_MDIO_CORE,
            BVSTK_PL_HAS_I2C_CORE,BVSTK_PL_RUNTIME_ENABLED,true,true,true,true,true,true,
            BVSTK_PL_SD_CAN_FILESYSTEM,true,true};
        bvstk_system_init(&service,&port);
        service.configured_valid=load(NULL,&service.configured)==BVSTK_OK;
        service.saved=service.configured_valid;
    }
    if (!service.configured_valid)
        service.configured_valid=load(NULL,&service.configured)==BVSTK_OK;
    taskEXIT_CRITICAL();
    return &service;
}
static int output(void *ctx,const void *data,size_t size)
{ return console_stream_write((int)(intptr_t)ctx,data,size); }
int bvstk_system_freertos_command(int argc,char **argv,int fd)
{
    /* Allocate once under a critical section; I/O runs with only this mutex. */
    SemaphoreHandle_t candidate=xSemaphoreCreateMutex();
    taskENTER_CRITICAL();if (!mutex) { mutex=candidate;candidate=NULL; }taskEXIT_CRITICAL();
    if (candidate) vSemaphoreDelete(candidate);
    if (!mutex || xSemaphoreTake(mutex,pdMS_TO_TICKS(5000))!=pdTRUE) return 1;
    bvstk_system_t *s=bvstk_system_freertos();
    const bvstk_output_t sink={output,(void *)(intptr_t)fd};
    int result=bvstk_system_cli(s,argc,argv,&sink);
    xSemaphoreGive(mutex);return result;
}

#include "protocols/http/bvstk_http_ui.h"
static void http_enter(void *ctx) { (void)ctx;(void)xSemaphoreTake(mutex,portMAX_DELAY); }
static void http_leave(void *ctx) { (void)ctx;xSemaphoreGive(mutex); }
int bvstk_system_freertos_http(const http_request_t *r,http_conn_t *c)
{
    SemaphoreHandle_t candidate=xSemaphoreCreateMutex();
    taskENTER_CRITICAL();if(!mutex){mutex=candidate;candidate=NULL;}taskEXIT_CRITICAL();
    if(candidate) vSemaphoreDelete(candidate);
    if(!mutex) return 0;
    const bvstk_guard_t guard={NULL,http_enter,http_leave};
    return bvstk_http_system(bvstk_system_freertos(),&guard,r,c);
}
