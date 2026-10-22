/* Configuration-driven I2C executor against an exact wire-operation script.
 * No device model names are available to the backend. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "services/i2c/bvstk_i2c_slave_service.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"

static i2c_device_config_t profiles[4], configs[6];
static bvstk_i2c_devices_t devices;
static bvstk_i2c_cache_t cache;
static bvstk_i2c_policy_t policy;
static bvstk_i2c_master_service_t master;
static bvstk_i2c_slave_service_t slave;
static uint64_t now;
static uint32_t wire_delay;
static unsigned locks, unlocks, held, commits, denials, endpoint_transactions;
typedef struct {
    uint8_t address, tx[3], value;
    size_t nt, nr;
    bvstk_status_t status;
} operation_t;
static operation_t operations[128];
static size_t expected, consumed;

uint64_t bvstk_platform_now_ms(void) { return now; }
void bvstk_platform_sleep_ms(uint32_t ms) { assert(held); now += ms; }
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t b, size_t s)
{ (void)r; (void)b; (void)s; return -1; }
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { (void)r; }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t o, uint32_t *v)
{ (void)r; (void)o; (void)v; assert(0); return -1; }
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t o, uint32_t v)
{ (void)r; (void)o; (void)v; assert(0); return -1; }

static bvstk_status_t lock_bus(void *c, uint32_t ms)
{ (void)c; (void)ms; assert(!held); held = 1; ++locks; return BVSTK_OK; }
static void unlock_bus(void *c)
{ (void)c; assert(held); held = 0; ++unlocks; }
static bvstk_status_t transfer(void *c, uint8_t address, const uint8_t *tx,
                              size_t nt, uint8_t *rx, size_t nr, uint32_t ms)
{
    (void)c; (void)ms;
    assert(held && consumed < expected);
    const operation_t *op = &operations[consumed++];
    if (address != op->address || nt != op->nt || nr != op->nr ||
        (nt && memcmp(tx, op->tx, nt))) {
        fprintf(stderr,"wire mismatch at %zu: address=%02x tx=%zu rx=%zu\n",consumed,address,nt,nr);
        assert(0);
    }
    if (op->status == BVSTK_OK)
        for (size_t i=0; i<nr; ++i) rx[i]=(uint8_t)(op->value+i);
    now += wire_delay;
    return op->status;
}
static void event(void *ctx, const bvstk_event_t *e)
{
    (void)ctx;
    if (e->type == BVSTK_EVENT_REG_COMMIT) ++commits;
    if (e->type == BVSTK_EVENT_REG_DENIED) ++denials;
    if (e->arg0==0x51 &&
        (e->type==BVSTK_EVENT_MASTER_READ || e->type==BVSTK_EVENT_MASTER_WRITE))
        ++endpoint_transactions;
}
static void operation(uint8_t addr, size_t nt, uint8_t a, uint8_t b, uint8_t c,
                      size_t nr, uint8_t value, bvstk_status_t status)
{
    assert(expected < sizeof(operations)/sizeof(operations[0]));
    operations[expected++] = (operation_t){addr,{a,b,c},value,nt,nr,status};
}
static void rd_switch(uint8_t address, uint8_t value)
{ operation(address,0,0,0,0,1,value,BVSTK_OK); }
static void wr_switch(uint8_t address, uint8_t value)
{ operation(address,1,value,0,0,0,0,BVSTK_OK); }
static void rd(uint8_t address, uint8_t reg, uint8_t value)
{ operation(address,1,reg,0,0,1,value,BVSTK_OK); }
static void wr(uint8_t address, uint8_t reg, uint8_t value)
{ operation(address,2,reg,value,0,0,0,BVSTK_OK); }
static void done(unsigned count)
{
    assert(consumed == expected && !held && locks == count && unlocks == count);
    expected = consumed = locks = unlocks = 0;
}
static void init(size_t count)
{
    wire_delay = 0;
    assert(!held && expected == consumed);
    expected = consumed = locks = unlocks = commits = denials = 0;
    const bvstk_i2c_master_io_t io = {
        .transfer=transfer, .lock=lock_bus, .unlock=unlock_bus
    };
    const bvstk_event_sink_t sink = {.publish=event};
    assert(bvstk_i2c_devices_init_from_config(&devices,configs,count)==BVSTK_OK);
    assert(bvstk_i2c_cache_init(&cache,count)==BVSTK_OK);
    assert(bvstk_i2c_policy_init(&policy,configs,count)==BVSTK_OK);
    assert(bvstk_i2c_master_service_init(&master,NULL,&io,&devices,&cache,&policy,&sink)==BVSTK_OK);
    assert(bvstk_i2c_slave_service_init(&slave,&devices,&cache,&master,0)==BVSTK_OK);
}
static void topology(void)
{
    memcpy(configs,profiles,sizeof(profiles));
    strcpy(configs[0].name,"bridge_x");
    for (size_t i=1;i<4;i++) strcpy(configs[i].endpoint.parent,"bridge_x");
    init(4);
}
static bvstk_status_t host(const uint8_t *tx, size_t n, int read, uint8_t *value)
{
    size_t received=0;
    bvstk_status_t status=bvstk_i2c_slave_service_handle_frame(&slave,tx,n,(uint8_t)read,
        value,value?1:0,&received,20);
    if (read && status==BVSTK_OK) assert(received==1);
    return status;
}
static void profiles_load(const char *root)
{
    const char *files[]={
        "pca9545a/pca9545a.json",
        "pca9545a/pcf8563/pcf8563.json",
        "pca9545a/p24c02a/p24c02a.json",
        "pca9545a/pca9555/pca9555.json"
    };
    static char json[BVSTK_PROFILE_JSON_MAX+1], encoded[BVSTK_PROFILE_JSON_MAX+1];
    for (size_t i=0;i<4;i++) {
        char path[512]; snprintf(path,sizeof(path),"%s/i2c/%s",root,files[i]);
        FILE *f=fopen(path,"rb"); assert(f);
        size_t n=fread(json,1,sizeof(json)-1,f); assert(feof(f));json[n]=0;fclose(f);
        assert(bvstk_i2c_config_parse_json(json,&profiles[i])==BVSTK_OK);
        assert(profiles[i].endpoint.transfer.configured && !profiles[i].settings_len);
        strcpy(profiles[i].file_name,files[i]);
        assert(bvstk_device_profile_format_i2c(&profiles[i],encoded,sizeof(encoded),&n)==BVSTK_OK);
        assert(strstr(encoded,"bvstk_i2c_bridge"));
        i2c_device_config_t roundtrip;
        assert(bvstk_i2c_config_parse_json(encoded,&roundtrip)==BVSTK_OK);
        assert(!memcmp(&roundtrip.endpoint,&profiles[i].endpoint,sizeof(roundtrip.endpoint)));
        assert(roundtrip.reg_count==profiles[i].reg_count);
        assert(roundtrip.whitelist_len==profiles[i].whitelist_len);
    }
}
static void routes_and_policy(void)
{
    uint8_t value=0;
    size_t id;
    topology();
    assert(bvstk_i2c_master_service_resolve_host(&master,0x51,&id)==BVSTK_ERR_NOT_FOUND);
    rd_switch(0x70,0); wr_switch(0x70,1); rd(0x51,2,0x42); wr_switch(0x70,0);
    assert(bvstk_i2c_master_service_read(&master,1,2,&value,20)==BVSTK_OK && value==0x42);
    assert(master.switch_known[0] && master.switch_value[0]==0);
    assert(bvstk_i2c_master_service_resolve_host(&master,0x51,&id)==BVSTK_ERR_NOT_FOUND);
    done(1);
    /* Switch writes are raw bytes, with no register prefix. */
    assert(bvstk_i2c_slave_service_select_address(&slave,0x70)==BVSTK_OK);
    uint8_t select=1;
    wr_switch(0x70,1);
    assert(host(&select,1,0,NULL)==BVSTK_OK);
    assert(bvstk_i2c_master_service_resolve_host(&master,0x51,&id)==BVSTK_OK && id==1);
    assert(!bvstk_i2c_cache_is_valid(&cache,0,0));
    done(1);
    select=3; /* default whitelist prohibits simultaneous channels */
    assert(host(&select,1,0,NULL)==BVSTK_ERR_DENIED && denials);
    done(0);
    assert(bvstk_i2c_slave_service_select_address(&slave,0x51)==BVSTK_OK);
    uint8_t pointer=2;
    assert(host(&pointer,1,0,NULL)==BVSTK_OK);
    /* A previously cached byte cannot satisfy a "fresh" read. */
    rd_switch(0x70,1);wr_switch(0x70,1);rd(0x51,2,0x43);wr_switch(0x70,1);
    assert(host(NULL,0,1,&value)==BVSTK_OK && value==0x43);
    done(1);
    /* Restoring a previous state is cleanup, including a target failure. */
    rd_switch(0x70,8);wr_switch(0x70,1);
    operation(0x51,1,3,0,0,1,0,BVSTK_ERR_IO);wr_switch(0x70,8);
    assert(bvstk_i2c_master_service_read(&master,1,3,&value,20)==BVSTK_ERR_IO);
    assert(master.switch_known[0] && master.switch_value[0]==8);
    done(1);
    rd_switch(0x70,8);operation(0x70,1,1,0,0,0,0,BVSTK_ERR_TIMEOUT);wr_switch(0x70,8);
    assert(bvstk_i2c_master_service_read(&master,1,3,&value,20)==BVSTK_ERR_TIMEOUT);
    done(1);
    rd_switch(0x70,8);wr_switch(0x70,1);rd(0x51,3,5);
    operation(0x70,1,8,0,0,0,0,BVSTK_ERR_IO);
    assert(bvstk_i2c_master_service_read(&master,1,3,&value,20)==BVSTK_ERR_IO);
    assert(!master.switch_known[0]);
    done(1);
    assert(bvstk_i2c_master_service_rule_clear_list(&master,0,I2C_POLICY_WHITELIST)==BVSTK_OK);
    unsigned transactions_before=endpoint_transactions;
    uint8_t presence_before=master.presence[1];
    rd_switch(0x70,0);
    assert(bvstk_i2c_master_service_read(&master,1,3,&value,20)==BVSTK_ERR_DENIED);
    assert(endpoint_transactions==transactions_before && master.presence[1]==presence_before);
    done(1);
    /* Invalid live rewiring leaves current routing untouched. */
    i2c_device_config_t changed=configs[1]; changed.endpoint.channel=2;
    assert(bvstk_i2c_master_service_set_config(&master,1,&changed)==BVSTK_ERR_UNSUPPORTED);
}
static void nested_and_ambiguous(void)
{
    uint8_t value;
    size_t id, route[BVSTK_I2C_ROUTE_MAX], depth;
    uint8_t channels[BVSTK_I2C_ROUTE_MAX];
    topology();
    configs[2]=profiles[0];
    strcpy(configs[2].name,"inner");
    configs[2].addr_7b=0x71;
    strcpy(configs[2].endpoint.parent,"bridge_x");
    configs[2].endpoint.channel=2;
    strcpy(configs[1].endpoint.parent,"inner");
    configs[1].endpoint.channel=1;
    init(3);
    assert(bvstk_i2c_devices_route(&devices,1,route,channels,&depth)==BVSTK_OK);
    assert(depth==2 && route[0]==0 && route[1]==2 && channels[0]==2 && channels[1]==1);
    rd_switch(0x70,0);wr_switch(0x70,4);
    rd_switch(0x71,0);wr_switch(0x71,2);rd(0x51,2,0x33);
    wr_switch(0x71,0);wr_switch(0x70,0);
    assert(bvstk_i2c_master_service_read(&master,1,2,&value,20)==BVSTK_OK);
    done(1);
    strcpy(devices.items[0].endpoint.parent,"inner");
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_ERR_MALFORMED);
    topology();
    configs[2]=configs[1];strcpy(configs[2].name,"second");configs[2].endpoint.channel=1;
    init(3);
    assert(bvstk_i2c_devices_find_by_addr(&devices,0x51,&id)==BVSTK_ERR_UNSUPPORTED);
    master.switch_known[0]=1;master.switch_value[0]=1;
    assert(bvstk_i2c_master_service_resolve_host(&master,0x51,&id)==BVSTK_OK && id==1);
    master.switch_value[0]=2;
    assert(bvstk_i2c_master_service_resolve_host(&master,0x51,&id)==BVSTK_OK && id==2);
    master.switch_value[0]=3;
    assert(bvstk_i2c_master_service_resolve_host(&master,0x51,&id)==BVSTK_ERR_UNSUPPORTED);
    devices.items[2].endpoint.channel=0;
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_ERR_MALFORMED);
    devices.items[2].endpoint.channel=4;
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_ERR_MALFORMED);
    strcpy(devices.items[2].endpoint.parent,"missing");
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_ERR_MALFORMED);
    topology();
    /* Encoded selectors use a match mask, not a hard-coded one-hot algorithm. */
    configs[0].max_value_code=255;configs[0].policy=I2C_POLICY_BLACKLIST;
    bvstk_i2c_switch_t *sw=&configs[0].endpoint.router;
    sw->select_mask=7;sw->preserve_mask=0x80;
    for (unsigned i=0;i<4;i++) sw->channels[i]=(bvstk_i2c_channel_t){(uint8_t)(4+i),7};
    init(4);
    rd_switch(0x70,0x85);wr_switch(0x70,0x84);rd(0x51,2,0x66);wr_switch(0x70,0x85);
    assert(bvstk_i2c_master_service_read(&master,1,2,&value,20)==BVSTK_OK);
    done(1);
    assert(bvstk_i2c_channel_selected(sw,1,0x85));
    assert(!bvstk_i2c_channel_selected(sw,0,0x85));
    /* Channel count and encoding are profile data as well. */
    configs[0].endpoint.router.channel_count=16;
    configs[0].endpoint.router.select_mask=31;
    configs[0].endpoint.router.preserve_mask=0;
    for (unsigned i=0;i<16;i++)
        configs[0].endpoint.router.channels[i]=(bvstk_i2c_channel_t){(uint8_t)(16+i),31};
    configs[1].endpoint.channel=15;
    init(2);
    rd_switch(0x70,0);wr_switch(0x70,31);rd(0x51,2,0x67);wr_switch(0x70,0);
    assert(bvstk_i2c_master_service_read(&master,1,2,&value,20)==BVSTK_OK);
    done(1);
    /* Four switch levels are supported; deeper chains fail validation. */
    for (size_t i=0;i<5;i++) {
        configs[i]=profiles[0];
        snprintf(configs[i].name,sizeof(configs[i].name),"sw_%u",(unsigned)i);
        configs[i].addr_7b=(uint8_t)(0x70+i);
        if (i) snprintf(configs[i].endpoint.parent,sizeof(configs[i].endpoint.parent),"sw_%u",(unsigned)i-1);
    }
    configs[5]=profiles[1];strcpy(configs[5].endpoint.parent,"sw_4");
    assert(bvstk_i2c_devices_init_from_config(&devices,configs,6)==BVSTK_OK);
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_ERR_MALFORMED);
    configs[4]=profiles[1];strcpy(configs[4].endpoint.parent,"sw_3");
    init(5);
    assert(bvstk_i2c_devices_route(&devices,4,route,channels,&depth)==BVSTK_OK && depth==4);
}
static void direct(const i2c_device_config_t *profile)
{
    configs[0]=*profile;
    configs[0].endpoint.parent[0]=0;configs[0].endpoint.channel=0;
    configs[0].policy=I2C_POLICY_BLACKLIST;configs[0].blacklist_len=0;
    init(1);
}
static void sequences_and_verification(void)
{
    uint8_t value;
    unsigned transactions_before;
    direct(&profiles[3]);
    assert(bvstk_i2c_master_service_write(&master,0,0,1,BVSTK_EVENT_SOURCE_HOST,20)==BVSTK_ERR_DENIED);
    done(0); /* Input ports are read-only even under a permissive policy. */
    const uint8_t gpio[]={3,0x21,0x32,0x43};
    wr(0x20,3,0x21);wr(0x20,2,0x32);wr(0x20,3,0x43);
    assert(host(gpio,sizeof(gpio),0,NULL)==BVSTK_OK);
    assert(slave.register_pointer[0][0]==2);
    done(3);
    assert(!bvstk_i2c_cache_is_valid(&cache,0,3));
    direct(&profiles[2]);
    const uint8_t memory[]={7,0x91,0x92};
    uint64_t before=now;
    wr(0x57,7,0x91);rd(0x57,7,0x91);wr(0x57,0,0x92);rd(0x57,0,0x92);
    assert(host(memory,sizeof(memory),0,NULL)==BVSTK_OK);
    assert(now-before==10 && slave.register_pointer[0][0]==1);
    assert(bvstk_i2c_cache_read(&cache,0,0,&value)==BVSTK_OK && value==0x92);
    done(2);
    wr(0x57,4,0x81);rd(0x57,4,0x11); /* ACK under WP is not a successful write */
    assert(bvstk_i2c_master_service_write(&master,0,4,0x81,BVSTK_EVENT_SOURCE_HOST,20)==BVSTK_ERR_IO);
    assert(!bvstk_i2c_cache_is_valid(&cache,0,4));
    done(1);
    direct(&profiles[1]);
    /* Background presence checks are physical reads, but stay out of the
     * user-facing telemetry journal. They still establish presence and cache
     * the selected safe resource. */
    transactions_before = endpoint_transactions;
    rd(0x51, profiles[1].presence_check_reg, 0x5A);
    assert(bvstk_i2c_master_service_probe(&master, 0,
        profiles[1].presence_check_reg, 20) == BVSTK_OK);
    assert(endpoint_transactions == transactions_before);
    assert(master.presence[0] == BVSTK_I2C_PRESENCE_RESPONDING);
    assert(bvstk_i2c_cache_read(&cache, 0, profiles[1].presence_check_reg,
        &value) == BVSTK_OK && value == 0x5A);
    done(1);
    operation(0x51, 1, profiles[1].presence_check_reg, 0, 0, 1, 0,
        BVSTK_ERR_IO);
    assert(bvstk_i2c_master_service_probe(&master, 0,
        profiles[1].presence_check_reg, 20) == BVSTK_ERR_IO);
    assert(master.presence[0] == BVSTK_I2C_PRESENCE_UNAVAILABLE);
    assert(!bvstk_i2c_cache_is_valid(&cache, 0, profiles[1].presence_check_reg));
    done(1);
    uint8_t pointer=15;
    assert(host(&pointer,1,0,NULL)==BVSTK_OK);
    rd(0x51,15,1);assert(host(NULL,0,1,&value)==BVSTK_OK);
    assert(slave.register_pointer[0][0]==0);done(1);
    configs[0].endpoint.transfer.read_sequence.mode=BVSTK_I2C_SEQUENCE_LINEAR;
    init(1);assert(host(&pointer,1,0,NULL)==BVSTK_OK);
    rd(0x51,15,1);assert(host(NULL,0,1,&value)==BVSTK_OK);done(1);
    assert(host(NULL,0,1,&value)==BVSTK_ERR_RANGE);done(0);
}
static void configurable_prefix(void)
{
    uint8_t value;
    direct(&profiles[1]);
    configs[0].endpoint.transfer.address_bytes=2;
    configs[0].endpoint.transfer.address_base=0x1200;
    configs[0].endpoint.transfer.invalidate_after_write=0;
    init(1);
    operation(0x51,2,0x12,3,0,1,0x45,BVSTK_OK);
    assert(bvstk_i2c_master_service_read(&master,0,3,&value,20)==BVSTK_OK && value==0x45);
    done(1);
    const uint8_t write[]={0x12,3,0x44};
    operation(0x51,3,0x12,3,0x44,0,0,BVSTK_OK);
    assert(host(write,sizeof(write),0,NULL)==BVSTK_OK);done(1);
    configs[0].endpoint.transfer.little_endian=1;
    configs[0].endpoint.transfer.stop_before_read=1;
    init(1);
    operation(0x51,2,3,0x12,0,0,0,BVSTK_OK);
    operation(0x51,0,0,0,0,1,0x46,BVSTK_OK);
    assert(bvstk_i2c_master_service_read(&master,0,3,&value,20)==BVSTK_OK && value==0x46);
    done(1);
    uint8_t bad_pointer[]={3,0x11};
    assert(host(bad_pointer,2,0,NULL)==BVSTK_ERR_RANGE);done(0);
    i2c_device_config_t changed=configs[0];changed.reg_count=8;
    assert(bvstk_i2c_master_service_set_config(&master,0,&changed)==BVSTK_ERR_UNSUPPORTED);
    configs[0].endpoint.transfer.address_bytes=3;
    assert(bvstk_i2c_endpoint_validate(&configs[0].endpoint,16)==BVSTK_ERR_RANGE);
}
static void multiple_spaces(const char *root)
{
    static char json[BVSTK_PROFILE_JSON_MAX+1], encoded[BVSTK_PROFILE_JSON_MAX+1];
    char path[512]; size_t n, id; uint8_t value, addresses[128], index;
    snprintf(path,sizeof(path),"%s/../tests/host/fixtures/device-multi-space.json",root);
    FILE *f=fopen(path,"rb"); assert(f);
    n=fread(json,1,sizeof(json)-1,f); assert(feof(f)); json[n]=0; fclose(f);
    assert(bvstk_i2c_config_parse_json(json,&configs[0])==BVSTK_OK);
    assert(configs[0].reg_count==8 && configs[0].endpoint.space_count==2);
    assert(configs[0].whitelist[0].reg==4 && configs[0].settings[0].reg==0);
    assert(bvstk_device_profile_format_i2c(&configs[0],encoded,sizeof(encoded),&n)==BVSTK_OK);
    i2c_device_config_t copy;
    assert(bvstk_i2c_config_parse_json(encoded,&copy)==BVSTK_OK);
    assert(!memcmp(&copy,&configs[0],sizeof(copy)));
    init(1);
    assert(bvstk_i2c_devices_addresses(&devices,addresses)==2 && addresses[0]==0x50 && addresses[1]==0x51);
    assert(bvstk_i2c_devices_find_by_addr(&devices,0x51,&id)==BVSTK_OK && id==0);
    assert(bvstk_i2c_address_register(&copy.endpoint,copy.addr_7b,8,0x51,0,&index) && index==4);
    assert(!bvstk_i2c_address_register(&copy.endpoint,copy.addr_7b,8,0x51,4,&index));
    rd(0x50,0,0x11); rd(0x51,0,0x22);
    assert(bvstk_i2c_master_service_read(&master,0,0,&value,20)==BVSTK_OK && value==0x11);
    assert(bvstk_i2c_master_service_read(&master,0,4,&value,20)==BVSTK_OK && value==0x22);
    assert(cache.values[0][0]==0x11 && cache.values[0][4]==0x22);
    done(2);
    assert(bvstk_i2c_master_service_write(&master,0,0,0x12,BVSTK_EVENT_SOURCE_HOST,20)==BVSTK_ERR_DENIED);
    wr(0x51,0,0x12);
    assert(bvstk_i2c_master_service_write(&master,0,4,0x12,BVSTK_EVENT_SOURCE_HOST,20)==BVSTK_OK);
    assert(cache.values[0][0]==0x11 && cache.values[0][4]==0x12); done(1);
    /* Pointer and wrap are local to a selected I2C address. */
    uint8_t p=3;
    assert(bvstk_i2c_slave_service_select_address(&slave,0x51)==BVSTK_OK);
    assert(host(&p,1,0,NULL)==BVSTK_OK);
    p=1;
    assert(bvstk_i2c_slave_service_select_address(&slave,0x50)==BVSTK_OK);
    assert(host(&p,1,0,NULL)==BVSTK_OK);
    assert(bvstk_i2c_slave_service_select_address(&slave,0x51)==BVSTK_OK);
    rd(0x51,3,0x33); assert(host(NULL,0,1,&value)==BVSTK_OK && value==0x33);
    assert(slave.register_pointer[0][1]==0 && slave.register_pointer[0][0]==1);
    assert(bvstk_i2c_slave_service_select_address(&slave,0x50)==BVSTK_OK);
    rd(0x50,1,0x44); assert(host(NULL,0,1,&value)==BVSTK_OK && value==0x44);
    done(2);
    operation(0x51,1,1,0,0,1,0,BVSTK_ERR_IO);
    assert(bvstk_i2c_master_service_read(&master,0,5,&value,20)==BVSTK_ERR_IO);
    assert(master.presence[0]==BVSTK_I2C_PRESENCE_RESPONDING);
    assert(master.space_presence[0][1]==BVSTK_I2C_PRESENCE_UNAVAILABLE); done(1);
    /* Both addresses follow the same route and both participate in collisions. */
    configs[1]=configs[0]; configs[0]=profiles[0];
    strcpy(configs[1].endpoint.parent,configs[0].name); configs[1].endpoint.channel=1;
    init(2);
    rd_switch(0x70,0);wr_switch(0x70,2);rd(0x51,0,0x55);wr_switch(0x70,0);
    assert(bvstk_i2c_master_service_read(&master,1,4,&value,20)==BVSTK_OK && value==0x55); done(1);
    configs[2]=profiles[1];configs[2].endpoint.channel=1;
    assert(bvstk_i2c_devices_init_from_config(&devices,configs,3)==BVSTK_OK);
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_ERR_MALFORMED);
    configs[2].endpoint.channel=0;
    assert(bvstk_i2c_devices_init_from_config(&devices,configs,3)==BVSTK_OK);
    assert(bvstk_i2c_devices_validate_topology(&devices)==BVSTK_OK);
    copy.endpoint.spaces[1].address=0x50;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    copy=configs[1]; copy.endpoint.spaces[1].first=3;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    copy=configs[1]; strcpy(copy.endpoint.spaces[1].id,"identity");
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    /* Missing/unknown spaces and duplicate (space, offset) fail closed. */
    char *where=strstr(json,"\"space\": \"diagnostics\""); assert(where);
    memcpy(where+10,"identities ",11);
    assert(bvstk_i2c_config_parse_json(json,&copy)!=BVSTK_OK);
    /* Full-size two-window profiles use every cache index, including 255. */
    configs[0]=configs[1];
    configs[0].endpoint.parent[0]=0;configs[0].endpoint.channel=0;
    configs[0].reg_count=256;
    configs[0].endpoint.spaces[0].count=128;
    configs[0].endpoint.spaces[1].first=128;
    configs[0].endpoint.spaces[1].count=128;
    memset(&configs[0].profile,0,sizeof(configs[0].profile));
    assert(bvstk_device_profile_format_i2c(&configs[0],encoded,sizeof(encoded),&n)==BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(encoded,&copy)==BVSTK_OK && copy.reg_count==256);
    assert(bvstk_i2c_address_register(&copy.endpoint,copy.addr_7b,256,0x51,127,&index) && index==255);
    init(1);
    rd(0x51,127,0xab);
    assert(bvstk_i2c_master_service_read(&master,0,255,&value,20)==BVSTK_OK && value==0xab);
    done(1);
    copy.endpoint.spaces[1].count=129;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    copy=configs[0];copy.endpoint.space_count=BVSTK_ADDRESS_SPACES_MAX+1;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    copy=configs[0];copy.endpoint.transfer.configured=0;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    copy=configs[0];copy.endpoint.transfer.address_bytes=0;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
    copy=configs[0];copy.endpoint.transfer.read_sequence.span=3;
    assert(bvstk_i2c_config_validate(&copy)!=BVSTK_OK);
}

static void block_sampling(void)
{
    /* Config from multiple_spaces: one endpoint, two 128-byte windows. */
    configs[1]=configs[0]; configs[0]=profiles[0];
    strcpy(configs[1].endpoint.parent,configs[0].name); configs[1].endpoint.channel=1;
    configs[1].endpoint.transfer.read_sequence.mode=BVSTK_I2C_SEQUENCE_LINEAR;
    configs[1].endpoint.transfer.read_sequence.span=0;
    init(2);
    now=1234;
    unsigned before=endpoint_transactions;
    rd_switch(0x70,4);wr_switch(0x70,2);
    operation(0x51,1,96,0,0,16,0x10,BVSTK_OK);wr_switch(0x70,4);
    assert(bvstk_i2c_master_service_sample(&master,1,224,16,20)==BVSTK_OK);
    done(1);
    for (unsigned i=0;i<16;++i) {
        assert(bvstk_i2c_cache_is_valid(&cache,1,224+i));
        assert(cache.values[1][224+i]==0x10+i && cache.updated_ms[1][224+i]==1234);
        assert(cache.read_id[1][224+i] != 0 && cache.read_id[1][224+i] == cache.read_id[1][224]);
    }
    assert(commits==0 && endpoint_transactions==before);
    assert(bvstk_i2c_master_service_sample(&master,1,120,16,20)!=BVSTK_OK);
    assert(bvstk_i2c_master_service_sample(&master,1,224,17,20)!=BVSTK_OK);
    assert(bvstk_i2c_master_service_sample(&master,1,256,1,20)!=BVSTK_OK);
    done(0);
    rd_switch(0x70,4);wr_switch(0x70,2);
    operation(0x51,1,96,0,0,16,0,BVSTK_ERR_IO);wr_switch(0x70,4);
    assert(bvstk_i2c_master_service_sample(&master,1,224,16,20)==BVSTK_ERR_IO);
    for (unsigned i=0;i<16;++i) {
        assert(!bvstk_i2c_cache_is_valid(&cache,1,224+i));
        assert(cache.read_id[1][224+i] == 0);
    }
    done(1);
    assert(master.switch_known[0] && master.switch_value[0]==4);
}

static void rtc_sampling(void)
{
    topology(); now = 2000; wire_delay = 3;
    rd_switch(0x70, 2); wr_switch(0x70, 1);
    operation(0x51, 1, 0, 0, 0, 16, 0x10, BVSTK_OK); wr_switch(0x70, 2);
    assert(bvstk_i2c_master_service_sample(&master, 1, 0, 16, 100) == BVSTK_OK);
    done(1);
    bvstk_i2c_cache_entry_t entry;
    for (unsigned r = 0; r < 16; ++r) {
        assert(bvstk_i2c_cache_entry(&cache, 1, r, &entry) == BVSTK_OK);
        assert(entry.valid && !entry.seeded && entry.value == 0x10+r);
        assert(entry.updated_ms == 2012 && entry.read_duration_ms == 12 && entry.read_id);
        assert(entry.read_id == cache.read_id[1][0]);
    }
    uint32_t group = entry.read_id;
    /* A scalar update at the same millisecond still breaks coherence. */
    assert(bvstk_i2c_cache_write(&cache, 1, 2, 0x12) == BVSTK_OK);
    assert(cache.updated_ms[1][2] == cache.updated_ms[1][3] && !cache.read_id[1][2]);
    assert(bvstk_i2c_cache_seed(&cache, 1, 3, 0x13) == BVSTK_OK && !cache.read_id[1][3]);
    const uint8_t bytes[] = {0, 1, 2};
    assert(bvstk_i2c_cache_read_block(&cache, 1, 2, bytes, 3, now, 0) == BVSTK_OK);
    assert(cache.read_id[1][2] != group && cache.read_id[1][2] == cache.read_id[1][4]);
    assert(bvstk_i2c_cache_read_block(&cache, 1, 255, bytes, 3, now, 0) == BVSTK_ERR_RANGE);
    cache.next_read_id = UINT32_MAX;
    assert(bvstk_i2c_cache_read_block(&cache, 1, 0, bytes, 3, now, 0) == BVSTK_OK);
    assert(cache.read_id[1][0] == 1 && cache.read_id[1][3] == 0 && cache.valid[1][3]);
    assert(commits == 0); wire_delay = 0;
}

static void eeprom_sampling(void)
{
    topology();
    for (unsigned i = 0; i < 256; ++i) {
        now = 10000U + i * 10U;
        rd_switch(0x70, 2); wr_switch(0x70, 1);
        rd(0x57, (uint8_t)i, (uint8_t)(255U-i)); wr_switch(0x70, 2);
        assert(bvstk_i2c_master_service_sample(&master, 2, i, 1, 20) == BVSTK_OK);
        done(1);
        assert(bvstk_i2c_cache_is_valid(&cache, 2, i));
        assert(cache.values[2][i] == 255U-i && cache.updated_ms[2][i] == now);
        assert(master.switch_known[0] && master.switch_value[0] == 2);
    }
    assert(commits == 0);
    assert(bvstk_i2c_master_service_sample(&master, 2, 256, 1, 20) == BVSTK_ERR_RANGE);
    done(0);
}

static void switch_observations(void)
{
    bvstk_i2c_switch_observation_t sample;
    uint8_t value;
    topology();
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK && !sample.valid);
    assert(bvstk_i2c_master_service_switch_observation(&master,1,&sample)==BVSTK_ERR_UNSUPPORTED);
    assert(bvstk_i2c_master_service_switch_observation(&master,0,NULL)==BVSTK_ERR_MALFORMED);
    assert(bvstk_i2c_cache_seed(&cache,0,0,0xff)==BVSTK_OK);
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK && !sample.valid);
    now=1234;
    rd_switch(0x70,0x90);wr_switch(0x70,1);rd(0x51,2,0x42);wr_switch(0x70,0);
    assert(bvstk_i2c_master_service_read(&master,1,2,&value,20)==BVSTK_OK);
    done(1);
    assert(!bvstk_i2c_cache_is_valid(&cache,0,0));
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK);
    assert(sample.valid && sample.value==0x90 && sample.updated_ms==1234);
    now=2345;
    wr_switch(0x70,2);
    assert(bvstk_i2c_master_service_write(&master,0,0,2,BVSTK_EVENT_SOURCE_CONSOLE,20)==BVSTK_OK);
    done(1);
    operation(0x70,0,0,0,0,1,0,BVSTK_ERR_IO);
    assert(bvstk_i2c_master_service_probe(&master,0,0,20)==BVSTK_ERR_IO);
    done(1);
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK);
    assert(sample.valid && sample.value==0x90 && sample.updated_ms==1234);
    rd_switch(0x70,0xff);
    assert(bvstk_i2c_master_service_probe(&master,0,0,20)==BVSTK_OK);
    done(1);
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK);
    assert(sample.valid && sample.value==0xff && sample.updated_ms==2345);
    assert(bvstk_i2c_master_service_set_policy(&master,0,I2C_POLICY_BLACKLIST)==BVSTK_OK);
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK && sample.valid);
    topology();
    assert(bvstk_i2c_master_service_switch_observation(&master,0,&sample)==BVSTK_OK && !sample.valid);
    done(0); /* All observation getters are memory-only. */
}

int main(int argc, char **argv)
{
    assert(argc==2);
    profiles_load(argv[1]);
    routes_and_policy();
    nested_and_ambiguous();
    sequences_and_verification();
    configurable_prefix();
    multiple_spaces(argv[1]);
    block_sampling();
    rtc_sampling();
    eeprom_sampling();
    switch_observations();
    puts("I2C topology: generic profiles, atomic routes/restoration, policies, duplicate addresses, sequences and write verification passed");
    return 0;
}
