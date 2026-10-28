#include "services/system/bvstk_system.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <inttypes.h>

int bvstk_network_ipv4(const char *s, uint32_t *out)
{
    uint32_t value = 0;
    if (!s || !out) return 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned n = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            if (++digits > 3) return 0;
            n = n * 10 + (unsigned)(*s++ - '0');
        }
        if (!digits || n > 255 || (i < 3 ? *s++ != '.' : *s != 0)) return 0;
        value = (value << 8) | n;
    }
    *out = value; return 1;
}
int bvstk_network_mac(const char *s, uint8_t mac[6])
{
    if (!s || strlen(s) != 17) return 0;
    uint8_t result[6];
    for (unsigned i = 0; i < 6; ++i) {
        unsigned n = 0;
        for (unsigned j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)*s++;
            if (!isxdigit(c)) return 0;
            n = n * 16 + (isdigit(c) ? (unsigned)(c - '0') : (unsigned)tolower(c) - 'a' + 10);
        }
        result[i] = (uint8_t)n;
        if (i < 5 && *s++ != ':') return 0;
    }
    memcpy(mac, result, 6); return 1;
}
void bvstk_network_address(uint32_t a, char out[16])
{ (void)snprintf(out, 16, "%u.%u.%u.%u", (unsigned)(a>>24), (unsigned)((a>>16)&255), (unsigned)((a>>8)&255), (unsigned)(a&255)); }
bvstk_status_t bvstk_network_validate(const network_config_t *c)
{
    if (!c || !c->has_ip || !c->has_netmask || !c->has_gateway || !c->has_mac)
        return BVSTK_ERR_MALFORMED;
    uint32_t inverse = ~c->netmask_be;
    unsigned nonzero = 0;
    for (unsigned i = 0; i < 6; ++i) nonzero |= c->mac[i];
    if ((inverse & (inverse + 1U)) || !nonzero || (c->mac[0]&1U) ||
        !c->ip_be || (c->ip_be >> 24) >= 224U) return BVSTK_ERR_RANGE;
    return BVSTK_OK;
}
/* Grammar/duplicate checking precedes extraction; values contain no escapes. */
static int string_value(const char *json, const char *key, char *out, size_t cap)
{
    char pattern[32]; (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern); while (isspace((unsigned char)*p)) ++p;
    if (*p++ != ':') return 0;
    while (isspace((unsigned char)*p)) ++p;
    if (*p++ != '"') return 0;
    size_t n = 0;
    while (*p && *p != '"') {
        if (*p == '\\' || n + 1 >= cap) return 0;
        out[n++] = *p++;
    }
    out[n] = 0; return *p == '"';
}
bvstk_status_t bvstk_network_parse(const char *json, network_config_t *out)
{
    static const char *const keys[] = {"ipv4", "mac"};
    if (!json || !out || !bvstk_http_json_object(json, keys, 2, 3)) return BVSTK_ERR_MALFORMED;
    const char *ipv4 = strstr(json, "\"ipv4\"");
    if (!ipv4 || !(ipv4 = strchr(ipv4, ':'))) return BVSTK_ERR_MALFORMED;
    while (isspace((unsigned char)*++ipv4)) {}
    if (*ipv4 != '{') return BVSTK_ERR_MALFORMED;
    const char *end = strchr(ipv4, '}');
    char nested[256];
    if (!end || (size_t)(end-ipv4+1) >= sizeof(nested)) return BVSTK_ERR_MALFORMED;
    memcpy(nested, ipv4, (size_t)(end-ipv4+1)); nested[end-ipv4+1] = 0;
    static const char *const fields[] = {"ip", "netmask", "gateway"};
    if (!bvstk_http_json_object(nested, fields, 3, 7)) return BVSTK_ERR_MALFORMED;
    network_config_t c = {0}; char ip[32], mask[32], gw[32], mac[32];
    if (!string_value(nested,"ip",ip,sizeof(ip)) || !string_value(nested,"netmask",mask,sizeof(mask)) ||
        !string_value(nested,"gateway",gw,sizeof(gw)) || !string_value(json,"mac",mac,sizeof(mac)) ||
        !bvstk_network_ipv4(ip,&c.ip_be) || !bvstk_network_ipv4(mask,&c.netmask_be) ||
        !bvstk_network_ipv4(gw,&c.gateway_be) || !bvstk_network_mac(mac,c.mac)) return BVSTK_ERR_MALFORMED;
    c.has_ip = c.has_netmask = c.has_gateway = c.has_mac = true;
    bvstk_status_t status = bvstk_network_validate(&c);
    if (status == BVSTK_OK) *out = c;
    return status;
}
int bvstk_network_format(const network_config_t *c, char *out, size_t cap)
{
    char ip[16], mask[16], gw[16];
    if (bvstk_network_validate(c) != BVSTK_OK) return -1;
    bvstk_network_address(c->ip_be,ip); bvstk_network_address(c->netmask_be,mask); bvstk_network_address(c->gateway_be,gw);
    int n = snprintf(out,cap,"{\"ipv4\":{\"ip\":\"%s\",\"netmask\":\"%s\",\"gateway\":\"%s\"},\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",
        ip,mask,gw,c->mac[0],c->mac[1],c->mac[2],c->mac[3],c->mac[4],c->mac[5]);
    return n >= 0 && (size_t)n < cap ? n : -1;
}
void bvstk_system_init(bvstk_system_t *s, const bvstk_system_port_t *port)
{ memset(s,0,sizeof(*s)); s->port = *port; }
bvstk_status_t bvstk_system_restore(bvstk_system_t *s)
{
    network_config_t c;
    bvstk_status_t st = s->port.load ? s->port.load(s->port.context,&c) : BVSTK_ERR_UNSUPPORTED;
    if (st != BVSTK_OK) return st;
    st = bvstk_network_validate(&c); if (st != BVSTK_OK) return st;
    s->configured = c; s->configured_valid = s->saved = s->saved_known = true;
    return s->port.apply ? s->port.apply(s->port.context,&c) : BVSTK_ERR_UNSUPPORTED;
}
bvstk_status_t bvstk_system_network_set(bvstk_system_t *s, const network_config_t *c, bool apply, bvstk_network_result_t *r)
{
    if (!s || !r) return BVSTK_ERR_MALFORMED;
    memset(r,0,sizeof(*r));
    bvstk_status_t st = bvstk_network_validate(c);
    if (st != BVSTK_OK) return st;
    s->configured = *c; s->configured_valid = true; s->saved = false; s->saved_known = true;
    r->save_status = s->port.save ? s->port.save(s->port.context,c) : BVSTK_ERR_UNSUPPORTED;
    r->saved = s->saved = r->save_status == BVSTK_OK;
    r->apply_status = BVSTK_OK;
    if (apply) {
        r->apply_status = s->port.apply ? s->port.apply(s->port.context,c) : BVSTK_ERR_UNSUPPORTED;
        r->applied = r->apply_status == BVSTK_OK;
    }
    return r->save_status != BVSTK_OK ? r->save_status : r->apply_status;
}
int bvstk_system_runtime_json(bvstk_system_t *s, char *out, size_t cap)
{
    char details[256] = "{}"; bool i2c = false, mdio = false;
    if (s->port.details && s->port.details(s->port.context,details,sizeof(details),&i2c,&mdio) < 0) return -1;
    const bvstk_capabilities_t *c = &s->port.capabilities;
#define B(field) (c->field ? "true" : "false")
    int n = snprintf(out,cap,"{\"schema_version\":1,\"os\":\"%s\",\"uptime_ms\":%" PRIu64 ",\"capabilities\":{"
        "\"i2c\":%s,\"mdio\":%s,\"presence_poll\":%s,\"acquisition\":%s,\"cache\":%s,\"observe\":%s,"
        "\"dcp2_fs\":%s,\"dcp2_notify\":%s,\"dcp2_stream\":%s,\"http\":%s,\"sd_pl\":%s,\"network_config\":%s,\"reboot\":%s},"
        "\"readiness\":{\"i2c\":%s,\"mdio\":%s},\"os_details\":%s}",s->port.os,
        s->port.uptime ? s->port.uptime(s->port.context) : 0,
        B(i2c),B(mdio),B(presence_poll),B(acquisition),B(cache),B(observe),B(dcp2_fs),B(dcp2_notify),B(dcp2_stream),
        B(http),B(sd_pl),B(network_config),B(reboot),i2c?"true":"false",mdio?"true":"false",details);
#undef B
    return n >= 0 && (size_t)n < cap ? n : -1;
}
int bvstk_system_network_json(bvstk_system_t *s, char *out, size_t cap)
{
    network_config_t c = {0}; bool up=false,link=false;
    bvstk_status_t st = s->port.current ? s->port.current(s->port.context,&c,&up,&link) : BVSTK_ERR_UNSUPPORTED;
    char current[256]="null",configured[256]="null";
    if (st == BVSTK_OK && bvstk_network_format(&c,current,sizeof(current)) < 0) return -1;
    if (s->configured_valid && bvstk_network_format(&s->configured,configured,sizeof(configured)) < 0) return -1;
    int n=snprintf(out,cap,"{\"current\":%s,\"configured\":%s,\"saved\":%s,\"up\":%s,\"link_up\":%s,\"status\":\"%s\"}",
        current,configured,s->saved_known?(s->saved?"true":"false"):"null",up?"true":"false",link?"true":"false",bvstk_status_string(st));
    return n>=0 && (size_t)n<cap?n:-1;
}
static int emit(const bvstk_output_t *out, const char *fmt,...)
{
    char text[1536]; va_list args; va_start(args,fmt);
    int n=vsnprintf(text,sizeof(text),fmt,args); va_end(args);
    return n<0 || (size_t)n>=sizeof(text) || bvstk_output_write(out,text,(size_t)n)!=n ? 1:0;
}
int bvstk_system_cli(bvstk_system_t *s,int argc,char **v,const bvstk_output_t *out)
{
    if (!s || argc<1) return 2;
    if (!strcasecmp(v[0],"system")) {
        if (argc==2 && (!strcasecmp(v[1],"runtime") || !strcasecmp(v[1],"network"))) {
            char json[1400]; int n = !strcasecmp(v[1],"runtime") ? bvstk_system_runtime_json(s,json,sizeof(json)) : bvstk_system_network_json(s,json,sizeof(json));
            return n<0 ? 1 : emit(out,"%s\r\n",json);
        }
        if ((argc==7 || argc==8) && !strcasecmp(v[1],"network") && !strcasecmp(v[2],"set")) {
            network_config_t c={0};bvstk_network_result_t result;
            if (!bvstk_network_ipv4(v[3],&c.ip_be)||!bvstk_network_ipv4(v[4],&c.netmask_be)||
                !bvstk_network_ipv4(v[5],&c.gateway_be)||!bvstk_network_mac(v[6],c.mac)||
                (argc==8&&strcasecmp(v[7],"save-only"))) goto usage;
            c.has_ip=c.has_netmask=c.has_gateway=c.has_mac=true;
            bvstk_status_t st=bvstk_system_network_set(s,&c,argc==7,&result);
            (void)emit(out,"%s saved=%s applied=%s save_status=%s apply_status=%s\r\n",
                st==BVSTK_OK?"OK":"ERR",result.saved?"true":"false",result.applied?"true":"false",
                bvstk_status_string(result.save_status),bvstk_status_string(result.apply_status));
            return st!=BVSTK_OK;
        }
    } else if (!strcasecmp(v[0],"reboot")) {
        uint32_t delay=1000;
        if (argc>=2 && (!strcasecmp(v[1],"-y") || !strcasecmp(v[1],"--yes") || !strcasecmp(v[1],"confirm")) &&
            (argc==2 || (argc==3 && bvstk_http_uint(v[2],5000,&delay)))) {
            bvstk_status_t st=s->port.reboot?s->port.reboot(s->port.context,delay):BVSTK_ERR_UNSUPPORTED;
            (void)emit(out,"%s\r\n",st==BVSTK_OK?"OK (rebooting)":bvstk_status_string(st));return st!=BVSTK_OK;
        }
    } else if (!strcasecmp(v[0],"ip")) {
        network_config_t c={0};bool up=false,link=false;
        if (!s->port.current || s->port.current(s->port.context,&c,&up,&link)!=BVSTK_OK) { (void)emit(out,"ERR NOT_READY\r\n");return 1; }
        if (argc==1 || (argc>=2 && (!strcasecmp(v[1],"addr") || !strcasecmp(v[1],"a") || !strcasecmp(v[1],"address")))) {
            if (argc==1 || argc==2 || (argc==3 && !strcasecmp(v[2],"show"))) {
                char ip[16],mask[16],gw[16];unsigned prefix=0;uint32_t m=c.netmask_be;
                while (m&0x80000000U) { ++prefix;m<<=1; }
                bvstk_network_address(c.ip_be,ip);bvstk_network_address(c.netmask_be,mask);bvstk_network_address(c.gateway_be,gw);
                return emit(out,"1: eth0: <%s>\r\n    link/ether %02x:%02x:%02x:%02x:%02x:%02x\r\n    inet %s/%u\r\n    netmask %s\r\n    gateway %s\r\n",up?"UP":"DOWN",c.mac[0],c.mac[1],c.mac[2],c.mac[3],c.mac[4],c.mac[5],ip,prefix,mask,gw);
            }
            if (argc==4 && (!strcasecmp(v[2],"set") || !strcasecmp(v[2],"add"))) {
                char value[40];if (strlen(v[3])>=sizeof(value)) goto usage;
                strcpy(value,v[3]);char *slash=strchr(value,'/');uint32_t prefix;
                if (!slash) goto usage;
                *slash++=0;
                if (!bvstk_network_ipv4(value,&c.ip_be)||!bvstk_http_uint(slash,32,&prefix)) goto usage;
                c.netmask_be=prefix ? UINT32_MAX<<(32-prefix):0;
            } else goto usage;
        } else if (argc>=2 && (!strcasecmp(v[1],"route") || !strcasecmp(v[1],"r"))) {
            if (argc==2 || (argc==3&&!strcasecmp(v[2],"show"))) { char gw[16];bvstk_network_address(c.gateway_be,gw);return emit(out,"default via %s dev eth0\r\n",gw); }
            if (argc!=6 || (strcasecmp(v[2],"set")&&strcasecmp(v[2],"add")) || strcasecmp(v[3],"default") || strcasecmp(v[4],"via") || !bvstk_network_ipv4(v[5],&c.gateway_be)) goto usage;
        } else if (argc>=2 && (!strcasecmp(v[1],"link") || !strcasecmp(v[1],"l"))) {
            if (argc==2 || (argc==3&&!strcasecmp(v[2],"show"))) return emit(out,"1: eth0: <%s>\r\n    link/ether %02x:%02x:%02x:%02x:%02x:%02x\r\n",up?"UP":"DOWN",c.mac[0],c.mac[1],c.mac[2],c.mac[3],c.mac[4],c.mac[5]);
            if (argc!=5 || strcasecmp(v[2],"set") || strcasecmp(v[3],"address") || !bvstk_network_mac(v[4],c.mac)) goto usage;
        } else if (!(argc==2&&!strcasecmp(v[1],"save"))) goto usage;
        bvstk_network_result_t result;
        bvstk_status_t st=bvstk_system_network_set(s,&c,!(argc==2&&!strcasecmp(v[1],"save")),&result);
        (void)emit(out,"%s saved=%s applied=%s save_status=%s apply_status=%s\r\n",st==BVSTK_OK?"OK":"ERR",result.saved?"true":"false",result.applied?"true":"false",bvstk_status_string(result.save_status),bvstk_status_string(result.apply_status));
        return st!=BVSTK_OK;
    }
usage:
    (void)emit(out,"Usage: system runtime|network; system network set IPv4 mask gateway MAC [save-only]; ip addr [show|set IPv4/prefix]; ip route [show|set default via IPv4]; ip link [show|set address MAC]; ip save; reboot -y [0..5000 ms]\r\n");
    return 2;
}
