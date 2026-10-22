#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "services/acquisition/bvstk_acquisition.h"
#include "shared/config/bvstk_device_profile.h"
#include "shared/config/bvstk_i2c_config_codec.h"

static unsigned reads, first_read, count_read, invalidations;
static int ready = 1, has_condition = 1;
static uint16_t condition_value = 0x40;
static bvstk_status_t result;
static int is_ready(void *ctx) { (void)ctx; return ready; }
static int cached(void *ctx, unsigned r, uint16_t *v)
{ (void)ctx; assert(r == 3); *v = condition_value; return has_condition; }
static bvstk_status_t read_chunk(void *ctx, unsigned first, unsigned count)
{ (void)ctx; ++reads; first_read = first; count_read = count; return result; }
static void invalidate(void *ctx, unsigned first, unsigned count)
{ (void)ctx; (void)first; invalidations += count; }

static void scheduler(void)
{
    bvstk_acquisition_plan_t p = {.enabled=1, .group_count=2, .read_delay_ms=10, .retry_delay_ms=100};
    p.groups[0] = (bvstk_acquisition_group_t){.interval_ms=0, .chunk_size=2, .range_count=1,
        .ranges={{0,4}}};
    p.groups[1] = (bvstk_acquisition_group_t){.interval_ms=1000, .chunk_size=2, .range_count=1,
        .ranges={{16,4}}, .condition_count=1, .conditions={{3,0x44,0x40}}};
    bvstk_acquisition_state_t s = {0};
    const bvstk_acquisition_io_t io = {NULL,is_ready,cached,read_chunk,invalidate};
    assert(bvstk_acquisition_step(&p,&s,&io,0) && first_read == 0 && count_read == 2);
    assert(!bvstk_acquisition_step(&p,&s,&io,9));
    assert(bvstk_acquisition_step(&p,&s,&io,10) && first_read == 16);
    assert(bvstk_acquisition_step(&p,&s,&io,20) && first_read == 2);
    result = BVSTK_ERR_IO;
    assert(bvstk_acquisition_step(&p,&s,&io,30) && first_read == 18);
    assert(!bvstk_acquisition_step(&p,&s,&io,100));
    result = BVSTK_OK;
    assert(bvstk_acquisition_step(&p,&s,&io,130) && first_read == 18);
    assert(!bvstk_acquisition_step(&p,&s,&io,500));
    assert(bvstk_acquisition_step(&p,&s,&io,1130) && first_read == 16);
    has_condition = 0;
    assert(!bvstk_acquisition_step(&p,&s,&io,1140) && invalidations == 4);
    has_condition = 1; condition_value = 0x44;
    assert(!bvstk_acquisition_step(&p,&s,&io,1150));
    condition_value = 0x40;
    assert(bvstk_acquisition_step(&p,&s,&io,1160) && first_read == 16);
    ready = -1;
    assert(!bvstk_acquisition_step(&p,&s,&io,1170) && s.connected);
    ready = 0;
    assert(!bvstk_acquisition_step(&p,&s,&io,1180) && !s.connected && invalidations == 12);
    assert(!bvstk_acquisition_step(&p,&s,&io,1200) && invalidations == 12);
    ready = 1;
    assert(bvstk_acquisition_step(&p,&s,&io,1210) && first_read == 0);
}

static char input[65537], output[65537], changed[65537];
static i2c_device_config_t config, saved;
static void reject(const char *old, const char *replacement)
{
    const char *p = strstr(input,old); assert(p);
    size_t n = (size_t)(p-input); memcpy(changed,input,n);
    strcpy(changed+n,replacement); strcat(changed,p+strlen(old));
    assert(bvstk_i2c_config_parse_json(changed,&saved) != BVSTK_OK);
}
static void eeprom_profile(const char *path)
{
    FILE *f = fopen(path, "rb"); assert(f);
    size_t n = fread(input, 1, sizeof(input)-1, f); assert(feof(f)); input[n] = 0; fclose(f);
    assert(bvstk_i2c_config_parse_json(input, &config) == BVSTK_OK);
    const bvstk_acquisition_plan_t *p = &config.profile.acquisition;
    assert(p->enabled && p->group_count == 1 && p->groups[0].condition_count == 0);
    assert(p->groups[0].range_count == 1 && p->groups[0].ranges[0].first == 0 &&
           p->groups[0].ranges[0].count == 256 && p->groups[0].chunk_size == 1);
    assert(p->read_delay_ms == 10 && p->groups[0].interval_ms == 5000);
    assert(config.endpoint.transfer.read_sequence.mode == BVSTK_I2C_SEQUENCE_WRAP);
    assert(config.endpoint.transfer.write_sequence.span == 8 && config.whitelist_len == 0);
    assert(bvstk_device_profile_format_i2c(&config, output, sizeof(output), &n) == BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(output, &saved) == BVSTK_OK);
    assert(!memcmp(p, &saved.profile.acquisition, sizeof(*p)));
    bvstk_acquisition_state_t state = {0};
    const bvstk_acquisition_io_t io = {NULL, is_ready, cached, read_chunk, invalidate};
    ready = 1; result = BVSTK_OK; reads = invalidations = 0;
    for (unsigned i = 0; i < 256; ++i) {
        assert(bvstk_acquisition_step(p, &state, &io, i * 10U));
        assert(first_read == i && count_read == 1);
    }
    assert(reads == 256 && !bvstk_acquisition_step(p, &state, &io, 7549));
    assert(bvstk_acquisition_step(p, &state, &io, 7550) && first_read == 0);
    ready = 0;
    assert(!bvstk_acquisition_step(p, &state, &io, 7560) && invalidations == 256);
    ready = 1;
    assert(bvstk_acquisition_step(p, &state, &io, 7570) && first_read == 0);
}

static void rtc_profile(const char *path)
{
    FILE *f = fopen(path, "rb"); assert(f);
    size_t n = fread(input, 1, sizeof(input)-1, f); assert(feof(f)); input[n] = 0; fclose(f);
    assert(bvstk_i2c_config_parse_json(input, &config) == BVSTK_OK);
    const bvstk_acquisition_plan_t *p = &config.profile.acquisition;
    assert(p->enabled && p->group_count == 1 && p->groups[0].chunk_size == 16);
    assert(p->groups[0].range_count == 1 && p->groups[0].ranges[0].first == 0 &&
           p->groups[0].ranges[0].count == 16 && p->groups[0].interval_ms == 1000);
    assert(config.endpoint.transfer.read_sequence.mode == BVSTK_I2C_SEQUENCE_WRAP);
    assert(config.endpoint.transfer.address_bytes == 1 && !config.endpoint.transfer.stop_before_read);
    assert(config.whitelist_len == 0 && config.settings_len == 0);
    assert(bvstk_device_profile_format_i2c(&config, output, sizeof(output), &n) == BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(output, &saved) == BVSTK_OK);
    assert(!memcmp(p, &saved.profile.acquisition, sizeof(*p)));
    reject("\"mode\": \"wrap\",\n        \"span\": 0", "\"mode\": \"wrap\",\n        \"span\": 8");
    reject("\"mode\": \"wrap\",\n        \"span\": 0", "\"mode\": \"fixed\"");
    bvstk_acquisition_state_t state = {0};
    const bvstk_acquisition_io_t io = {NULL, is_ready, cached, read_chunk, invalidate};
    ready = 1; result = BVSTK_OK; reads = invalidations = 0;
    assert(bvstk_acquisition_step(p, &state, &io, 0) && first_read == 0 && count_read == 16);
    assert(!bvstk_acquisition_step(p, &state, &io, 999));
    assert(bvstk_acquisition_step(p, &state, &io, 1000) && reads == 2);
    ready = 0;
    assert(!bvstk_acquisition_step(p, &state, &io, 1001) && invalidations == 16);
    ready = 1;
    assert(bvstk_acquisition_step(p, &state, &io, 1002) && first_read == 0 && count_read == 16);
}

int main(int argc, char **argv)
{
    assert(argc == 4); FILE *f=fopen(argv[1],"rb"); assert(f);
    size_t n=fread(input,1,sizeof(input)-1,f); assert(feof(f)); input[n]=0; fclose(f);
    assert(bvstk_i2c_config_parse_json(input,&config)==BVSTK_OK);
    assert(config.profile.acquisition.enabled && config.profile.acquisition.group_count==4);
    assert(config.profile.acquisition.groups[2].ranges[0].first==224);
    assert(bvstk_device_profile_format_i2c(&config,output,sizeof(output),&n)==BVSTK_OK);
    assert(bvstk_i2c_config_parse_json(output,&saved)==BVSTK_OK);
    assert(!memcmp(&config.profile.acquisition,&saved.profile.acquisition,sizeof(saved.profile.acquisition)));
    reject("\"first\": \"a0_00\", \"count\": 128", "\"first\": \"a0_00\", \"count\": 129");
    reject("\"chunk_size\": 2", "\"chunk_size\": 17");
    reject("\"read_delay_ms\": 10", "\"read_delay_ms\": 0");
    reject("\"interval_ms\": 1000", "\"interval_ms\": 1");
    reject("\"first\": \"a2_60\"", "\"first\": \"missing\"");
    reject("\"mask\": \"0x44\"", "\"mask\": \"0x00\"");
    reject("\"first\": \"a2_78\"", "\"first\": \"a2_70\"");
    reject("\"presence_check_enabled\": true", "\"presence_check_enabled\": false");
    reject("\"mode\": \"linear\"", "\"mode\": \"fixed\"");
    scheduler();
    eeprom_profile(argv[2]);
    rtc_profile(argv[3]);
    puts("Acquisition: SFP, EEPROM and RTC profiles, full scans, bounds, fair chunks, retries and reconnect passed");
    return 0;
}
