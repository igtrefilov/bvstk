/* Old files are immutable migration fixtures, not generated from v2. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"
#include "shared/config/bvstk_mdio_config_codec.h"
#include "services/i2c/bvstk_i2c_policy.h"

static char json[BVSTK_PROFILE_JSON_MAX+1], encoded[BVSTK_PROFILE_JSON_MAX+1], changed[BVSTK_PROFILE_JSON_MAX+1];
static void read_profile(const char *root, const char *relative)
{
    char path[1024]; assert(snprintf(path,sizeof(path),"%s/%s",root,relative)<(int)sizeof(path));
    FILE *f=fopen(path,"rb"); assert(f);
    size_t n=fread(json,1,BVSTK_PROFILE_JSON_MAX,f); assert(!ferror(f) && feof(f)); json[n]=0;
    assert(fclose(f)==0);
}
static void replace(const char *source, const char *old, const char *replacement)
{
    const char *where=strstr(source,old); assert(where);
    size_t prefix=(size_t)(where-source), tail=strlen(where+strlen(old));
    assert(prefix+strlen(replacement)+tail<sizeof(changed));
    memcpy(changed,source,prefix); strcpy(changed+prefix,replacement);
    strcpy(changed+prefix+strlen(replacement),where+strlen(old));
}
static void same_i2c(const i2c_device_config_t *a, const i2c_device_config_t *b)
{
    static char x[32768],y[32768]; size_t xn,yn;
    assert(bvstk_i2c_config_serialize_json(a,x,sizeof(x),&xn)==BVSTK_OK);
    assert(bvstk_i2c_config_serialize_json(b,y,sizeof(y),&yn)==BVSTK_OK);
    assert(xn==yn && !strcmp(x,y));
}
static void same_mdio(const mdio_phy_config_t *a, const mdio_phy_config_t *b)
{
    static char x[16384],y[16384]; size_t xn,yn;
    assert(bvstk_mdio_config_format(a,x,sizeof(x),&xn)==BVSTK_OK);
    assert(bvstk_mdio_config_format(b,y,sizeof(y),&yn)==BVSTK_OK);
    assert(xn==yn && !strcmp(x,y));
}
static void same_metadata(const bvstk_profile_metadata_t *a,const bvstk_profile_metadata_t *b,unsigned count)
{
    assert(a->version==2 && b->version==2);
    assert(!strcmp(a->model,b->model) && !strcmp(a->vendor,b->vendor));
    assert(!strcmp(a->device_class,b->device_class) && !strcmp(a->label,b->label));
    for(unsigned r=0;r<count;++r) assert(!strcmp(a->ids+a->id_offset[r],b->ids+b->id_offset[r]));
}
static void policy_i2c(const i2c_device_config_t *a,const i2c_device_config_t *b)
{
    static bvstk_i2c_policy_t left,right;
    bvstk_i2c_device_t d={0}; d.addr_7b=a->addr_7b; d.reg_count=a->reg_count; d.max_value_code=a->max_value_code;
    assert(bvstk_i2c_policy_init(&left,a,1)==BVSTK_OK && bvstk_i2c_policy_init(&right,b,1)==BVSTK_OK);
    for(unsigned r=0;r<256;++r) for(unsigned v=0;v<256;++v)
        assert(bvstk_i2c_policy_permits(&left,0,&d,(uint8_t)r,(uint8_t)v)==
               bvstk_i2c_policy_permits(&right,0,&d,(uint8_t)r,(uint8_t)v));
}
static void policy_mdio(const mdio_phy_config_t *a,const mdio_phy_config_t *b)
{
    for(unsigned r=0;r<32;++r) for(unsigned v=0;v<=65535;++v)
        assert(bvstk_mdio_policy_allows(a,(uint8_t)r,(uint16_t)v)==bvstk_mdio_policy_allows(b,(uint8_t)r,(uint16_t)v));
}
static void migrations(const char *root,const char *legacy)
{
    const char *i2c_names[]={"i2c/axp15060.json","i2c/pca9536.json"};
    const char *mdio_names[]={"mdio/lan8720.json","mdio/dp83848c.json","mdio/ksz8081rna.json"}; size_t n;
    for(unsigned i=0;i<2;++i) {
        i2c_device_config_t old,current,saved;
        read_profile(legacy,i2c_names[i]); assert(bvstk_i2c_config_parse_json(json,&old)==BVSTK_OK && !old.profile.version);
        read_profile(root,i2c_names[i]); assert(bvstk_i2c_config_parse_json(json,&current)==BVSTK_OK && current.profile.version==2);
        same_i2c(&old,&current); policy_i2c(&old,&current);
        assert(bvstk_device_profile_format_i2c(&current,encoded,sizeof(encoded),&n)==BVSTK_OK && n==strlen(encoded));
        assert(bvstk_i2c_config_parse_json(encoded,&saved)==BVSTK_OK);
        same_i2c(&current,&saved); same_metadata(&current.profile,&saved.profile,current.reg_count);
        assert(bvstk_device_profile_format_i2c(&old,encoded,sizeof(encoded),&n)==BVSTK_OK);
        assert(bvstk_i2c_config_parse_json(encoded,&saved)==BVSTK_OK); same_i2c(&old,&saved);
    }
    for(unsigned i=0;i<3;++i) {
        mdio_phy_config_t old,current,saved;
        read_profile(legacy,mdio_names[i]); assert(bvstk_mdio_config_parse(json,&old)==BVSTK_OK && !old.profile.version);
        read_profile(root,mdio_names[i]); assert(bvstk_mdio_config_parse(json,&current)==BVSTK_OK && current.profile.version==2);
        same_mdio(&old,&current); policy_mdio(&old,&current);
        assert(bvstk_device_profile_format_mdio(&current,encoded,sizeof(encoded),&n)==BVSTK_OK && n==strlen(encoded));
        assert(bvstk_mdio_config_parse(encoded,&saved)==BVSTK_OK);
        same_mdio(&current,&saved); same_metadata(&current.profile,&saved.profile,current.reg_count);
        assert(bvstk_device_profile_format_mdio(&old,encoded,sizeof(encoded),&n)==BVSTK_OK);
        assert(bvstk_mdio_config_parse(encoded,&saved)==BVSTK_OK); same_mdio(&old,&saved);
    }
}
static void reject(const char *source,int mdio)
{
    if(mdio) {
        mdio_phy_config_t target,before; memset(&target,0xa5,sizeof(target)); memcpy(&before,&target,sizeof(target));
        assert(bvstk_mdio_config_parse(source,&target)!=BVSTK_OK && !memcmp(&target,&before,sizeof(target)));
    } else {
        i2c_device_config_t target,before; memset(&target,0xa5,sizeof(target)); memcpy(&before,&target,sizeof(target));
        assert(bvstk_i2c_config_parse_json(source,&target)!=BVSTK_OK && !memcmp(&target,&before,sizeof(target)));
    }
}
static void malformed(const char *root)
{
    mdio_phy_config_t c,round; size_t n;
    read_profile(root,"mdio/lan8720.json"); assert(bvstk_mdio_config_parse(json,&c)==BVSTK_OK);
    assert(bvstk_device_profile_format_mdio(&c,encoded,sizeof(encoded),&n)==BVSTK_OK);
    const char *edits[][2]={
        {"\"schema_version\": 2","\"schema_version\": 3"},
        {"\"schema_version\": 2","\"schema_version\": 2, \"schema_version\": 2"},
        {"\"schema_version\": 2,",""},
        {"\"schema_version\": 2","\"schema_version\": 2.0"},
        {"\"schema_version\": 2","\"schema_version\": 02"},
        {"\"id\": \"lan8720\"","\"id\": \"lan8720\", \"extra\": {}"},
        {"\"address\": \"0x01\"","\"address\": \"0x20\""},
        {"\"address\": \"0x01\"","\"address\": 1"},
        {"\"bvstk_mdio_bridge\"","\"mdio_clause45\""},
        {"\"pl_mdio_0\"","\"pl_mdio_1\""},
        {"\"width_bits\": 16","\"width_bits\": 8"},
        {"\"access\": \"rw\"","\"access\": \"ro\""},
        {"\"mode\": \"driver\"","\"mode\": \"snapshot\""},
        {"\"write_mask\": \"0xFFFF\"","\"write_mask\": \"0x1FFFF\""},
        {"\"write_mask\": \"0xFFFF\"","\"write_mask\": \"0x0001\""},
        {"\"autopoll_enabled\": true","\"autopoll_enabled\": 1"},
        {"\"autopoll_cycle_delay_ms\": 50","\"autopoll_cycle_delay_ms\": -1"},
        {"\"autopoll_reg_delay_ms\": 0","\"autopoll_reg_delay_ms\": 4294967296"},
        {"\"autopoll_order\": [\"reg_00\"","\"autopoll_order\": [\"reg_00\", \"reg_00\""},
        {"\"id\": \"reg_01\"","\"id\": \"reg_00\""},
        {"\"kind\": \"register\", \"address\": \"0x01\"","\"kind\": \"register\", \"address\": \"0x00\""},
        {"\"id\": \"reg_00\"","\"id\": \"missing_ref\""},
        {"\"apply_on_startup\": false","\"apply_on_startup\": true"},
        {"\"desired\": []","\"desired\": [{\"resource\":\"reg_00\",\"value\":\"0x8000\"}]"},
        {"\"telemetry\": true","\"telemetry\": false"},
        {"\"events\": true","\"events\": null"},
        {"\"mode\": \"blacklist\", \"rules\": []","\"mode\": \"whitelist\", \"rules\": []"},
        {"\"resources\": [","\"resources\": [null,"},
        {"\"model\": \"LAN8720A\"","\"model\": \"bad\\u0000name\""},
        {"\"model\": \"LAN8720A\"","\"model\": \"bad\\ud800\""},
        {"\"model\": \"LAN8720A\"","\"model\": \"bad\\nname\""}
    };
    for(size_t i=0;i<sizeof(edits)/sizeof(edits[0]);++i) { replace(encoded,edits[i][0],edits[i][1]); reject(changed,1); }
    for(size_t i=0;i<n-2;++i) { char saved=encoded[i]; encoded[i]=0; reject(encoded,1); encoded[i]=saved; }
    replace(encoded,"\"model\": \"LAN8720A\"","\"model\": \"\\u0424\\u0438\\u0437\\u0438\\u043a\\u0430 \\\"PHY\\\" \\ud83d\\udce1\"");
    assert(bvstk_mdio_config_parse(changed,&c)==BVSTK_OK && strstr(c.profile.model,"Физика \"PHY\""));
    assert(bvstk_device_profile_format_mdio(&c,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_mdio_config_parse(json,&round)==BVSTK_OK); same_metadata(&c.profile,&round.profile,c.reg_count);
    read_profile(root,"i2c/pca9536.json");
    replace(json,"\"capture_cache_on_save\": true","\"capture_cache_on_save\": false"); reject(changed,0);
    replace(json,"\"max_write_value\": \"0x0F\"","\"max_write_value\": \"0x41\""); reject(changed,0);
    replace(json,"\"address_bits\": 7","\"address_bits\": 10"); reject(changed,0);
    replace(json,"\"address\": \"0x41\"","\"address\": \"0x10041\""); reject(changed,0);
    memset(changed,' ',sizeof(changed)); memcpy(changed,"{}",2); changed[sizeof(changed)-1]=0; unsigned version;
    assert(bvstk_device_profile_version(changed,&version)==BVSTK_OK && version==0);
    char *oversize=malloc(BVSTK_PROFILE_JSON_MAX+2U); assert(oversize);
    memcpy(oversize,changed,BVSTK_PROFILE_JSON_MAX); oversize[BVSTK_PROFILE_JSON_MAX]=' '; oversize[BVSTK_PROFILE_JSON_MAX+1]=0;
    assert(bvstk_device_profile_version(oversize,&version)!=BVSTK_OK); free(oversize);
}
static void mutable_settings(void)
{
    i2c_device_config_t i={0},ir; mdio_phy_config_t m={0},mr; size_t n;
    strcpy(i.name,"full_i2c"); i.addr_7b=0x36; i.reg_count=256; i.max_value_code=64; i.policy=I2C_POLICY_BLACKLIST;
    i.whitelist_len=i.blacklist_len=i.settings_len=256;
    for(unsigned r=0;r<256;++r) {
        i.whitelist[r]=(i2c_rule_entry_t){(uint8_t)r,17}; i.blacklist[r]=(i2c_rule_entry_t){(uint8_t)r,18};
        i.settings[r]=(i2c_rule_entry_t){(uint8_t)r,(uint8_t)(255-r)};
    }
    assert(bvstk_device_profile_format_i2c(&i,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(json,&ir)==BVSTK_OK); same_i2c(&i,&ir); policy_i2c(&i,&ir);
    i.policy=I2C_POLICY_WHITELIST;
    assert(bvstk_device_profile_format_i2c(&i,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(json,&ir)==BVSTK_OK); same_i2c(&i,&ir);
    i.settings_len=2; i.settings[0]=(i2c_rule_entry_t){7,200}; i.settings[1]=(i2c_rule_entry_t){7,201};
    assert(bvstk_device_profile_format_i2c(&i,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(json,&ir)==BVSTK_OK && ir.settings_len==1 && ir.settings[0].reg==7 && ir.settings[0].val==201);
    strcpy(i.name,"Old.Name"); assert(bvstk_device_profile_format_i2c(&i,json,sizeof(json),&n)==BVSTK_ERR_UNSUPPORTED);
    assert(bvstk_i2c_config_serialize_json(&i,json,sizeof(json),&n)==BVSTK_OK);
    strcpy(m.name,"phy"); m.phy_addr=31; m.reg_count=32; m.policy=MDIO_POLICY_BLACKLIST;
    m.autopoll_regs_len=32; m.autopoll_cycle_delay_ms=600000; m.autopoll_reg_delay_ms=60000;
    m.settings_len=32; m.whitelist_len=m.blacklist_len=64;
    for(unsigned r=0;r<32;++r) { m.autopoll_regs[r]=(uint8_t)(31-r); m.settings[r]=(mdio_setting_entry_t){(uint8_t)r,(uint16_t)(65535-r)}; }
    for(unsigned r=0;r<64;++r) {
        m.whitelist[r]=(mdio_rule_entry_t){(uint8_t)(r%32),0x8010,0xffff}; m.blacklist[r]=(mdio_rule_entry_t){(uint8_t)(r%32),0x0040,0x00f0};
    }
    assert(bvstk_device_profile_format_mdio(&m,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_mdio_config_parse(json,&mr)==BVSTK_OK); same_mdio(&m,&mr); policy_mdio(&m,&mr);
    m.policy=MDIO_POLICY_WHITELIST; m.autopoll_enabled=true;
    assert(bvstk_device_profile_format_mdio(&m,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_mdio_config_parse(json,&mr)==BVSTK_OK); same_mdio(&m,&mr); policy_mdio(&m,&mr);
    m.profile=mr.profile;
    memset(m.profile.ids,0,sizeof(m.profile.ids)); m.profile.ids_size=1;
    for(unsigned r=0;r<m.reg_count;++r) {
        m.profile.id_offset[r]=m.profile.ids_size;
        int length=snprintf(m.profile.ids+m.profile.ids_size,sizeof(m.profile.ids)-m.profile.ids_size,"custom_reg_%u",r);
        assert(length>0); m.profile.ids_size+=(uint16_t)(length+1);
    }
    assert(bvstk_device_profile_format_mdio(&m,json,sizeof(json),&n)==BVSTK_OK);
    assert(bvstk_mdio_config_parse(json,&mr)==BVSTK_OK);
    same_mdio(&m,&mr); same_metadata(&m.profile,&mr.profile,m.reg_count);
    for(size_t cap=1;cap<128;++cap) {
        memset(encoded,0x5a,sizeof(encoded)); n=999;
        assert(bvstk_device_profile_format_mdio(&m,encoded,cap,&n)==BVSTK_ERR_RANGE);
        assert(n==0 && encoded[0]==0 && encoded[cap]==0x5a);
    }
}
int main(int argc,char **argv)
{
    assert(argc==3); migrations(argv[1],argv[2]); malformed(argv[1]); mutable_settings();
    puts("v2 profiles: five migrations, all policy values, strict rejection, metadata, cache and polling round trips passed");
    return 0;
}
