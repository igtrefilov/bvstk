#include "shared/protocols/http/bvstk_http_input.h"
#include "services/system/bvstk_system.h"
#include "shared/interfaces/bvstk_guard.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
static const char *field(const char *json,const char *key)
{
    char tag[40];snprintf(tag,sizeof(tag),"\"%s\"",key);
    const char *p=strstr(json,tag);if(!p) return NULL;
    p+=strlen(tag);while(isspace((unsigned char)*p))++p;
    if(*p++!=':') return NULL;
    while(isspace((unsigned char)*p))++p;
    return p;
}
static int http_json_true(const char *json,const char *key)
{ const char *p=field(json,key);return p&&!strncmp(p,"true",4); }
static int http_json_uint(const char *json,const char *key,uint32_t max,unsigned *out)
{
    const char *p=field(json,key);char number[16];size_t n=0;
    if(!p) return 0;
    while(*p>='0'&&*p<='9'&&n+1<sizeof(number)) number[n++]=*p++;
    number[n]=0;while(isspace((unsigned char)*p))++p;
    uint32_t value;if((*p!=','&&*p!='}')||!bvstk_http_uint(number,max,&value)) return 0;
    *out=value;return 1;
}

#include "protocols/http/bvstk_http_ui.h"
#include "shared/protocols/http/bvstk_http_input.h"
static int text_field(const char *json,const char *key,char *out,size_t cap)
{
    const char *p=field(json,key);size_t n=0;
    if(!p||*p++!='"') return 0;
    while(*p&&*p!='"') { if(*p=='\\'||n+1>=cap)return 0;out[n++]=*p++; }
    out[n]=0;return *p=='"';
}
static bvstk_status_t network_body(const char *json,network_config_t *cfg)
{
    char ip[40],mask[24],gw[24],mac[24];unsigned prefix;
    memset(cfg,0,sizeof(*cfg));
    if(!text_field(json,"ip",ip,sizeof(ip))||!text_field(json,"gateway",gw,sizeof(gw))||!text_field(json,"mac",mac,sizeof(mac)))return BVSTK_ERR_MALFORMED;
    char *slash=strchr(ip,'/');
    if(slash) { *slash++=0;uint32_t n;if(!bvstk_http_uint(slash,32,&n))return BVSTK_ERR_MALFORMED;prefix=n;cfg->netmask_be=prefix?UINT32_MAX<<(32-prefix):0; }
    else if(text_field(json,"netmask",mask,sizeof(mask))) { if(!bvstk_network_ipv4(mask,&cfg->netmask_be))return BVSTK_ERR_MALFORMED; }
    else if(http_json_uint(json,"prefix",32,&prefix))cfg->netmask_be=prefix?UINT32_MAX<<(32-prefix):0;
    else return BVSTK_ERR_MALFORMED;
    if(!bvstk_network_ipv4(ip,&cfg->ip_be)||!bvstk_network_ipv4(gw,&cfg->gateway_be)||!bvstk_network_mac(mac,cfg->mac))return BVSTK_ERR_MALFORMED;
    cfg->has_ip=cfg->has_netmask=cfg->has_gateway=cfg->has_mac=true;
    return bvstk_network_validate(cfg);
}
int bvstk_http_system(bvstk_system_t *service,const bvstk_guard_t *guard,const http_request_t *r,http_conn_t *c)
{
    bool net=!strcmp(r->path,"/api/net"),runtime=!strcmp(r->path,"/api/runtime"),restart_request=!strcmp(r->path,"/api/reboot");
    if(!net&&!runtime&&!restart_request) return 0;
    char json[2048];int n=-1;
    if(!strcmp(r->method,"GET")&&!restart_request) {
        guard->enter(guard->context);
        if(runtime) n=bvstk_system_runtime_json(service,json,sizeof(json));
        else {
            network_config_t cfg;bool up,link;
            if(service->port.current(service->port.context,&cfg,&up,&link)==BVSTK_OK) {
                char ip[16],mask[16],gw[16];bvstk_network_address(cfg.ip_be,ip);bvstk_network_address(cfg.netmask_be,mask);bvstk_network_address(cfg.gateway_be,gw);
                n=snprintf(json,sizeof(json),"{\"ip\":\"%s\",\"netmask\":\"%s\",\"gateway\":\"%s\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"mode\":\"static\",\"dhcp\":false,\"up\":%s,\"link_up\":%s}",ip,mask,gw,cfg.mac[0],cfg.mac[1],cfg.mac[2],cfg.mac[3],cfg.mac[4],cfg.mac[5],up?"true":"false",link?"true":"false");
            }
        }
        guard->leave(guard->context);
    } else if(!strcmp(r->method,"PUT")&&!runtime) {
        if(!http_ui_body(r,c,json,sizeof(json))) return 1;
        /* The shared strict JSON validator rejects duplicate/unknown fields. */
        if(restart_request) {
            static const char *const keys[]={"confirm","delay_ms"};
            if(!bvstk_http_json_object(json,keys,2,1)||!http_json_true(json,"confirm")) { http_ui_error(c->fd,400,"confirmation required");return 1; }
            unsigned delay=1000;char *p=strstr(json,"delay_ms");if(p) { p=strchr(p,':');if(!p||!http_json_uint(json,"delay_ms",5000,&delay)) { http_ui_error(c->fd,400,"invalid delay");return 1; } }
            guard->enter(guard->context);bvstk_status_t st=service->port.reboot(service->port.context,delay);guard->leave(guard->context);
            if(st!=BVSTK_OK) { http_ui_status(c->fd,st);return 1; }
            strcpy(json,"{\"ok\":true,\"rebooting\":true}");n=(int)strlen(json);
        } else {
            /* Extract apply separately; the remaining network object is parsed by the shared codec. */
            static const char *const keys[]={"ip","netmask","gateway","mac","apply","prefix"};
            if(!bvstk_http_json_object(json,keys,6,13)) { http_ui_error(c->fd,400,"invalid network object");return 1; }
            bool live=true;char *p=strstr(json,"\"apply\"");
            if(p) {
                char *v=strchr(p,':')+1;while(*v==' '||*v=='\t'||*v=='\r'||*v=='\n')++v;
                if(!strncmp(v,"false",5)) live=false;else if(strncmp(v,"true",4)) { http_ui_error(c->fd,400,"invalid apply flag");return 1; }
                char *end=v+(live?4:5);while(*end==' '||*end=='\t'||*end=='\r'||*end=='\n')++end;
                char *start=p;while(start>json&&start[-1]==' ')--start;
                if(start>json&&start[-1]==',') --start;
                else if(*end==',')++end;
                memmove(start,end,strlen(end)+1);
            }
            network_config_t cfg;bvstk_network_result_t result;
            if(network_body(json,&cfg)!=BVSTK_OK) { http_ui_error(c->fd,400,"invalid network values");return 1; }
            guard->enter(guard->context);(void)bvstk_system_network_set(service,&cfg,live,&result);guard->leave(guard->context);
            n=snprintf(json,sizeof(json),"{\"ok\":true,\"saved\":%s,\"applied\":%s,\"save_status\":\"%s\",\"apply_status\":\"%s\"}",result.saved?"true":"false",result.applied?"true":"false",bvstk_status_string(result.save_status),bvstk_status_string(result.apply_status));
        }
    } else { http_ui_error(c->fd,405,"unsupported method");return 1; }
    if(n<0||(size_t)n>=sizeof(json)) http_ui_error(c->fd,503,"system unavailable");
    else { http_ui_header(c->fd,"application/json",n);http_ui_text(c->fd,json); }
    return 1;
}
