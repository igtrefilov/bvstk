/* Reuse the shell fixture's actual portable services, fake buses and config
 * store. HTTP and shell consequently exercise the same policies. */
#define main shell_fixture_main
#include "test_bus_shell.c"
#undef main
#define DIR NativeDir
#include <dirent.h>
#undef DIR
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "apps/freertos/services/http/http_ui.h"
#include "apps/freertos/runtime/bvstk_observer.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"

static char storage[256];
static int mounted = 1, close_error;
static FATFS fat = {.fs_type=FS_FAT32,.n_fatent=4098,.csize=1};
static fs_shared_ctx_t contexts[] = {{.ready=&mounted,.root="0:/"},{.ready=&mounted,.root="1:/"},{.ready=&mounted,.root="2:/"}};
static fs_device_info_t host_devices[] = {{"flash","flash",&contexts[0]},{"sd","sd",&contexts[1]},{"sd-pl","sd-pl",&contexts[2]}};
void *pvPortMalloc(size_t n) { return malloc(n); }
void vPortFree(void *p) { free(p); }
int http_server_write(int fd, const void *p, size_t n) { if (n > 17) n = 17; return console_stream_write(fd,p,n); }
int xSemaphoreTake(SemaphoreHandle_t s, unsigned t) { (void)s;(void)t;return 1; }
int xSemaphoreGive(SemaphoreHandle_t s) { (void)s;return 1; }
int bvstk_runtime_i2c_ready(void) { return ready; }
int bvstk_runtime_mdio_ready(void) { return ready; }
int fs_shared_is_ready(const fs_shared_ctx_t *c) { return c && *c->ready; }
const fs_device_info_t *fs_device_by_name(const char *name) {
    for (size_t i=0;i<3;i++) if (!strcmp(name,host_devices[i].name)) return &host_devices[i];
    return NULL;
}
static FRESULT failure(void) { return errno == ENOENT ? FR_NO_FILE : errno == EEXIST ? FR_EXIST : FR_DENIED; }
static void native_path(const char *path,char out[512]) {
    assert(path[1] == ':' && path[2] == '/' && path[0] >= '0' && path[0] <= '2');
    assert(!strstr(path,".."));
    snprintf(out,512,"%s/%c%s",storage,path[0],path+2);
}
FRESULT f_open(FIL *f,const char *path,BYTE flags) {
    char p[512]; native_path(path,p); struct stat st;
    int exists = !stat(p,&st);
    if (exists && S_ISDIR(st.st_mode)) return FR_DENIED;
    if ((flags & FA_CREATE_NEW) && exists) return FR_EXIST;
    const char *mode = flags & FA_WRITE ? ((flags & FA_CREATE_NEW) || !exists ? "w+b" : "r+b") : "rb";
    f->native=fopen(p,mode); f->size=exists ? (FSIZE_t)st.st_size : 0;
    return f->native ? FR_OK : failure();
}
FRESULT f_read(FIL *f,void *p,UINT n,UINT *got) { *got=(UINT)fread(p,1,n,f->native); return ferror(f->native) ? FR_DISK_ERR : FR_OK; }
FRESULT f_write(FIL *f,const void *p,UINT n,UINT *got) { *got=(UINT)fwrite(p,1,n,f->native); f->size=(FSIZE_t)ftell(f->native); return *got==n ? FR_OK : FR_DISK_ERR; }
FRESULT f_close(FIL *f) { int r=fclose(f->native); if(close_error){close_error=0;return FR_DISK_ERR;} return r ? FR_DISK_ERR : FR_OK; }
FRESULT f_sync(FIL *f) { return fflush(f->native) ? FR_DISK_ERR : FR_OK; }
FRESULT f_truncate(FIL *f) { f->size=0; return ftruncate(fileno(f->native),0) ? FR_DISK_ERR : FR_OK; }
FRESULT f_stat(const char *path,FILINFO *fi) {
    char p[512]; struct stat st; native_path(path,p);
    if(stat(p,&st)) return failure();
    memset(fi,0,sizeof(*fi));
    snprintf(fi->fname,sizeof(fi->fname),"%s",strrchr(path,'/')+1);
    fi->fsize=(FSIZE_t)st.st_size;fi->fattrib=S_ISDIR(st.st_mode)?AM_DIR:0;return FR_OK;
}
FRESULT f_getfree(const char *p,DWORD *n,FATFS **out) { (void)p;*n=2048;*out=&fat;return FR_OK; }
FRESULT f_opendir(DIR *d,const char *p) { native_path(p,d->path);d->native=opendir(d->path);return d->native?FR_OK:failure(); }
FRESULT f_readdir(DIR *d,FILINFO *fi) {
    struct dirent *entry=readdir(d->native);memset(fi,0,sizeof(*fi));if(!entry)return FR_OK;
    snprintf(fi->fname,sizeof(fi->fname),"%s",entry->d_name);
    fi->fattrib=entry->d_type==DT_DIR?AM_DIR:0;return FR_OK;
}
FRESULT f_closedir(DIR *d) { return closedir(d->native)?FR_DISK_ERR:FR_OK; }
FRESULT f_mkdir(const char *p) {char n[512];native_path(p,n);return mkdir(n,0700)?failure():FR_OK;}
FRESULT f_unlink(const char *p) {char n[512];native_path(p,n);return remove(n)?failure():FR_OK;}
FRESULT f_rename(const char *a,const char *b) {char x[512],y[512];struct stat st;native_path(a,x);native_path(b,y);if(!stat(y,&st))return FR_EXIST;return rename(x,y)?failure():FR_OK;}
int bvstk_sd_pl_is_ready(void) {return mounted;}
const bvstk_sd_volume_t *bvstk_sd_pl_volume(void) {static bvstk_sd_volume_t v={0,10000,32};return &v;}
bvstk_sd_service_t *bvstk_sd_master_freertos_service(void) {return NULL;}
bvstk_status_t bvstk_sd_service_info(bvstk_sd_service_t *s,bvstk_sd_card_info_t *out) {(void)s;memset(out,0,sizeof(*out));out->ready=1;return BVSTK_OK;}

static int body_read(void *user,void *out,size_t n) {
    const char **p=user;size_t len=strlen(*p);if(n>len)n=len;if(n>13)n=13;
    memcpy(out,*p,n);*p+=n;return (int)n;
}
static void http_run(const char *method,const char *path,const char *body,int status) {
    const char *cursor=body; char expected[40];
    http_request_t req={.method=method,.path=path,.has_content_length=true,.content_length=strlen(body)};
    http_conn_t conn={.fd=-1,.read_body=body_read,.read_user=&cursor};
    output_length=0;output[0]=0;
    assert(http_ui_handle(&req,&conn)==1);
    snprintf(expected,sizeof(expected),"HTTP/1.0 %d ",status);
    if(strncmp(output,expected,strlen(expected))){fprintf(stderr,"%s %s expected %d:\n%s\n",method,path,status,output);abort();}
}
static void revision(char out[9]) {
    const char *p=strstr(output,"\"revision\":\"");assert(p);memcpy(out,p+12,8);out[8]=0;
}
int main(int argc,char **argv) {
    assert(argc==2);snprintf(storage,sizeof(storage),"%s",argv[1]);assert(!mkdir(storage,0700));
    for(int i=0;i<3;i++){char p[300];snprintf(p,sizeof(p),"%s/%d",storage,i);assert(!mkdir(p,0700));}
    reset_fixture();char rev[9],path[256],json[32768];size_t n;
    /* The API still edits legacy projections of v2-backed profiles. */
    assert(bvstk_device_profile_format_i2c(&i2c_config,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(json,&i2c_config)==BVSTK_OK);
    strcpy(i2c_config.file_name,"dev.json"); strcpy(i2c_config.profile.label,"I2C metadata");
    assert(bvstk_device_profile_format_mdio(&mdio_saved,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_mdio_config_parse(json,&mdio_saved)==BVSTK_OK);
    strcpy(mdio_saved.file_name,"dev.json"); strcpy(mdio_saved.profile.label,"MDIO metadata");
    init_services();
    i2c.events.publish = bvstk_observer_publish;
    mdio.events.publish = bvstk_observer_publish;
    http_run("GET","/api/ui/bus?bus=mdio","",200);assert(strstr(output,"\"devices\""));
    mdio.cache[0][2]=(bvstk_mdio_cache_entry_t){.value=7,.valid=1,.updated_ms=bvstk_platform_now_ms()};
    mdio.cache[0][3]=(bvstk_mdio_cache_entry_t){.value=0xc0f1,.valid=1,.updated_ms=bvstk_platform_now_ms()};
    http_run("GET","/api/ui/bus?bus=mdio","",200);
    assert(strstr(output,"\"available\":true,\"presence\":\"responding\""));
    mdio.cache[0][3].updated_ms=bvstk_platform_now_ms()-UINT64_C(3000);
    http_run("GET","/api/ui/bus?bus=mdio","",200);
    assert(strstr(output,"\"available\":false,\"presence\":\"unavailable\""));
    mdio.cache[0][3].updated_ms=bvstk_platform_now_ms();mdio.cache[0][3].seeded=1;
    http_run("GET","/api/ui/bus?bus=mdio","",200);
    assert(strstr(output,"\"available\":false,\"presence\":\"unavailable\""));
    mdio.cache[0][3].seeded=0;
    http_run("GET","/api/ui/bus?bus=mdio&name=dev","",200);revision(rev);assert(strstr(output,"\"saved\":false"));
    snprintf(path,sizeof(path),"/api/ui/bus?bus=mdio&name=dev&action=save&revision=%s",rev);
    http_run("PUT",path,"{}",200);assert(mdio_saves==1);
    save_ok=0;http_run("PUT",path,"{}",500);save_ok=1;
    http_run("PUT","/api/ui/bus?bus=mdio&name=dev&action=apply&revision=wrong","{}",409);
    snprintf(path,sizeof(path),"/api/ui/bus?bus=mdio&name=dev&action=apply&revision=%s",rev);
    mdio_saved.policy=MDIO_POLICY_BLACKLIST;
    assert(bvstk_mdio_config_format(&mdio_saved,json,sizeof(json),&n)==BVSTK_OK);
    http_run("PUT",path,json,200);assert(mdio_saves==2); /* explicit failed save above counted, apply did not save */
    mdio_phy_config_t mdio_after;
    assert(bvstk_mdio_service_get_config(&mdio,0,&mdio_after)==BVSTK_OK);
    assert(mdio_after.profile.version==2 && !strcmp(mdio_after.profile.label,"MDIO metadata"));
    http_run("PUT",path,json,409);
    http_run("GET","/api/ui/bus?bus=i2c&name=dev","",200);revision(rev);
    assert(strstr(output,"\"available\":false,\"presence\":\"unknown\""));
    assert(bvstk_i2c_cache_write(&cache,0,2,3)==BVSTK_OK);
    http_run("GET","/api/ui/bus?bus=i2c","",200);
    assert(strstr(output,"\"available\":false,\"presence\":\"unknown\""));
    http_run("GET","/api/ui/bus?bus=i2c&name=dev","",200);
    assert(strstr(output,"\"cache\":") && strstr(output,"\"reg\":2,\"val\":3,\"valid\":true") &&
        strstr(output,"\"updated_ms\":"));
    http_run("GET","/api/ui/bus?bus=i2c&name=dev&action=cache","",200);
    assert(strstr(output,"\"cache\":") && strstr(output,"\"source\":\"I2C\"") &&
        strstr(output,"\"updated_ms\":"));
    snprintf(path,sizeof(path),"/api/ui/bus?bus=i2c&name=dev&action=apply&revision=%s",rev);
    http_run("PUT",path,"{\"name\":\"dev\",\"name\":\"other\"}",400);
    assert(bvstk_i2c_config_serialize_json(&i2c_config,json,sizeof(json),&n)==BVSTK_OK);
    http_run("PUT",path,json,200);
    assert(i2c_config.profile.version==2 && !strcmp(i2c_config.profile.label,"I2C metadata"));
    http_run("PUT","/api/ui/bus?bus=i2c&name=dev&action=write&reg=2&val=4&confirm=true","{}",403);assert(!writes);
    http_run("PUT","/api/ui/bus?bus=i2c&name=dev&action=write&reg=2&val=3&confirm=true","{}",200);assert(writes==1);
    http_run("GET","/api/ui/bus?bus=i2c","",200);
    assert(strstr(output,"\"presence\":\"unknown\""));
    http_run("PUT","/api/ui/bus?bus=i2c&name=dev&action=read&reg=2","{}",200);assert(strstr(output,"\"val\":3"));
    http_run("GET","/api/ui/bus?bus=i2c&name=dev","",200);
    assert(strstr(output,"\"available\":true,\"presence\":\"responding\""));
    io_status=BVSTK_ERR_IO;
    http_run("PUT","/api/ui/bus?bus=i2c&name=dev&action=read&reg=2","{}",500);
    http_run("GET","/api/ui/bus?bus=i2c&name=dev&action=cache","",200);
    assert(strstr(output,"\"reg\":2,\"val\":3,\"valid\":false"));
    http_run("GET","/api/ui/bus?bus=i2c","",200);
    assert(strstr(output,"\"available\":false,\"presence\":\"unavailable\""));
    io_status=BVSTK_OK;
    http_run("PUT","/api/ui/bus?bus=i2c&name=dev&action=read&reg=2","{}",200);
    i2c_device_config_t moved=i2c_config;moved.addr_7b++;
    assert(bvstk_i2c_master_service_set_config(&i2c,0,&moved)==BVSTK_OK);
    http_run("GET","/api/ui/bus?bus=i2c","",200);
    assert(strstr(output,"\"presence\":\"unknown\""));
    assert(bvstk_i2c_master_service_set_config(&i2c,0,&i2c_config)==BVSTK_OK);
    {
        uint64_t before = bvstk_observer_head(0, BVSTK_JOURNAL_TELEMETRY);
        io_status = BVSTK_ERR_IO;
        http_run("PUT","/api/ui/bus?bus=i2c&name=dev&action=read&reg=2","{}",500);
        io_status = BVSTK_OK;
        snprintf(path,sizeof(path),"/api/ui/observe?bus=i2c&channel=telemetry&address=54&after=%llu",
            (unsigned long long)before);
        http_run("GET",path,"",200);
        assert(strstr(output,"\"records\":[]"));
        before = bvstk_observer_head(1, BVSTK_JOURNAL_TELEMETRY);
        assert(bvstk_mdio_service_step(&mdio, 1000U, 100U) == BVSTK_OK);
        snprintf(path,sizeof(path),"/api/ui/observe?bus=mdio&channel=telemetry&address=1&after=%llu",
            (unsigned long long)before);
        http_run("GET",path,"",200);
        assert(strstr(output,"\"records\":[]"));
    }
    {
        const uint32_t physical_reads = mdio.stats.reads;
        const unsigned physical_writes = writes;
        uint64_t head = bvstk_observer_head(0, BVSTK_JOURNAL_EVENTS);
        assert(head == 4);
        http_run("GET","/api/ui/observe?bus=i2c&channel=events&address=54","",200);
        assert(strstr(output,"\"type\":1") && strstr(output,"\"type\":3") && strstr(output,"\"type\":2"));
        assert(strstr(output,"\"source\":4") && strstr(output,"\"lost\":\"0\""));
        http_run("GET","/api/ui/observe?bus=i2c&channel=events&address=65","",200);
        assert(strstr(output,"\"records\":[]") && strstr(output,"\"next\":\"4\""));
        http_run("GET","/api/ui/observe?bus=i2c&channel=telemetry&address=54","",200);
        assert(strstr(output,"\"type\":258") && strstr(output,"\"status\":9"));
        http_run("GET","/api/ui/observe?bus=mdio&channel=telemetry&address=1","",200);
        assert(strstr(output,"\"records\":[]"));
        assert(mdio.stats.reads == physical_reads && writes == physical_writes);
        assert(bvstk_observer_head(0, BVSTK_JOURNAL_EVENTS) == head);
        http_run("GET","/api/ui/observe?bus=i2c&channel=events&after=4","",200);
        assert(strstr(output,"\"records\":[]"));
        http_run("GET","/api/ui/observe?bus=i2c&channel=events&after=999","",200);
        assert(strstr(output,"\"reset\":true"));
        const char *bad[] = {"after=-1", "after=18446744073709551616", "after=18446744073709551615",
            "after=1&after=2", "address=128", "address=1&address=2", "after=abc"};
        for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
            snprintf(path,sizeof(path),"/api/ui/observe?bus=i2c&channel=events&%s",bad[i]);
            http_run("GET",path,"",400);
        }
        http_run("GET","/api/ui/observe?bus=mdio&channel=telemetry&address=32","",400);
        http_run("GET","/api/ui/observe?bus=spi&channel=telemetry","",400);
        http_run("GET","/api/ui/observe?bus=i2c&channel=unknown","",400);
        http_run("PUT","/api/ui/observe?bus=i2c&channel=events","",405);
        http_run("GET","/api/ui/observe-more?bus=i2c&channel=events","",404);
    }
    http_run("GET","/api/ui/volumes","",200);assert(strstr(output,"sd-pl"));assert(strstr(output,"\"name\":\"flash\",\"ready\":true,\"fat_bits\":32"));assert(strstr(output,"\"name\":\"sd\",\"ready\":true,\"fat_bits\":32"));
    http_run("GET","/api/ui/files?volume=sd-pl&path=%2F","",200);
    http_run("PUT","/api/ui/files?volume=sd-pl&path=%2Fdemo&action=mkdir&confirm=true","{}",200);
    http_run("PUT","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt&confirm=true","hello",200);
    http_run("PUT","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt&confirm=true","bad",409);
    http_run("GET","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt","",200);assert(strstr(output,"hello"));
    http_run("GET","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt&max_bytes=4","",413);
    http_run("GET","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt&max_bytes=5","",200);
    http_run("GET","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt&max_bytes=-1","",400);
    http_run("PUT","/api/ui/file?volume=sd-pl&path=%2Fdemo%2Ffile.txt&confirm=true&overwrite=true","new",200);
    http_run("PUT","/api/ui/files?volume=sd-pl&path=%2Fdemo%2Ffile.txt&action=copy&dst_volume=flash&dst=%2Fcopy.txt&confirm=true","{}",200);
    http_run("GET","/api/ui/file?volume=flash&path=%2Fcopy.txt","",200);assert(strstr(output,"new"));
    http_run("PUT","/api/ui/files?volume=flash&path=%2Fcopy.txt&action=rename&dst_volume=flash&dst=%2Frenamed.txt&confirm=true","{}",200);
    http_run("PUT","/api/ui/files?volume=flash&path=%2Frenamed.txt&action=remove&confirm=true","{}",200);
    http_run("GET","/api/ui/files?volume=sd-pl&path=%2F..%2Fsecret","",400);
    http_run("GET","/api/ui/files?volume=sd-pl&path=%2F&path=%2Fa","",400);
    http_run("GET","/api/ui/files?volume=sd-pl&path=%2F%00oops","",400);
    http_run("PUT","/api/ui/files?volume=sd-pl&path=%2F&action=remove&confirm=true","{}",400);
    http_run("PUT","/api/ui/file?volume=sd-pl&path=%2Funconfirmed","x",400);
    close_error=1;
    http_run("PUT","/api/ui/file?volume=sd-pl&path=%2Fclose-failure&confirm=true","x",500);
    mounted=0;http_run("GET","/api/ui/files?volume=sd-pl&path=%2F","",503);mounted=1;
    puts("HTTP UI bus and filesystem tests passed");return 0;
}
