#include "apps/neutrino/runtime/bvstk_system_neutrino.h"
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "shared/interfaces/bvstk_clock.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <spawn.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern char **environ;
static bvstk_system_t system_service;
static bvstk_neutrino_command_t *commands;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static bool initialized, reboot_pending;
static unsigned reboot_delay;
/* Fixed executables and separately validated arguments; no shell parsing. */
static int execute(const char *path, char *const argv[], char *output, size_t cap)
{
    int pipes[2]={-1,-1};
    if (output && pipe(pipes)) return -1;
    int descriptors[3]={SPAWN_FDCLOSED,
        output?pipes[1]:(fcntl(STDOUT_FILENO,F_GETFD)<0?SPAWN_FDCLOSED:STDOUT_FILENO),
        fcntl(STDERR_FILENO,F_GETFD)<0?SPAWN_FDCLOSED:STDERR_FILENO};
    struct inheritance inherit;
    memset(&inherit,0,sizeof(inherit));
    pid_t pid=spawn(path,3,descriptors,&inherit,argv,environ);
    int rc=pid<0?errno:0;
    if (output) {
        close(pipes[1]); size_t n=0;char discard[256];ssize_t got;
        while ((got=read(pipes[0],n+1<cap?output+n:discard,n+1<cap?cap-n-1:sizeof(discard)))!=0) {
            if (got<0) { if (errno==EINTR) continue;break; }
            if (n+1<cap) n+=(size_t)got;
        }
        close(pipes[0]);output[n]=0;
    }
    if (rc) { fprintf(stderr,"bvstkd: spawn %s: %s\n",path,strerror(rc));return -1; }
    int status;
    while (waitpid(pid,&status,0)<0) if (errno!=EINTR) { fprintf(stderr,"bvstkd: wait %s: %s\n",path,strerror(errno));return -1; }
    return WIFEXITED(status)&&WEXITSTATUS(status)==0?0:-1;
}
static bvstk_status_t current(void *context,network_config_t *c,bool *up,bool *link)
{
    (void)context;memset(c,0,sizeof(*c));*up=*link=false;
    struct ifaddrs *list=NULL;
    if (getifaddrs(&list)) return BVSTK_ERR_IO;
    for (struct ifaddrs *p=list;p;p=p->ifa_next) {
        if (strcmp(p->ifa_name,"xzynq0") || !p->ifa_addr) continue;
        *up=(p->ifa_flags&IFF_UP)!=0;*link=(p->ifa_flags&IFF_RUNNING)!=0;
        if (p->ifa_addr->sa_family==AF_INET) {
            c->ip_be=ntohl(((struct sockaddr_in *)p->ifa_addr)->sin_addr.s_addr);c->has_ip=true;
            if (p->ifa_netmask) { c->netmask_be=ntohl(((struct sockaddr_in *)p->ifa_netmask)->sin_addr.s_addr);c->has_netmask=true; }
        } else if (p->ifa_addr->sa_family==AF_LINK && p->ifa_data) {
            struct sockaddr_dl *d=(struct sockaddr_dl *)p->ifa_addr;
            if (d->sdl_alen==6) { memcpy(c->mac,LLADDR(d),6);c->has_mac=true; }
        }
    }
    freeifaddrs(list);
    char table[8192];char *args[]={"netstat","-rn",NULL};
    if (execute("/proc/boot/netstat",args,table,sizeof(table))) return BVSTK_ERR_IO;
    c->gateway_be=0;c->has_gateway=true;
    for (char *p=table;*p;) {
        char destination[64],gateway[64],interface[64];
        char *next=strchr(p,'\n');if (next) *next=0;
        /* Interface is verified as a separate token before selecting a route. */
        if (sscanf(p,"%63s %63s",destination,gateway)==2 &&
            (!strcmp(destination,"default")||!strcmp(destination,"0.0.0.0"))) {
            bool matches=false;char *save=NULL;
            for (char *token=strtok_r(p," \t",&save);token;token=strtok_r(NULL," \t",&save)) {
                (void)snprintf(interface,sizeof(interface),"%s",token);
                if (!strcmp(interface,"xzynq0")) matches=true;
            }
            if (matches && !bvstk_network_ipv4(gateway,&c->gateway_be)) return BVSTK_ERR_IO;
        }
        if (!next) break;
        p=next+1;
    }
    return c->has_ip&&c->has_netmask&&c->has_mac?BVSTK_OK:BVSTK_ERR_NOT_READY;
}
static bvstk_status_t load(void *context,network_config_t *c)
{
    (void)context;const bvstk_fs_storage_t *fs=bvstk_fs_posix_default();
    if (!fs) return BVSTK_ERR_NOT_READY;
    char json[1024];void *file=NULL;uint64_t size=0;size_t got=0;
    bvstk_fs_result_t st=fs->reader->file_open(fs->reader->context,"flash:/config/network.json",&file,&size);
    if (st!=BVSTK_FS_OK) return st==BVSTK_FS_NOT_FOUND?BVSTK_ERR_NOT_FOUND:BVSTK_ERR_IO;
    if (size>=sizeof(json)) { fs->reader->file_close(fs->reader->context,file);return BVSTK_ERR_RANGE; }
    size_t total=0;
    while (total<(size_t)size) {
        st=fs->reader->file_read(fs->reader->context,file,(uint8_t *)json+total,(size_t)size-total,&got);
        if (st!=BVSTK_FS_OK||!got) break;
        total+=got;
    }
    fs->reader->file_close(fs->reader->context,file);
    if (total!=size) return BVSTK_ERR_IO;
    json[total]=0;return bvstk_network_parse(json,c);
}
static bvstk_status_t save(void *context,const network_config_t *c)
{
    (void)context;char json[256];int n=bvstk_network_format(c,json,sizeof(json));
    const bvstk_fs_storage_t *fs=bvstk_fs_posix_default();
    if (n<0) return BVSTK_ERR_MALFORMED;
    if (!fs) return BVSTK_ERR_NOT_READY;
    if (bvstk_fs_mkdirs(fs,"flash:/config")!=BVSTK_FS_OK) return BVSTK_ERR_IO;
    return bvstk_fs_save(fs,"flash:/config/network.json",json,(size_t)n)==BVSTK_FS_OK?BVSTK_OK:BVSTK_ERR_IO;
}
static bvstk_status_t apply(void *context,const network_config_t *c)
{
    network_config_t old;bool up,link;
    bvstk_status_t st=current(context,&old,&up,&link);
    if (st!=BVSTK_OK) return st;
    /* GEM MAC is configured by the driver at boot, not by a live alias. */
    bool mac_pending=memcmp(old.mac,c->mac,6)!=0;
    char ip[16],mask[16],gw[16],previous[16];
    bvstk_network_address(c->ip_be,ip);bvstk_network_address(c->netmask_be,mask);
    bvstk_network_address(c->gateway_be,gw);bvstk_network_address(old.gateway_be,previous);
    char *address[]={"ifconfig","xzynq0",ip,"netmask",mask,"up",NULL};
    if (execute("/proc/boot/ifconfig",address,NULL,0)) return BVSTK_ERR_IO;
    if (old.gateway_be!=c->gateway_be) {
        if (old.gateway_be) {
            char *del[]={"route","delete","default",previous,NULL};
            if (execute("/proc/boot/route",del,NULL,0)) return BVSTK_ERR_IO;
        }
        if (c->gateway_be) {
            char *add[]={"route","add","default",gw,NULL};
            if (execute("/proc/boot/route",add,NULL,0)) return BVSTK_ERR_IO;
        }
    }
    return mac_pending?BVSTK_ERR_UNSUPPORTED:BVSTK_OK;
}
static void *restart(void *argument)
{
    unsigned delay=*(unsigned *)argument;
    bvstk_platform_sleep_ms(delay);
    char *args[]={"shutdown","-S","reboot","-q",NULL};
    if (execute("/proc/boot/shutdown",args,NULL,0)) fprintf(stderr,"bvstkd: shutdown failed\n");
    pthread_mutex_lock(&mutex);reboot_pending=false;pthread_mutex_unlock(&mutex);return NULL;
}
static bvstk_status_t reboot_system(void *context,unsigned delay)
{
    (void)context;
    if (reboot_pending) return BVSTK_ERR_BUSY;
    pthread_t thread;reboot_delay=delay;reboot_pending=true;
    if (pthread_create(&thread,NULL,restart,&reboot_delay)) { reboot_pending=false;return BVSTK_ERR_INTERNAL; }
    pthread_detach(thread);return BVSTK_OK;
}
static uint64_t uptime(void *context) { (void)context;return bvstk_platform_now_ms(); }
static int details(void *context,char *out,size_t cap,bool *i2c,bool *mdio)
{
    (void)context;
    *i2c=commands&&commands->control&&commands->control->i2c;
    *mdio=commands&&commands->control&&commands->control->mdio;
    return snprintf(out,cap,"{\"pid\":%ld,\"online_cpus\":%ld,\"network_mac_live\":false,\"network_mac_restore\":false}",(long)getpid(),sysconf(_SC_NPROCESSORS_ONLN));
}
int bvstk_system_neutrino_start(bvstk_neutrino_command_t *runtime)
{
    pthread_mutex_lock(&mutex);commands=runtime;
    bvstk_system_port_t port={0};port.os="neutrino";port.current=current;port.load=load;port.save=save;
    port.apply=apply;port.reboot=reboot_system;port.uptime=uptime;port.details=details;
    port.capabilities=(bvstk_capabilities_t){true,true,true,true,true,true,true,true,true,true,true,true,true};
    bvstk_system_init(&system_service,&port);initialized=true;
    bvstk_status_t st=bvstk_system_restore(&system_service);
    if (st!=BVSTK_OK&&st!=BVSTK_ERR_NOT_FOUND) fprintf(stderr,"bvstkd: network restore: %s; current network retained where possible\n",bvstk_status_string(st));
    pthread_mutex_unlock(&mutex);return st==BVSTK_OK||st==BVSTK_ERR_NOT_FOUND;
}
int bvstk_system_neutrino_command(int argc,char **argv,const bvstk_output_t *out)
{
    pthread_mutex_lock(&mutex);
    int result=initialized?bvstk_system_cli(&system_service,argc,argv,out):1;
    pthread_mutex_unlock(&mutex);return result;
}

#include "protocols/http/bvstk_http_ui.h"
static void system_enter(void *context) { (void)context;pthread_mutex_lock(&mutex); }
static void system_leave(void *context) { (void)context;pthread_mutex_unlock(&mutex); }
int bvstk_system_neutrino_http(const http_request_t *r,http_conn_t *c)
{ const bvstk_guard_t guard={NULL,system_enter,system_leave};return bvstk_http_system(&system_service,&guard,r,c); }
