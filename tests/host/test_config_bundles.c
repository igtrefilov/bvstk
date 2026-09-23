/* Real configuration task against temporary POSIX files via a FatFs shim.
 * Includes the implementation to exercise startup, precedence and recovery. */
#define _DEFAULT_SOURCE
#define DIR NativeDir
#include <dirent.h>
#undef DIR
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "apps/freertos/config/config_store.c"
#include "apps/neutrino/config/bvstk_i2c_config_store.h"

static char storage[256];
static const char *stat_failure;
static FRESULT failure(void) { return errno==ENOENT ? FR_NO_FILE : errno==EEXIST ? FR_EXIST : FR_DENIED; }
static void native_path(const char *p,char out[512]) {
    assert(!strncmp(p,QSPI_ROOT,strlen(QSPI_ROOT)) && !strstr(p,".."));
    assert(snprintf(out,512,"%s/%s",storage,p+strlen(QSPI_ROOT))<512);
}
FRESULT f_stat(const char *p,FILINFO *out) {
    char n[512]; struct stat info;
    if (stat_failure && !strcmp(p,stat_failure)) return FR_DISK_ERR;
    native_path(p,n); if(stat(n,&info)) return failure();
    memset(out,0,sizeof(*out));out->fattrib=S_ISDIR(info.st_mode)?AM_DIR:0;out->fsize=(FSIZE_t)info.st_size;return FR_OK;
}
FRESULT f_open(FIL *f,const char *p,BYTE flags) {
    char n[512];struct stat info;native_path(p,n);int exists=!stat(n,&info);
    if(exists&&S_ISDIR(info.st_mode))return FR_DENIED;
    f->native=fopen(n,flags&FA_WRITE?"w+b":"rb");f->size=exists?(FSIZE_t)info.st_size:0;return f->native?FR_OK:failure();
}
FRESULT f_read(FIL *f,void *data,UINT n,UINT *got) {*got=(UINT)fread(data,1,n,f->native);return ferror(f->native)?FR_DISK_ERR:FR_OK;}
FRESULT f_write(FIL *f,const void *data,UINT n,UINT *got) {*got=(UINT)fwrite(data,1,n,f->native);return *got==n?FR_OK:FR_DISK_ERR;}
FRESULT f_close(FIL *f) {return fclose(f->native)?FR_DISK_ERR:FR_OK;}
FRESULT f_sync(FIL *f) {return fflush(f->native)?FR_DISK_ERR:FR_OK;}
FRESULT f_mkdir(const char *p) {char n[512];native_path(p,n);return mkdir(n,0700)?failure():FR_OK;}
FRESULT f_unlink(const char *p) {char n[512];native_path(p,n);return remove(n)?failure():FR_OK;}
FRESULT f_rename(const char *p,const char *q) {char a[512],b[512];struct stat info;native_path(p,a);native_path(q,b);if(!stat(b,&info))return FR_EXIST;return rename(a,b)?failure():FR_OK;}
FRESULT f_opendir(DIR *d,const char *p) {native_path(p,d->path);d->native=opendir(d->path);return d->native?FR_OK:failure();}
FRESULT f_readdir(DIR *d,FILINFO *f) {struct dirent *e=readdir(d->native);memset(f,0,sizeof(*f));if(e){strcpy(f->fname,e->d_name);f->fattrib=e->d_type==DT_DIR?AM_DIR:0;}return FR_OK;}
FRESULT f_closedir(DIR *d) {return closedir(d->native)?FR_DISK_ERR:FR_OK;}
int qspi_fs_is_ready(void) {return 1;}
int xil_printf(const char *format, ...) {(void)format;return 0;}
void vTaskDelay(TickType_t ticks) {(void)ticks;}
void vTaskDelete(TaskHandle_t task) {(void)task;}
static void put(const char *p,const char *data) {assert(ensure_device_parent(p));assert(write_file_atomic(p,data,strlen(data)));}
static void startup(void) {config_task(NULL);assert(s_ready&&s_startup_done);}
int main(int argc,char **argv) {
    assert(argc==2&&strlen(argv[1])<sizeof(storage));strcpy(storage,argv[1]);assert(!mkdir(storage,0700));
    startup();
    assert(s_i2c_cfg_count==2 && s_mdio_cfg_count==3);
    assert(!strcmp(s_i2c_cfgs[0].file_name,"axp15060/axp15060.json"));
    assert(!strcmp(s_mdio_cfgs[0].file_name,"lan8720/lan8720.json"));
    assert(file_exists(QSPI_ROOT "config/mdio/lan8720/lan8720.json"));
    // Maps, JS and arbitrary extra JSON inside a bundle are not devices.
    put(QSPI_ROOT "config/mdio/lan8720/register_map.json","{invalid-as-profile}");
    put(QSPI_ROOT "config/mdio/lan8720/extra.json","{invalid-as-profile}");
    put(QSPI_ROOT "config/mdio/lan8720/view.js","invalid();");
    put(QSPI_ROOT "config/i2c/reference_register_map.json","{}");
    // Nested policy takes precedence even over malformed flat copies.
    put(QSPI_ROOT "config/mdio/lan8720.json","broken flat shadow");
    put(QSPI_ROOT "config/i2c/axp15060.json","broken flat shadow");
    startup();assert(s_i2c_cfg_count==2&&s_mdio_cfg_count==3);
    mdio_phy_config_t m=s_mdio_cfgs[0];m.policy=MDIO_POLICY_BLACKLIST;m.blacklist_len=0;
    assert(save_mdio_device_locked(&m));
    assert(file_exists(QSPI_ROOT "config/mdio/lan8720/lan8720.json.bak"));
    startup();assert(s_mdio_cfgs[0].policy==MDIO_POLICY_BLACKLIST);
    assert(!strcmp(s_mdio_cfgs[0].profile.view,"view.js"));
    // Interrupted replacement recovers the prior nested policy before scanning.
    assert(f_unlink(QSPI_ROOT "config/mdio/lan8720/lan8720.json")==FR_OK);
    startup();assert(s_mdio_cfg_count==3 && s_mdio_cfgs[0].policy==MDIO_POLICY_WHITELIST);
    // Flat profiles remain usable; they are not replaced by bundled defaults.
    assert(f_unlink(QSPI_ROOT "config/i2c/axp15060.json")==FR_OK);
    assert(f_rename(QSPI_ROOT "config/i2c/axp15060/axp15060.json",QSPI_ROOT "config/i2c/axp15060.json")==FR_OK);
    startup();assert(s_i2c_cfg_count==2 && !strcmp(s_i2c_cfgs[0].file_name,"axp15060/axp15060.json"));
    i2c_device_config_t i=s_i2c_cfgs[0];i.policy=I2C_POLICY_BLACKLIST;assert(save_i2c_device_locked(&i));
    startup();assert(s_i2c_cfgs[0].policy==I2C_POLICY_BLACKLIST);
    assert(file_exists(QSPI_ROOT "config/i2c/axp15060/axp15060.json"));
    // Read failure in a bundle is NOT interpreted as a missing optional map.
    stat_failure=QSPI_ROOT "config/mdio/lan8720/lan8720.json";
    startup();assert(s_mdio_cfg_count==0);stat_failure=NULL;
    // An invalid bundled profile disables forwarding, no fallback to its shadow.
    put(QSPI_ROOT "config/mdio/lan8720/lan8720.json","{}");
    startup();assert(s_mdio_cfg_count==0);
    // POSIX/Neutrino uses the same bundle paths and preserves linked metadata.
    static bvstk_neutrino_i2c_config_store_t nto;
    char primary[512], fallback[512];
    snprintf(primary,sizeof(primary),"%s/config/i2c",storage);
    snprintf(fallback,sizeof(fallback),"%s/configs/i2c",storage);
    assert(bvstk_neutrino_i2c_config_store_init(&nto,primary,fallback));
    assert(nto.device_count==2);
    const i2c_device_config_t *p=bvstk_neutrino_i2c_config_store_find_name(&nto,"pca9536",NULL);
    assert(p && !strcmp(p->file_name,"pca9536/pca9536.json"));
    i=*p;strcpy(i.profile.register_map,"register_map.json");strcpy(i.profile.view,"view.js");
    assert(bvstk_neutrino_i2c_config_store_save(&nto,&i));
    put(QSPI_ROOT "config/i2c/pca9536/other.json","not a profile");
    put(QSPI_ROOT "config/i2c/pca9536.json","invalid shadow");
    assert(bvstk_neutrino_i2c_config_store_init(&nto,primary,fallback));
    p=bvstk_neutrino_i2c_config_store_find_name(&nto,"pca9536",NULL);
    assert(nto.device_count==2 && p && !strcmp(p->profile.view,"view.js"));
    puts("Config bundles: startup, sidecars, flat compatibility, precedence, explicit save, backup recovery and fail-closed passed");
    return 0;
}
