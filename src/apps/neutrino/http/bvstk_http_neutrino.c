#include "apps/neutrino/http/bvstk_http_neutrino.h"
#include "protocols/http/bvstk_http_ui.h"
#include "shared/protocols/http/bvstk_http_input.h"
#include "apps/neutrino/runtime/bvstk_system_neutrino.h"
#include "ports/neutrino-zynq7000/storage/bvstk_fs_posix.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include <sys/statvfs.h>
#include "hardware/boards/ax7020/bvstk_qspi_layout.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bvstk_neutrino_command_t *owner;
static bvstk_observation_t *journal;
static bvstk_i2c_master_service_t *i2c(void) { return owner->control->i2c; }
static bvstk_mdio_service_t *mdio(void) { return owner->control->mdio; }
static void lock(int m) { if(m) bvstk_neutrino_mdio_runtime_lock(owner->runtime->mdio);else bvstk_neutrino_i2c_runtime_lock(owner->runtime->i2c); }
static void unlock(int m) { if(m) bvstk_neutrino_mdio_runtime_unlock(owner->runtime->mdio);else bvstk_neutrino_i2c_runtime_unlock(owner->runtime->i2c); }
static bvstk_status_t snapshot_locked(int m,size_t id,http_bus_config_t *out)
{
    if(m) return bvstk_mdio_service_get_config(mdio(),id,&out->mdio);
    const i2c_device_config_t *stored=bvstk_neutrino_i2c_config_store_get(&owner->runtime->i2c->config_store,id);
    return stored?bvstk_control_i2c_snapshot(owner->control,id,stored,0,&out->i2c):BVSTK_ERR_NOT_READY;
}
static bvstk_status_t snapshot(int m,size_t id,http_bus_config_t *out)
{ lock(m);bvstk_status_t st=snapshot_locked(m,id,out);unlock(m);return st; }
static bvstk_status_t apply(int m,size_t id,const http_bus_config_t *cfg,const char *revision)
{
    http_bus_config_t *old=malloc(sizeof(*old));char *json=malloc(HTTP_UI_JSON_CAP);
    if(!old||!json) { free(old);free(json);return BVSTK_ERR_INTERNAL; }
    lock(m);bvstk_status_t st=snapshot_locked(m,id,old);size_t n=0;
    if(st==BVSTK_OK) st=m?bvstk_mdio_config_format(&old->mdio,json,HTTP_UI_JSON_CAP,&n):bvstk_i2c_config_serialize_json(&old->i2c,json,HTTP_UI_JSON_CAP,&n);
    char actual[16];snprintf(actual,sizeof(actual),"%08lx",(unsigned long)bvstk_http_revision(st==BVSTK_OK?json:""));
    if(st==BVSTK_OK&&strcmp(actual,revision)) st=BVSTK_ERR_BUSY;
    if(st==BVSTK_OK) {
        if(m) {
            st=bvstk_mdio_service_set_config(mdio(),id,&cfg->mdio);
            if(st==BVSTK_OK&&!bvstk_neutrino_mdio_runtime_sync_locked(owner->runtime->mdio,id)) st=BVSTK_ERR_INTERNAL;
        } else if(!bvstk_neutrino_i2c_runtime_apply_locked(owner->runtime->i2c,id,&cfg->i2c)) st=BVSTK_ERR_IO;
    }
    unlock(m);free(old);free(json);return st;
}
static int save(int m,size_t id,const http_bus_config_t *cfg)
{
    (void)id;
    return m?bvstk_neutrino_mdio_config_store_save(&owner->runtime->mdio->config_store,&cfg->mdio):
        bvstk_neutrino_i2c_config_store_save(&owner->runtime->i2c->config_store,&cfg->i2c);
}
static void sync_device(size_t id)
{ lock(0);(void)bvstk_neutrino_i2c_runtime_sync_locked(owner->runtime->i2c,id);unlock(0); }
static void observe(unsigned b,unsigned c,int address,int device,uint64_t after,bvstk_journal_batch_t *out)
{ bvstk_observation_read(journal,b,c,address,device,after,out); }
static void volumes(int fd)
{
    const bvstk_fs_backend_t *b=owner->storage->reader;
    http_ui_header(fd,"application/json",-1);http_ui_text(fd,"{\"volumes\":[");
    for(size_t i=0;i<b->volume_count(b->context);++i) {
        bvstk_fs_volume_t v={0};(void)b->volume(b->context,i,&v);
        http_ui_printf(fd,"%s{\"name\":",i?",":"");http_ui_string(fd,v.name);
        http_ui_printf(fd,",\"ready\":%s,\"supported\":%s,\"fat_bits\":0,",v.state==1?"true":"false",v.state==2?"false":"true");
        char native[40];struct statvfs info;snprintf(native,sizeof(native),"/%s",v.name);
        if(v.state==1&&!statvfs(native,&info)) {
            uint64_t block=info.f_frsize?info.f_frsize:info.f_bsize;
            http_ui_printf(fd,"\"total_bytes\":%llu,\"free_bytes\":%llu,",(unsigned long long)(block*info.f_blocks),(unsigned long long)(block*info.f_bavail));
        } else http_ui_text(fd,"\"total_bytes\":null,\"free_bytes\":null,");
        http_ui_text(fd,"\"format_allowed\":false}");
    }
    http_ui_text(fd,"]}");
}
static void sd_info(int fd)
{ http_ui_header(fd,"application/json",-1);http_ui_text(fd,"{\"supported\":false,\"ready\":false,\"mounted\":false,\"format_allowed\":false}"); }
const http_ui_backend_t *http_ui_backend(void)
{
    static const http_ui_backend_t b={i2c,mdio,snapshot,apply,save,sync_device,bvstk_fs_posix_default,volumes,sd_info,malloc,free,observe,false};return &b;
}
static const char *mime(const char *p)
{
    const char *ext=strrchr(p,'.');
    if(ext&&!strcmp(ext,".js")) return "text/javascript; charset=utf-8";
    if(ext&&!strcmp(ext,".css")) return "text/css; charset=utf-8";
    if(ext&&!strcmp(ext,".html")) return "text/html; charset=utf-8";
    if(ext&&!strcmp(ext,".svg")) return "image/svg+xml";
    if(ext&&!strcmp(ext,".png")) return "image/png";
    if(ext&&!strcmp(ext,".json")) return "application/json; charset=utf-8";
    return "application/octet-stream";
}
static int diagnostics(const http_request_t *r,http_conn_t *c)
{
    bool mem=!strcmp(r->path,"/api/diag/mem/read")||!strcmp(r->path,"/api/diag/mem/write");
    bool bus=!strcmp(r->path,"/api/diag/i2c/read")||!strcmp(r->path,"/api/diag/i2c/write");
    if(!mem&&!bus)return 0;
    if(strcmp(r->method,"PUT")) { http_ui_error(c->fd,405,"PUT required");return 1; }
    char body[1024];if(!http_ui_body(r,c,body,sizeof(body)))return 1;
    bool write=strstr(r->path,"/write")!=NULL;uint32_t address=0,value=0,reg=0;int confirm=0;
    static const char *const mk[]={"addr","val","confirm"};
    static const char *const ik[]={"name","addr_7b","reg","val"};
    if(!bvstk_http_json_object(body,mem?mk:ik,mem?3:4,mem?(write?7:1):(write?12:4)))goto bad;
    if(mem) {
        if(!http_legacy_uint(body,"addr",&address)||(write&&(!http_legacy_uint(body,"val",&value)||!http_legacy_bool(body,"confirm",&confirm)||!confirm)))goto bad;
        unsigned width=(address&3)?8:32;
        if(write&&width==8&&value>255)goto bad;
        bvstk_status_t st=bvstk_control_mem_check(owner->control,address,width,1,0,write);
        uint64_t result=0;
        if(st==BVSTK_OK)st=write?bvstk_control_mem_write(owner->control,address,width,value):bvstk_control_mem_read(owner->control,address,width,&result);
        if(st!=BVSTK_OK) { http_ui_status(c->fd,st);return 1; }
        http_ui_header(c->fd,"application/json",-1);
        if(write)http_ui_printf(c->fd,"{\"ok\":true,\"width\":%u}",width);
        else http_ui_printf(c->fd,"{\"ok\":true,\"width\":%u,\"val\":%llu}",width,(unsigned long long)result);
        return 1;
    }
    char name[32]={0};size_t id=0;
    if(!http_legacy_uint(body,"reg",&reg)||reg>255||(write&&(!http_legacy_uint(body,"val",&value)||value>255)))goto bad;
    bool named=http_legacy_string(body,"name",name,sizeof(name));
    if(!named&&!http_legacy_uint(body,"addr_7b",&address))goto bad;
    bvstk_status_t st=named?bvstk_i2c_master_service_find_by_name(i2c(),name,&id):
        address<=127?bvstk_i2c_master_service_find_by_addr(i2c(),(uint8_t)address,&id):BVSTK_ERR_RANGE;
    bvstk_i2c_device_t device;
    if(st==BVSTK_OK)st=bvstk_i2c_master_service_device_info(i2c(),id,&device);
    uint8_t index=(uint8_t)reg;
    if(st==BVSTK_OK&&!named&&!bvstk_i2c_address_register(&device.endpoint,device.addr_7b,device.reg_count,address,reg,&index))st=BVSTK_ERR_RANGE;
    uint8_t result=0;
    if(st==BVSTK_OK)st=write?bvstk_i2c_master_service_write(i2c(),id,index,(uint8_t)value,BVSTK_EVENT_SOURCE_WEB,100):
        bvstk_i2c_master_service_read_from(i2c(),id,index,&result,BVSTK_EVENT_SOURCE_WEB,100);
    if(st!=BVSTK_OK) { http_ui_status(c->fd,st);return 1; }
    if(write) { sync_device(id);http_ui_status(c->fd,BVSTK_OK); }
    else { http_ui_header(c->fd,"application/json",-1);http_ui_text(c->fd,"{\"ok\":true,\"name\":");http_ui_string(c->fd,device.name);http_ui_printf(c->fd,",\"reg\":%u,\"val\":%u}",(unsigned)reg,result); }
    return 1;
bad:
    http_ui_error(c->fd,400,"invalid diagnostic request");return 1;
}
int http_handle_request(const http_request_t *r,http_conn_t *c)
{
    if(http_ui_handle(r,c)||bvstk_system_neutrino_http(r,c)||diagnostics(r,c)) return 1;
    if(!strcmp(r->path,"/api/version")&&!strcmp(r->method,"GET")) {
        http_ui_header(c->fd,"application/json",-1);
        http_ui_printf(c->fd,"{\"build_date\":\"%s\",\"build_time\":\"%s\",\"http_port\":80}",__DATE__,__TIME__);return 1;
    }
    if(!strcmp(r->path,"/api/fs")&&!strcmp(r->method,"GET")) { volumes(c->fd);return 1; }
    if(!strncmp(r->path,"/tar/",5)) {
        const char *part=r->path+5;const char *slash=strchr(part,'/');char volume[12],path[BVSTK_FS_PATH_CAPACITY];
        if(!slash||slash-part>=(int)sizeof(volume)) { http_ui_error(c->fd,400,"invalid archive volume");return 1; }
        memcpy(volume,part,(size_t)(slash-part));volume[slash-part]=0;
        if(bvstk_fs_path(volume,slash,path,sizeof(path))!=BVSTK_FS_OK) { http_ui_error(c->fd,400,"invalid archive path");return 1; }
        if(!strcmp(r->method,"PUT")) {
            if(!r->has_content_length&&!r->chunked) { http_ui_error(c->fd,411,"length required");return 1; }
            bvstk_fs_result_t st=bvstk_tar_extract_stream(owner->storage,path,c->read_body,c->read_user);
            if(st==BVSTK_FS_OK) {
                uint8_t padding[1024];int n;
                while((n=c->read_body(c->read_user,padding,sizeof(padding)))>0) {
                    for(int i=0;i<n;++i)if(padding[i])st=BVSTK_FS_INVALID;
                }
                if(n<0)st=BVSTK_FS_IO;
            }
            if(st==BVSTK_FS_OK)http_ui_status(c->fd,BVSTK_OK);else http_ui_error(c->fd,400,"archive extraction failed");return 1;
        }
        if(strcmp(r->method,"GET")) { http_ui_error(c->fd,405,"GET/PUT required");return 1; }
        bvstk_fs_reader_t reader;bvstk_fs_result_t st=bvstk_fs_reader_open(&reader,owner->storage->reader,path,BVSTK_FS_TAR,0);
        if(st!=BVSTK_FS_OK) { http_ui_error(c->fd,404,"archive source unavailable");return 1; }
        http_ui_header(c->fd,"application/x-tar",-1);bool eof=false;uint8_t data[2048];
        while(!eof) { size_t n=0;st=bvstk_fs_reader_read(&reader,data,sizeof(data),&n,&eof);if(st!=BVSTK_FS_OK||(n&&!http_ui_send(c->fd,data,n)))break; }
        bvstk_fs_reader_close(&reader);return 1;
    }
    if(!strcmp(r->path,"/api/qspi")&&!strcmp(r->method,"GET")) {
        bvstk_fs_info_t info;bool ready=bvstk_fs_stat(owner->storage->reader,"flash:/",&info)==BVSTK_FS_OK;
        http_ui_header(c->fd,"application/json",-1);http_ui_printf(c->fd,"{\"ready\":%s,\"fs_base_bytes\":%u,\"fs_size_bytes\":%u}",ready?"true":"false",(unsigned)QSPI_FS_BASE_BYTES,(unsigned)QSPI_FS_SIZE_BYTES);return 1;
    }
    /* Preserve direct file URLs used by the existing resource uploader. */
    for(unsigned i=0;i<2;++i) {
        const char *prefix=i?"/sd/":"/flash/";const char *volume=i?"sd":"flash";
        if(!strncmp(r->path,prefix,strlen(prefix))) {
            char query[2048];const char *relative=r->path+strlen(prefix)-1;size_t at=0;
            int n=snprintf(query,sizeof(query),"/api/ui/file?volume=%s&confirm=true&overwrite=true&path=",volume);
            at=(size_t)n;
            for(;*relative&&*relative!='?'&&at+4<sizeof(query);++relative) {
                unsigned char ch=(unsigned char)*relative;
                if(ch=='%') {
                    if(!relative[1]||!relative[2]||!isxdigit((unsigned char)relative[1])||!isxdigit((unsigned char)relative[2])) { http_ui_error(c->fd,400,"invalid URL encoding");return 1; }
                    memcpy(query+at,relative,3);at+=3;relative+=2;continue;
                }
                query[at++]='%';query[at++]="0123456789ABCDEF"[ch>>4];query[at++]="0123456789ABCDEF"[ch&15];
            }
            if(*relative&&*relative!='?') { http_ui_error(c->fd,400,"path too long");return 1; }
            query[at]=0;http_request_t alias=*r;alias.path=query;return http_ui_files(&alias,c);
        }
    }
    if(strncmp(r->path,"/api/",5)==0) { http_ui_error(c->fd,404,"unknown API endpoint");return 1; }
    if(strcmp(r->method,"GET")) { http_ui_error(c->fd,405,"GET required");return 1; }
    char path[BVSTK_FS_PATH_CAPACITY],relative[BVSTK_FS_PATH_CAPACITY];
    size_t len=strcspn(r->path,"?");
    if(len>=sizeof(relative)) { http_ui_error(c->fd,400,"path too long");return 1; }
    memcpy(relative,r->path,len);relative[len]=0;
    if(!strcmp(relative,"/")) strcpy(relative,"/index.html");
    if(!bvstk_http_path(relative)||snprintf(path,sizeof(path),"flash:/www%s",relative)>=(int)sizeof(path)) {
        http_ui_error(c->fd,400,"invalid path");return 1;
    }
    bvstk_fs_reader_t reader;const bvstk_fs_backend_t *b=owner->storage->reader;
    bvstk_fs_result_t st=bvstk_fs_reader_open(&reader,b,path,BVSTK_FS_FILE,0);
    if(st!=BVSTK_FS_OK) { http_ui_error(c->fd,st==BVSTK_FS_NOT_FOUND?404:503,"resource unavailable");return 1; }
    http_ui_header(c->fd,mime(path),(long long)reader.total_size);
    bool eof=false;uint8_t data[2048];
    while(!eof) { size_t n=0;st=bvstk_fs_reader_read(&reader,data,sizeof(data),&n,&eof);if(st!=BVSTK_FS_OK||(n&&!http_ui_send(c->fd,data,n))) break; }
    bvstk_fs_reader_close(&reader);return 1;
}
int bvstk_http_neutrino_start(bvstk_neutrino_command_t *commands,bvstk_observation_t *events)
{ owner=commands;journal=events;start_http_server();return 1; }
