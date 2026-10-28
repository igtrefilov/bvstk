#define main system_contract_main
#include "test_system.c"
#undef main
#include "protocols/http/bvstk_http_ui.h"
static char reply[8192];static size_t reply_size;static unsigned held;
int http_server_write(int fd,const void *p,size_t n)
{ (void)fd;assert(!held);assert(reply_size+n<sizeof(reply));memcpy(reply+reply_size,p,n);reply_size+=n;reply[reply_size]=0;return (int)n; }
int http_ui_legacy_i2c(const http_request_t *r,http_conn_t *c) { (void)r;(void)c;return 0; }
int http_ui_bus(const http_request_t *r,http_conn_t *c) { (void)r;(void)c;return 0; }
int http_ui_files(const http_request_t *r,http_conn_t *c) { (void)r;(void)c;return 0; }
int http_ui_observe(const http_request_t *r,http_conn_t *c) { (void)r;(void)c;return 0; }
static void enter(void *p) { (void)p;assert(!held);++held; }
static void leave(void *p) { (void)p;assert(held);--held; }
static int body(void *ctx,void *out,size_t cap)
{ const char **p=ctx;size_t n=strlen(*p);if(n>cap)n=cap;memcpy(out,*p,n);*p+=n;return (int)n; }
static void request(bvstk_system_t *s,const char *path,const char *json,int status)
{
    const bvstk_guard_t guard={NULL,enter,leave};const char *input=json?json:"";
    http_request_t r={0};r.method=json?"PUT":"GET";r.path=path;r.has_content_length=json!=NULL;r.content_length=strlen(input);
    http_conn_t c={1,body,&input};reply_size=0;
    assert(bvstk_http_system(s,&guard,&r,&c));char expected[32];snprintf(expected,sizeof(expected),"HTTP/1.0 %d",status);assert(strstr(reply,expected));assert(!held);
}
int main(void)
{
    fake_t f={0};assert(bvstk_network_ipv4("192.168.0.10",&f.actual.ip_be));assert(bvstk_network_ipv4("255.255.255.0",&f.actual.netmask_be));assert(bvstk_network_mac("00:0a:35:02:78:11",f.actual.mac));f.actual.has_ip=f.actual.has_netmask=f.actual.has_gateway=f.actual.has_mac=true;f.disk=f.actual;
    bvstk_system_port_t p={0};p.context=&f;p.os="neutrino";p.current=current;p.load=load;p.save=save;p.apply=apply;p.reboot=reboot_system;p.uptime=uptime;p.details=details;
    bvstk_system_t s;bvstk_system_init(&s,&p);
    request(&s,"/api/runtime",NULL,200);assert(strstr(reply,"neutrino"));
    request(&s,"/api/net",NULL,200);assert(strstr(reply,"192.168.0.10"));
    request(&s,"/api/reboot","{\"confirm\":false,\"delay_ms\":true}",400);assert(!f.reboots);
    request(&s,"/api/reboot","{\"confirm\":true,\"delay_ms\":-1}",400);
    request(&s,"/api/reboot","{\"confirm\":true,\"delay_ms\":5001}",400);
    request(&s,"/api/reboot","{\"confirm\":true,\"confirm\":false}",400);
    request(&s,"/api/reboot","{\"confirm\":true,\"delay_ms\":123}",200);assert(f.reboots==1&&f.delay==123);
    request(&s,"/api/net","{\"ip\":\"192.168.0.11\",\"netmask\":\"255.255.255.0\",\"gateway\":\"0.0.0.0\",\"mac\":\"00:0a:35:02:78:11\",\"apply\":false}",200);assert(f.saves==1&&!f.applies);assert(strstr(reply,"\"applied\":false"));
    request(&s,"/api/net","{\"apply\":false,\"ip\":\"192.168.0.12\",\"netmask\":\"255.255.255.0\",\"gateway\":\"0.0.0.0\",\"mac\":\"00:0a:35:02:78:11\"}",200);assert(f.saves==2&&!f.applies);
    request(&s,"/api/net","{\"ip\":\"bad\",\"netmask\":\"255.255.255.0\",\"gateway\":\"0.0.0.0\",\"mac\":\"00:0a:35:02:78:11\"}",400);assert(f.saves==2);
    puts("Shared HTTP system: strict inputs, explicit reboot, save-only, runtime and network I/O outside guards passed");
}
