#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_mdio_config_codec.h"
#include "shared/config/bvstk_device_profile.h"

static void read_profile(const char *root,const char *relative,char json[BVSTK_PROFILE_JSON_MAX+1])
{
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s",root,relative)<(int)sizeof(path));
    FILE *f=fopen(path,"rb");assert(f);
    size_t n=fread(json,1,BVSTK_PROFILE_JSON_MAX,f);assert(!ferror(f) && feof(f));json[n]=0;
    assert(fclose(f)==0);
}
int main(int argc,char **argv)
{
    char json[BVSTK_PROFILE_JSON_MAX+1];i2c_device_config_t i2c;mdio_phy_config_t mdio;
    assert(argc==2);
    read_profile(argv[1],"i2c/pca9545a/pca9555/pca9555.json",json);
    assert(bvstk_i2c_config_parse_json(json,&i2c)==BVSTK_OK);
    assert(!strcmp(i2c.name,"pca9555") && i2c.addr_7b==0x20 && i2c.reg_count==8 && i2c.max_value_code==255);
    assert(!strcmp(i2c.endpoint.parent,"pca9545a") && i2c.endpoint.channel==3);
    assert(i2c.policy==I2C_POLICY_WHITELIST && !i2c.whitelist_len && !i2c.settings_len);
    read_profile(argv[1],"i2c/pca9545a/sfp/sfp.json",json);
    assert(bvstk_i2c_config_parse_json(json,&i2c)==BVSTK_OK);
    assert(!strcmp(i2c.name,"sfp") && i2c.reg_count==256 && i2c.addr_7b==0x50);
    assert(!strcmp(i2c.endpoint.parent,"pca9545a") && i2c.endpoint.channel==1);
    assert(i2c.endpoint.space_count==2);
    assert(!strcmp(i2c.endpoint.spaces[0].id,"identity"));
    assert(i2c.endpoint.spaces[0].first==0 && i2c.endpoint.spaces[0].count==128);
    assert(!strcmp(i2c.endpoint.spaces[1].id,"diagnostics"));
    assert(i2c.endpoint.spaces[1].address==0x51 && i2c.endpoint.spaces[1].first==128);
    assert(i2c.endpoint.spaces[1].count==128);
    for (unsigned n=0;n<sizeof(i2c.endpoint.read_only);n++) assert(i2c.endpoint.read_only[n]==0xFF);
    assert(i2c.policy==I2C_POLICY_WHITELIST && !i2c.whitelist_len && !i2c.settings_len);
    assert(!strcmp(i2c.profile.register_map,"register_map.json") && !strcmp(i2c.profile.view,"view.js"));
    assert(i2c.profile.ids_size<=BVSTK_PROFILE_IDS_CAP);
    size_t json_size=0;
    assert(bvstk_device_profile_format_i2c(&i2c,json,sizeof(json),&json_size)==BVSTK_OK && json_size>0);
    i2c_device_config_t restored;
    assert(bvstk_i2c_config_parse_json(json,&restored)==BVSTK_OK);
    assert(!memcmp(&restored.endpoint,&i2c.endpoint,sizeof(i2c.endpoint)));
    assert(!memcmp(&restored.profile,&i2c.profile,sizeof(i2c.profile)));
    read_profile(argv[1],"i2c/axp15060/axp15060.json",json);
    assert(bvstk_i2c_config_parse_json(json,&i2c)==BVSTK_OK);
    assert(i2c.addr_7b==0x36 && i2c.reg_count==74 && i2c.max_value_code==64);
    assert(!strcmp(i2c.profile.register_map,"register_map.json") && !strcmp(i2c.profile.view,"view.js"));
    const char *names[]={"dp83848c","ksz8081rna"};
    for (unsigned i=0;i<2;i++) {
        char path[80];snprintf(path,sizeof(path),"mdio/%s/%s.json",names[i],names[i]);read_profile(argv[1],path,json);
        assert(bvstk_mdio_config_parse(json,&mdio)==BVSTK_OK);
        assert(!strcmp(mdio.name,names[i]) && mdio.phy_addr==i+2 && mdio.reg_count==32);
        assert(mdio.policy==MDIO_POLICY_WHITELIST && !mdio.whitelist_len && !mdio.settings_len && !mdio.autopoll_enabled);
    }
    const char *good[]={"lan8720/lan8720.json","lan8720/profile.json","AxP15060.json","a-b_c/a-b_c.json","a-b_c/profile.json","a/b/profile.json","pca9545a/pcf8563/pcf8563.json"};
    const char *bad[]={"../profile.json","/profile.json","a/../profile.json","a//b/b.json","a/register_map.json","a./profile.json","a\\profile.json","a/","a.json.bak"};
    for (size_t n=0;n<sizeof(good)/sizeof(good[0]);++n) assert(bvstk_device_profile_path_valid(good[n]));
    for (size_t n=0;n<sizeof(bad)/sizeof(bad[0]);++n) assert(!bvstk_device_profile_path_valid(bad[n]));
    assert(bvstk_device_profile_same_file("LAN8720.json","lan8720/lan8720.json"));
    assert(bvstk_device_profile_same_file("lan8720/profile.json","lan8720/lan8720.json"));
    assert(!bvstk_device_profile_same_file("lan8720.json","dp83848c/profile.json"));
    char saved[I2C_CFG_FILE_NAME_MAX];
    assert(bvstk_device_profile_save_path("pcf8563","pca9545a/pcf8563/profile.json",saved,sizeof(saved)));
    assert(!strcmp(saved,"pca9545a/pcf8563/pcf8563.json"));
    assert(bvstk_device_profile_save_path("pcf8563",saved,saved+100,sizeof(saved)-100));
    assert(!strcmp(saved+100,"pca9545a/pcf8563/pcf8563.json"));
    assert(!bvstk_device_profile_save_path("other",saved,saved+100,sizeof(saved)-100));
    assert(bvstk_device_profile_location_valid(saved,"pcf8563","pca9545a"));
    assert(!bvstk_device_profile_location_valid(saved,"pcf8563","wrong"));
    assert(!bvstk_device_profile_location_valid(saved,"pcf8563",""));
    assert(!bvstk_device_profile_location_valid("pcf8563/pcf8563.json","pcf8563","pca9545a"));
    assert(bvstk_device_profile_is_register_map("register_map.json"));
    assert(bvstk_device_profile_is_register_map("lan8720_register_map.json"));
    assert(!bvstk_device_profile_is_register_map("profile.json"));
    read_profile(argv[1],"mdio/lan8720/lan8720.json",json);
    assert(bvstk_mdio_config_parse(json,&mdio)==BVSTK_OK);
    strcpy(mdio.file_name,"lan8720/lan8720.json");
    assert(bvstk_mdio_config_validate(&mdio)==BVSTK_OK);
    assert(!strcmp(mdio.profile.register_map,"register_map.json") && !strcmp(mdio.profile.view,"view.js"));
    puts("Device profile tests passed: PCA9555, SFP, DP83848C, KSZ8081RNA; writes denied, no seeded presence");
    return 0;
}
