#include "services/system/bvstk_system.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    network_config_t actual, disk;
    unsigned saves, applies, reboots, delay;
    bvstk_status_t save_result, apply_result;
} fake_t;
static bvstk_status_t current(void *ctx,network_config_t *out,bool *up,bool *link)
{ *out=((fake_t *)ctx)->actual;*up=true;*link=false;return BVSTK_OK; }
static bvstk_status_t load(void *ctx,network_config_t *out)
{ *out=((fake_t *)ctx)->disk;return BVSTK_OK; }
static bvstk_status_t save(void *ctx,const network_config_t *c)
{ fake_t *f=ctx;++f->saves;if (!f->save_result) f->disk=*c;return f->save_result; }
static bvstk_status_t apply(void *ctx,const network_config_t *c)
{ fake_t *f=ctx;++f->applies;if (!f->apply_result) f->actual=*c;return f->apply_result; }
static bvstk_status_t reboot_system(void *ctx,unsigned delay)
{ fake_t *f=ctx;++f->reboots;f->delay=delay;return BVSTK_OK; }
static uint64_t uptime(void *ctx) { (void)ctx;return 1234567890123ULL; }
static int details(void *ctx,char *out,size_t cap,bool *i2c,bool *mdio)
{ (void)ctx;*i2c=false;*mdio=true;return snprintf(out,cap,"{\"pid\":42}"); }
static int output(void *ctx,const void *data,size_t size)
{ (void)ctx;(void)data;return (int)size; }
int main(void)
{
    const char *json="{\"ipv4\":{\"ip\":\"192.168.0.10\",\"netmask\":\"255.255.255.0\",\"gateway\":\"192.168.0.1\"},\"mac\":\"00:0a:35:00:01:02\"}";
    fake_t f={0};assert(bvstk_network_parse(json,&f.actual)==BVSTK_OK);f.disk=f.actual;
    char buffer[1600];network_config_t parsed;
    assert(bvstk_network_format(&f.actual,buffer,sizeof(buffer))>0);
    assert(bvstk_network_parse(buffer,&parsed)==BVSTK_OK);
    assert(parsed.ip_be==f.actual.ip_be && !memcmp(parsed.mac,f.actual.mac,6));
    const char *invalid[]={"{}","{\"ipv4\":{},\"mac\":\"00:00:00:00:00:01\"}",
      "{\"ipv4\":{\"ip\":\"1.2.3.4\",\"ip\":\"5.6.7.8\"},\"mac\":\"00:00:00:00:00:01\"}"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) assert(bvstk_network_parse(invalid[i],&parsed)!=BVSTK_OK);
    uint32_t address;uint8_t mac[6];
    assert(!bvstk_network_ipv4("1.2.3.4oops",&address));assert(!bvstk_network_ipv4("1.2.3.256",&address));
    assert(!bvstk_network_ipv4("1.2.3.-1",&address));assert(!bvstk_network_ipv4("1.2.3.4.5",&address));
    assert(!bvstk_network_mac("00:0a:35:00:01:02;id",mac));
    parsed=f.actual;parsed.netmask_be=0xff00ff00U;assert(bvstk_network_validate(&parsed)==BVSTK_ERR_RANGE);
    parsed=f.actual;parsed.mac[0]=1;assert(bvstk_network_validate(&parsed)==BVSTK_ERR_RANGE);
    bvstk_system_port_t port={0};port.context=&f;port.os="neutrino";port.current=current;
    port.load=load;port.save=save;port.apply=apply;port.reboot=reboot_system;port.uptime=uptime;port.details=details;
    port.capabilities.i2c=port.capabilities.mdio=port.capabilities.network_config=port.capabilities.reboot=true;
    bvstk_system_t s;bvstk_system_init(&s,&port);
    assert(bvstk_system_restore(&s)==BVSTK_OK && f.applies==1 && f.saves==0);
    network_config_t next=f.actual;next.ip_be++;
    bvstk_network_result_t result;
    assert(bvstk_system_network_set(&s,&next,false,&result)==BVSTK_OK);
    assert(result.saved&&!result.applied&&f.actual.ip_be!=f.disk.ip_be&&f.applies==1);
    bvstk_system_t restarted;bvstk_system_init(&restarted,&port);
    assert(bvstk_system_restore(&restarted)==BVSTK_OK && f.actual.ip_be==next.ip_be);
    f.save_result=BVSTK_ERR_IO;next.ip_be++;
    assert(bvstk_system_network_set(&s,&next,true,&result)==BVSTK_ERR_IO);
    assert(!result.saved&&result.applied&&f.actual.ip_be==next.ip_be);
    assert(s.configured.ip_be==next.ip_be&&!s.saved);
    f.save_result=BVSTK_OK;f.apply_result=BVSTK_ERR_IO;next.ip_be++;
    assert(bvstk_system_network_set(&s,&next,true,&result)==BVSTK_ERR_IO);
    assert(result.saved&&!result.applied&&f.actual.ip_be!=next.ip_be&&f.disk.ip_be==next.ip_be);
    unsigned saves=f.saves,applies=f.applies;
    parsed=next;parsed.has_mac=false;
    assert(bvstk_system_network_set(&s,&parsed,true,&result)==BVSTK_ERR_MALFORMED);
    assert(f.saves==saves&&f.applies==applies);
    assert(bvstk_system_runtime_json(&s,buffer,sizeof(buffer))>0);
    assert(strstr(buffer,"1234567890123")&&strstr(buffer,"\"i2c\":true")&&strstr(buffer,"\"readiness\":{\"i2c\":false"));
    assert(!strstr(buffer,"heap_free")&&!strstr(buffer,"tick_rate"));
    assert(bvstk_system_runtime_json(&s,buffer,16)<0);
    assert(bvstk_system_network_json(&s,buffer,sizeof(buffer))>0&&strstr(buffer,"\"configured\""));
    bvstk_output_t sink={output,NULL};
    char *bad[]={"reboot","-y","5001"},*good[]={"reboot","confirm","0"},*unconfirmed[]={"reboot"};
    assert(bvstk_system_cli(&s,1,unconfirmed,&sink)==2&&f.reboots==0);
    assert(bvstk_system_cli(&s,3,bad,&sink)==2&&f.reboots==0);
    assert(bvstk_system_cli(&s,3,good,&sink)==0&&f.reboots==1&&f.delay==0);
    char *invalid_prefix[]={"ip","addr","set","192.168.0.10/24junk"};
    assert(bvstk_system_cli(&s,4,invalid_prefix,&sink)==2);
    assert(f.saves==saves&&f.applies==applies);
    puts("System: strict network format, RAM/disk/current, independent save/apply failures, restart, capabilities/readiness and confirmed reboot passed");
    return 0;
}
