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
static int sync_failure;
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
FRESULT f_sync(FIL *f) {return sync_failure || fflush(f->native)?FR_DISK_ERR:FR_OK;}
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
static void policy_storage_roundtrip(void)
{
    i2c_device_config_t original_i2c = (*config_store_find_i2c_device_by_name("axp15060")), i = original_i2c;
    mdio_phy_config_t original_mdio = s_mdio_cfgs[0], m = original_mdio;
    i.policy = I2C_POLICY_BLACKLIST;
    i.whitelist_len = i.blacklist_len = 1;
    i.whitelist[0] = (i2c_rule_entry_t){0x13, 0x12};
    i.blacklist[0] = (i2c_rule_entry_t){0x13, 0x10};
    m.policy = MDIO_POLICY_BLACKLIST;
    m.whitelist_len = m.blacklist_len = 1;
    m.whitelist[0] = (mdio_rule_entry_t){0, 0x2000, 0x2000};
    m.blacklist[0] = (mdio_rule_entry_t){0, 0x8000, 0x8000};
    assert(save_i2c_device_locked(&i) && save_mdio_device_locked(&m));
    startup();
    assert((*config_store_find_i2c_device_by_name("axp15060")).policy == I2C_POLICY_BLACKLIST);
    assert((*config_store_find_i2c_device_by_name("axp15060")).whitelist_len == 1 && (*config_store_find_i2c_device_by_name("axp15060")).whitelist[0].val == 0x12);
    assert((*config_store_find_i2c_device_by_name("axp15060")).blacklist_len == 1 && (*config_store_find_i2c_device_by_name("axp15060")).blacklist[0].val == 0x10);
    assert(s_mdio_cfgs[0].policy == MDIO_POLICY_BLACKLIST);
    assert(s_mdio_cfgs[0].whitelist_len == 1 && s_mdio_cfgs[0].whitelist[0].mask == 0x2000);
    assert(s_mdio_cfgs[0].blacklist_len == 1 && s_mdio_cfgs[0].blacklist[0].val == 0x8000);
    assert(s_mdio_cfgs[0].blacklist[0].mask == 0x8000);
    assert((*config_store_find_i2c_device_by_name("axp15060")).settings_len == original_i2c.settings_len);
    assert(s_mdio_cfgs[0].settings_len == original_mdio.settings_len);
    assert(!strcmp((*config_store_find_i2c_device_by_name("axp15060")).profile.register_map, original_i2c.profile.register_map));
    assert(!strcmp(s_mdio_cfgs[0].profile.view, original_mdio.profile.view));
    assert(!strcmp((*config_store_find_i2c_device_by_name("axp15060")).profile.icon, "icon.js"));
    assert(!strcmp(s_mdio_cfgs[0].profile.icon, "icon.js"));
    /* A failed flush must report failure and leave the previous profile usable. */
    sync_failure = 1;
    assert(!save_i2c_device_locked(&original_i2c));
    assert(!save_mdio_device_locked(&original_mdio));
    sync_failure = 0;
    startup();
    assert((*config_store_find_i2c_device_by_name("axp15060")).policy == I2C_POLICY_BLACKLIST);
    assert(s_mdio_cfgs[0].policy == MDIO_POLICY_BLACKLIST);
    assert(save_i2c_device_locked(&original_i2c) && save_mdio_device_locked(&original_mdio));
    startup();
}
static void nested_storage_roundtrip(void)
{
    const char *path=QSPI_ROOT "config/i2c/pca9545a/pcf8563/pcf8563.json";
    const i2c_device_config_t *stored=config_store_find_i2c_device_by_name("pcf8563");
    assert(stored && !strcmp(stored->file_name,"pca9545a/pcf8563/pcf8563.json"));
    i2c_device_config_t changed=*stored;
    strcpy(changed.profile.label,"RTC");
    assert(save_i2c_device_locked(&changed));
    assert(!file_exists(QSPI_ROOT "config/i2c/pcf8563/pcf8563.json"));
    startup();
    stored=config_store_find_i2c_device_by_name("pcf8563");
    assert(stored && !strcmp(stored->profile.label,"RTC"));
    assert(!strcmp(stored->profile.icon,"icon.js"));
    assert(!memcmp(&stored->endpoint,&changed.endpoint,sizeof(stored->endpoint)));
    assert(f_unlink(path)==FR_OK); /* interrupted replace: restore nested .bak */
    startup();
    stored=config_store_find_i2c_device_by_name("pcf8563");
    assert(stored && !strcmp(stored->endpoint.parent,"pca9545a"));
    assert(!strcmp(stored->profile.icon,"icon.js"));
    assert(!strcmp(stored->file_name,"pca9545a/pcf8563/pcf8563.json"));
    assert(file_exists(path));
}
static void supplied_configuration(void)
{
    /* A supplied subset is complete; first startup must not add defaults. */
    char previous[sizeof(storage)];
    strcpy(previous,storage);
    assert(strlen(previous)+8<sizeof(storage));
    strcat(storage,"/subset");assert(!mkdir(storage,0700));
    put(QSPI_ROOT "config/i2c/axp15060/axp15060.json",DEFAULT_I2C_AXP15060_JSON);
    startup();
    assert(s_i2c_cfg_count==1 && !strcmp(s_i2c_cfgs[0].name,"axp15060"));
    assert(!file_exists(QSPI_ROOT "config/i2c/pca9545a/pca9545a.json"));
    assert(file_exists(QSPI_ROOT "config/i2c/.initialized"));
    assert(f_unlink(QSPI_ROOT "config/i2c/axp15060/axp15060.json")==FR_OK);
    startup();assert(s_i2c_cfg_count==0);
    strcpy(storage,previous);
}
int main(int argc,char **argv) {
    assert(argc==2&&strlen(argv[1])<sizeof(storage));strcpy(storage,argv[1]);assert(!mkdir(storage,0700));
    startup();
    assert(s_i2c_cfg_count==DEFAULT_I2C_CONFIG_FILES_COUNT && s_mdio_cfg_count==3);
    policy_storage_roundtrip();
    nested_storage_roundtrip();
    assert(!strcmp((*config_store_find_i2c_device_by_name("axp15060")).file_name,"axp15060/axp15060.json"));
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
    startup();assert(s_i2c_cfg_count==DEFAULT_I2C_CONFIG_FILES_COUNT&&s_mdio_cfg_count==3);
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
    startup();assert(s_i2c_cfg_count==DEFAULT_I2C_CONFIG_FILES_COUNT && !strcmp((*config_store_find_i2c_device_by_name("axp15060")).file_name,"axp15060/axp15060.json"));
    i2c_device_config_t i=(*config_store_find_i2c_device_by_name("axp15060"));i.policy=I2C_POLICY_BLACKLIST;assert(save_i2c_device_locked(&i));
    startup();assert((*config_store_find_i2c_device_by_name("axp15060")).policy==I2C_POLICY_BLACKLIST);
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
    assert(nto.device_count==DEFAULT_I2C_CONFIG_FILES_COUNT);
    const i2c_device_config_t *p=bvstk_neutrino_i2c_config_store_find_name(&nto,"axp15060",NULL);
    assert(p && !strcmp(p->file_name,"axp15060/axp15060.json"));
    i=*p;strcpy(i.profile.register_map,"register_map.json");strcpy(i.profile.view,"view.js");
    assert(bvstk_neutrino_i2c_config_store_save(&nto,&i));
    put(QSPI_ROOT "config/i2c/axp15060/other.json","not a profile");
    put(QSPI_ROOT "config/i2c/axp15060.json","invalid shadow");
    assert(bvstk_neutrino_i2c_config_store_init(&nto,primary,fallback));
    p=bvstk_neutrino_i2c_config_store_find_name(&nto,"axp15060",NULL);
    assert(nto.device_count==DEFAULT_I2C_CONFIG_FILES_COUNT && p && !strcmp(p->profile.view,"view.js"));
    p=bvstk_neutrino_i2c_config_store_find_name(&nto,"p24c02a",NULL);
    assert(p && !strcmp(p->file_name,"pca9545a/p24c02a/p24c02a.json"));
    i=*p;strcpy(i.profile.label,"EEPROM");
    assert(bvstk_neutrino_i2c_config_store_save(&nto,&i));
    assert(!file_exists(QSPI_ROOT "config/i2c/p24c02a/p24c02a.json"));
    assert(bvstk_neutrino_i2c_config_store_init(&nto,primary,fallback));
    p=bvstk_neutrino_i2c_config_store_find_name(&nto,"p24c02a",NULL);
    assert(p && !strcmp(p->profile.label,"EEPROM"));
    assert(!memcmp(&p->endpoint,&i.endpoint,sizeof(i.endpoint)));
    /* Profiles in the wrong directory fail closed; no root-address fallback. */
    assert(f_rename(QSPI_ROOT "config/i2c/pca9545a/p24c02a/p24c02a.json",
                    QSPI_ROOT "config/i2c/pca9545a/pca9545a.json")==FR_EXIST);
    put(QSPI_ROOT "config/i2c/other/other.json",DEFAULT_I2C_PCF8563_JSON);
    startup();assert(s_i2c_cfg_count==0);
    assert(!bvstk_neutrino_i2c_config_store_init(&nto,primary,fallback));
    assert(nto.device_count==0);
    supplied_configuration();
    puts("Config bundles: startup, sidecars, flat compatibility, precedence, explicit save, backup recovery and fail-closed passed");
    return 0;
}
