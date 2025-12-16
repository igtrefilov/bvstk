#include "ip_shell.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "lwip/def.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"

#include "../../config/config_store.h"
 

extern struct netif *netif;
extern unsigned char mac_ethernet_address[];

static void ip_writef(int fd, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    (void)lwip_write(fd, buf, n);
}

static uint32_t ipv4_be_from_lwip(const ip4_addr_t *ip4)
{
    if (!ip4) return 0;
    return lwip_ntohl(ip4_addr_get_u32(ip4));
}

static void ipv4_to_str(uint32_t be, char out[16])
{
    unsigned a = (be >> 24) & 0xffu;
    unsigned b = (be >> 16) & 0xffu;
    unsigned c = (be >> 8) & 0xffu;
    unsigned d = (be >> 0) & 0xffu;
    snprintf(out, 16, "%u.%u.%u.%u", a, b, c, d);
}

static int mask_prefix_from_netmask(const ip4_addr_t *nm)
{
    if (!nm) return 0;
    uint32_t m = ipv4_be_from_lwip(nm);
    int pfx = 0;
    while (pfx < 32 && (m & (1u << (31 - pfx)))) pfx++;
    return pfx;
}

static uint32_t netmask_be_from_prefix(int pfx, int *ok)
{
    if (ok) *ok = 0;
    if (pfx < 0 || pfx > 32) return 0;
    uint32_t m = (pfx == 0) ? 0u : (0xffffffffu << (32 - pfx));
    if (ok) *ok = 1;
    return m;
}

static int parse_ipv4_slash(const char *s, uint32_t *out_ip_be, uint32_t *out_netmask_be)
{
    if (!s || !out_ip_be || !out_netmask_be) return 0;
    char tmp[40];
    strncpy(tmp, s, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *slash = strchr(tmp, '/');
    if (!slash) return 0;
    *slash = '\0';
    const char *ip_s = tmp;
    const char *pfx_s = slash + 1;
    int pfx = atoi(pfx_s);
    int ok = 0;
    uint32_t nm = netmask_be_from_prefix(pfx, &ok);
    if (!ok) return 0;

    unsigned a, b, c, d;
    if (sscanf(ip_s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    uint32_t ip_host = ((a & 0xffu) << 24) | ((b & 0xffu) << 16) | ((c & 0xffu) << 8) | (d & 0xffu);

    *out_ip_be = ip_host;
    *out_netmask_be = nm;
    return 1;
}

static int parse_mac_str(const char *s, uint8_t mac[6])
{
    if (!s || !mac) return 0;
    int v[6];
    if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return 0;
    for (int i = 0; i < 6; ++i) {
        if (v[i] < 0 || v[i] > 255) return 0;
        mac[i] = (uint8_t)v[i];
    }
    return 1;
}

static void show_addr(int fd)
{
    if (!netif) { write_str(fd, "netif: down\r\n"); return; }

    char ipbuf[16], nmbuf[16], gwbuf[16];
    ipv4_to_str(ipv4_be_from_lwip(ip_2_ip4(&netif->ip_addr)), ipbuf);
    ipv4_to_str(ipv4_be_from_lwip(ip_2_ip4(&netif->netmask)), nmbuf);
    ipv4_to_str(ipv4_be_from_lwip(ip_2_ip4(&netif->gw)), gwbuf);
    int pfx = mask_prefix_from_netmask(ip_2_ip4(&netif->netmask));

    ip_writef(fd, "1: eth0: <UP>\r\n");
    ip_writef(fd, "    link/ether %02x:%02x:%02x:%02x:%02x:%02x\r\n",
              mac_ethernet_address[0], mac_ethernet_address[1], mac_ethernet_address[2],
              mac_ethernet_address[3], mac_ethernet_address[4], mac_ethernet_address[5]);
    ip_writef(fd, "    inet %s/%d brd 0.0.0.0 scope global eth0\r\n", ipbuf, pfx);
    ip_writef(fd, "    netmask %s\r\n", nmbuf);
    ip_writef(fd, "    gateway %s\r\n", gwbuf);
}

static void show_route(int fd)
{
    if (!netif) { write_str(fd, "default via 0.0.0.0 dev eth0\r\n"); return; }
    char gwbuf[16];
    ipv4_to_str(ipv4_be_from_lwip(ip_2_ip4(&netif->gw)), gwbuf);
    ip_writef(fd, "default via %s dev eth0\r\n", gwbuf);
}

static void show_link(int fd)
{
    ip_writef(fd, "1: eth0: <UP> mtu 1500\r\n");
    ip_writef(fd, "    link/ether %02x:%02x:%02x:%02x:%02x:%02x\r\n",
              mac_ethernet_address[0], mac_ethernet_address[1], mac_ethernet_address[2],
              mac_ethernet_address[3], mac_ethernet_address[4], mac_ethernet_address[5]);
}

static void apply_and_save(int fd, const network_config_t *cfg, bool apply_runtime)
{
    if (!cfg) { write_str(fd, "ERR\r\n"); return; }

    (void)config_store_set_network(cfg);
    int saved = config_store_save_network();
    if (!saved) {
        write_str(fd, "WARN: failed to save to flash:/config/network.json\r\n");
    }

    if (apply_runtime) {
        write_str(fd, "OK (applying)\r\n");
    } else {
        write_str(fd, "OK\r\n");
    }

    if (apply_runtime && netif) {
        ip_addr_t ipaddr, netmask, gw;
        IP4_ADDR(&ipaddr,
                 (cfg->ip_be >> 24) & 0xff, (cfg->ip_be >> 16) & 0xff,
                 (cfg->ip_be >> 8) & 0xff, (cfg->ip_be >> 0) & 0xff);
        IP4_ADDR(&netmask,
                 (cfg->netmask_be >> 24) & 0xff, (cfg->netmask_be >> 16) & 0xff,
                 (cfg->netmask_be >> 8) & 0xff, (cfg->netmask_be >> 0) & 0xff);
        IP4_ADDR(&gw,
                 (cfg->gateway_be >> 24) & 0xff, (cfg->gateway_be >> 16) & 0xff,
                 (cfg->gateway_be >> 8) & 0xff, (cfg->gateway_be >> 0) & 0xff);
        netif_set_ipaddr(netif, &ipaddr);
        netif_set_netmask(netif, &netmask);
        netif_set_gw(netif, &gw);
        memcpy(mac_ethernet_address, cfg->mac, 6);
        if (netif->hwaddr_len >= 6) memcpy(netif->hwaddr, cfg->mac, 6);
    }
}

static void cmd_ip_addr_set(int fd, const char *ip_with_prefix)
{
    uint32_t ip_be, nm_be;
    if (!parse_ipv4_slash(ip_with_prefix, &ip_be, &nm_be)) {
        write_str(fd, "ERR\r\n");
        return;
    }
    network_config_t cfg;
    if (!config_store_get_network(&cfg)) memset(&cfg, 0, sizeof(cfg));
    cfg.ip_be = ip_be;
    cfg.netmask_be = nm_be;
    cfg.has_ip = true;
    cfg.has_netmask = true;
    if (!cfg.has_gateway) {
        if (netif) {
            cfg.gateway_be = ipv4_be_from_lwip(ip_2_ip4(&netif->gw));
            cfg.has_gateway = true;
        } else {
            cfg.gateway_be = 0;
            cfg.has_gateway = true;
        }
    }
    if (!cfg.has_mac) {
        memcpy(cfg.mac, mac_ethernet_address, 6);
        cfg.has_mac = true;
    }
    apply_and_save(fd, &cfg, true);
}

static void cmd_ip_route_set_default(int fd, const char *gw_str)
{
    unsigned a, b, c, d;
    if (!gw_str || sscanf(gw_str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255) {
        write_str(fd, "ERR\r\n");
        return;
    }
    uint32_t gw_be = ((a & 0xffu) << 24) | ((b & 0xffu) << 16) | ((c & 0xffu) << 8) | (d & 0xffu);
    network_config_t cfg;
    if (!config_store_get_network(&cfg)) memset(&cfg, 0, sizeof(cfg));
    cfg.gateway_be = gw_be;
    cfg.has_gateway = true;
    if (!cfg.has_ip && netif) { cfg.ip_be = ipv4_be_from_lwip(ip_2_ip4(&netif->ip_addr)); cfg.has_ip = true; }
    if (!cfg.has_netmask && netif) { cfg.netmask_be = ipv4_be_from_lwip(ip_2_ip4(&netif->netmask)); cfg.has_netmask = true; }
    if (!cfg.has_mac) { memcpy(cfg.mac, mac_ethernet_address, 6); cfg.has_mac = true; }
    apply_and_save(fd, &cfg, true);
}

static void cmd_ip_link_set_mac(int fd, const char *mac_str)
{
    uint8_t mac[6];
    if (!parse_mac_str(mac_str, mac)) { write_str(fd, "ERR\r\n"); return; }
    network_config_t cfg;
    if (!config_store_get_network(&cfg)) memset(&cfg, 0, sizeof(cfg));
    memcpy(cfg.mac, mac, 6);
    cfg.has_mac = true;
    if (!cfg.has_ip && netif) { cfg.ip_be = ipv4_be_from_lwip(ip_2_ip4(&netif->ip_addr)); cfg.has_ip = true; }
    if (!cfg.has_netmask && netif) { cfg.netmask_be = ipv4_be_from_lwip(ip_2_ip4(&netif->netmask)); cfg.has_netmask = true; }
    if (!cfg.has_gateway && netif) { cfg.gateway_be = ipv4_be_from_lwip(ip_2_ip4(&netif->gw)); cfg.has_gateway = true; }
    apply_and_save(fd, &cfg, true);
}

static void cmd_ip_save_current(int fd)
{
    if (!netif) { write_str(fd, "ERR\r\n"); return; }
    network_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.ip_be = ipv4_be_from_lwip(ip_2_ip4(&netif->ip_addr));
    cfg.netmask_be = ipv4_be_from_lwip(ip_2_ip4(&netif->netmask));
    cfg.gateway_be = ipv4_be_from_lwip(ip_2_ip4(&netif->gw));
    cfg.has_ip = cfg.has_netmask = cfg.has_gateway = true;
    memcpy(cfg.mac, mac_ethernet_address, 6);
    cfg.has_mac = true;
    apply_and_save(fd, &cfg, false);
}

bool ip_handle(char *tok, char **save, int fd, console_session_t *session)
{
    (void)session;
    if (!tok) return false;

    if (strcasecmp(tok, "ip") == 0) {
        char *sub = strtok_r(NULL, " \t", save);
        if (!sub || strcasecmp(sub, "-h") == 0 || strcasecmp(sub, "--help") == 0) {
            ip_help(fd);
            return true;
        }

        if (strcasecmp(sub, "a") == 0 || strcasecmp(sub, "addr") == 0 || strcasecmp(sub, "address") == 0) {
            char *op = strtok_r(NULL, " \t", save);
            if (!op || strcasecmp(op, "show") == 0) { show_addr(fd); return true; }
            if (strcasecmp(op, "set") == 0 || strcasecmp(op, "add") == 0) {
                char *val = strtok_r(NULL, " \t", save);
                if (!val) { write_str(fd, "ERR\r\n"); return true; }
                cmd_ip_addr_set(fd, val);
                return true;
            }
            write_str(fd, "ERR\r\n");
            return true;
        }

        if (strcasecmp(sub, "r") == 0 || strcasecmp(sub, "route") == 0) {
            char *op = strtok_r(NULL, " \t", save);
            if (!op || strcasecmp(op, "show") == 0) { show_route(fd); return true; }
            if (strcasecmp(op, "set") == 0 || strcasecmp(op, "add") == 0) {
                char *dflt = strtok_r(NULL, " \t", save);
                char *via = strtok_r(NULL, " \t", save);
                char *gw = strtok_r(NULL, " \t", save);
                if (!dflt || !via || !gw) { write_str(fd, "ERR\r\n"); return true; }
                if (strcasecmp(dflt, "default") != 0 || strcasecmp(via, "via") != 0) { write_str(fd, "ERR\r\n"); return true; }
                cmd_ip_route_set_default(fd, gw);
                return true;
            }
            write_str(fd, "ERR\r\n");
            return true;
        }

        if (strcasecmp(sub, "l") == 0 || strcasecmp(sub, "link") == 0) {
            char *op = strtok_r(NULL, " \t", save);
            if (!op || strcasecmp(op, "show") == 0) { show_link(fd); return true; }
            if (strcasecmp(op, "set") == 0) {
                char *what = strtok_r(NULL, " \t", save);
                char *val = strtok_r(NULL, " \t", save);
                if (!what || !val) { write_str(fd, "ERR\r\n"); return true; }
                if (strcasecmp(what, "address") != 0) { write_str(fd, "ERR\r\n"); return true; }
                cmd_ip_link_set_mac(fd, val);
                return true;
            }
            write_str(fd, "ERR\r\n");
            return true;
        }

        if (strcasecmp(sub, "save") == 0) { cmd_ip_save_current(fd); return true; }

        write_str(fd, "ERR\r\n");
        return true;
    }

    return false;
}

void ip_help(int fd)
{
    write_str(fd, "ip usage (limited):\r\n");
    write_str(fd, "  ip addr show\r\n");
    write_str(fd, "  ip addr set <IPv4>/<prefix>\r\n");
    write_str(fd, "  ip link show\r\n");
    write_str(fd, "  ip link set address <mac>\r\n");
    write_str(fd, "  ip route show\r\n");
    write_str(fd, "  ip route set default via <gw>\r\n");
    write_str(fd, "  ip save   (persist current runtime netif to flash config)\r\n");
    write_str(fd, "notes:\r\n");
    write_str(fd, "  Changing IP may drop the current TCP session.\r\n");
}
