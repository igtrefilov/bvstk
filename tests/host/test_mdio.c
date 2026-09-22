#include "services/mdio/bvstk_mdio_service.h"
#include "hardware/pl/mdio/bvstk_mdio_regs.h"
#include "shared/cli/bvstk_mdio_completion.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t master[16384], slave[16384], bram[4096];
static uint16_t phy[32][32];
static uint64_t now;
static uint32_t frame;
static int pending, suppress_result, wrong_address, ack_armed;
static unsigned writes, resets, depth, locks;
static bvstk_mdio_core_t core;
static bvstk_mdio_service_t service;
static mdio_phy_config_t config;

uint64_t bvstk_platform_now_ms(void) { return now; }
void bvstk_platform_sleep_ms(uint32_t ms)
{
    now += ms;
    if (pending && !suppress_result) {
        unsigned p = (frame >> 21) & 31U, r = (frame >> 16) & 31U;
        size_t index = p * 32U + r;
        pending = 0;
        master[0] = 4U;
        if (frame & (1U << 26)) {
            ++writes;
            phy[p][r] = (uint16_t)frame;
            if (r == 0 && (frame & 0x8000U)) phy[p][r] = 0x3100U; /* self clearing RESET */
        } else {
            bram[index] = phy[p][r];
            master[4] = BVSTK_MDIO_BRAM_BASE + (uint32_t)index * 4U + (wrong_address ? 4U : 0U);
            master[0] |= 1U;
        }
    }
}
int bvstk_mmio_region_open(bvstk_mmio_region_t *r, uintptr_t base, size_t size)
{
    r->physical_base = base; r->size = size;
    r->mapped_base = (volatile uint8_t *)(base == BVSTK_MDIO_MASTER_BASE ? master :
        base == BVSTK_MDIO_SLAVE_BASE ? slave : base == BVSTK_MDIO_BRAM_BASE ? bram : NULL);
    return r->mapped_base ? 0 : -1;
}
void bvstk_mmio_region_close(bvstk_mmio_region_t *r) { memset(r, 0, sizeof(*r)); }
int bvstk_mmio_read32(const bvstk_mmio_region_t *r, size_t offset, uint32_t *v)
{
    if (!r || !r->mapped_base || offset >= r->size || (offset & 3U)) return -1;
    *v = ((volatile uint32_t *)r->mapped_base)[offset / 4U];
    if (r->physical_base == BVSTK_MDIO_MASTER_BASE && offset == 16U) ack_armed = 1;
    return 0;
}
int bvstk_mmio_write32(const bvstk_mmio_region_t *r, size_t offset, uint32_t v)
{
    if (!r || !r->mapped_base || offset >= r->size || (offset & 3U)) return -1;
    if (r->physical_base == BVSTK_MDIO_MASTER_BASE) {
        if (offset == 0U) { ++resets; pending = 0; master[0] = 4U; return 0; }
        if (offset == 8U) {
            assert(ack_armed); ack_armed = 0; master[0] &= ~1U; return 0;
        }
        if (offset == 12U) { frame = v; pending = 1; master[0] = 2U; return 0; }
    }
    ((volatile uint32_t *)r->mapped_base)[offset / 4U] = v;
    return 0;
}
static bvstk_status_t lock_service(void *ctx, uint32_t timeout)
{
    (void)ctx; (void)timeout;
    assert(!depth); ++depth; ++locks; return BVSTK_OK;
}
static void unlock_service(void *ctx) { (void)ctx; assert(depth == 1U); --depth; }
static void host_write(unsigned p, unsigned r, uint16_t v) { bram[1024U + p * 32U + r] = v; }
static uint16_t host_read(unsigned p, unsigned r) { return (uint16_t)bram[2048U + p * 32U + r]; }

static const char json[] =
    "{\"name\":\"lan8720\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\","
    "\"autopoll_enabled\":true,\"autopoll_cycle_delay_ms\":50,\"autopoll_regs\":[0,1,4,5,31],"
    "\"whitelist\":[{\"reg\":0,\"val\":0,\"mask\":0},{\"reg\":4,\"val\":481}],"
    "\"blacklist\":[],\"settings\":[{\"reg\":17,\"val\":55}]}";

static void codec_tests(void)
{
    mdio_phy_config_t c, roundtrip;
    char out[8192];
    size_t len;
    assert(bvstk_mdio_config_parse(json, &c) == BVSTK_OK);
    assert(bvstk_mdio_policy_allows(&c, 0, 0x8000));
    assert(bvstk_mdio_policy_allows(&c, 4, 481));
    assert(!bvstk_mdio_policy_allows(&c, 4, 482));
    assert(!bvstk_mdio_policy_allows(&c, 5, 481));
    assert(bvstk_mdio_config_format(&c, out, sizeof(out), &len) == BVSTK_OK && len == strlen(out));
    assert(bvstk_mdio_config_parse(out, &roundtrip) == BVSTK_OK);
    assert(!memcmp(&c, &roundtrip, sizeof(c)));
    assert(bvstk_mdio_config_format(&c, out, 5U, &len) == BVSTK_ERR_RANGE && len == 0U);
    assert(bvstk_mdio_config_parse(
        "{\"name\":\"phy\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"blacklist\","
        "\"write_allow_regs\":[0,4],\"write_deny_regs\":[31]}", &c) == BVSTK_OK);
    assert(c.whitelist_len == 2U && c.whitelist[1].mask == 0U && c.blacklist[0].reg == 31U);
    assert(!bvstk_mdio_policy_allows(&c, 31, 1234) && bvstk_mdio_policy_allows(&c, 4, 65535));
    c.policy = (mdio_policy_t)99;
    assert(!bvstk_mdio_policy_allows(&c, 0, 0) && bvstk_mdio_config_validate(&c) != BVSTK_OK);
    {
        const char *invalid[] = {
            "{}", "{\"name\":\"p\",\"name\":\"q\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\"}",
            "{\"name\":\"p\",\"phy_addr\":32,\"reg_count\":32,\"policy\":\"whitelist\"}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"allow\"}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\",}",
            "{\"name\":\"../p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\"}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\",\"whitelist\":[{\"reg\":0,\"val\":65536}]}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\",\"whitelist\":[],\"write_allow_regs\":[0]}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\",\"autopoll_regs\":[0,0]}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\",\"autopoll_enabled\":trueX}",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\"} garbage",
            "{\"name\":\"p\",\"phy_addr\":1,\"reg_count\":32,\"policy\":\"whitelist\",\"autopoll_reg_delay_ms\":999999999999999999999}"
        };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            memset(&roundtrip, 0x5A, sizeof(roundtrip));
            assert(bvstk_mdio_config_parse(invalid[i], &roundtrip) != BVSTK_OK);
            assert((unsigned char)roundtrip.name[0] == 0x5A);
        }
    }
    /* Every truncated prefix fails, without silently accepting partial rules. */
    for (size_t i = 0; i < strlen(json); ++i) {
        memcpy(out, json, i); out[i] = '\0';
        assert(bvstk_mdio_config_parse(out, &c) != BVSTK_OK);
    }
    memset(&c, 0, sizeof(c));
    strcpy(c.name, "maximum"); c.reg_count = 32; c.phy_addr = 31;
    c.whitelist_len = c.blacklist_len = MDIO_CFG_RULES_MAX;
    c.settings_len = c.autopoll_regs_len = 32;
    for (size_t i = 0; i < 32; ++i) { c.settings[i].reg = (uint8_t)i; c.autopoll_regs[i] = (uint8_t)i; }
    for (size_t i = 0; i < MDIO_CFG_RULES_MAX; ++i) {
        c.whitelist[i].reg = c.blacklist[i].reg = (uint8_t)(i % 32);
        c.whitelist[i].val = c.blacklist[i].val = 65535;
        c.whitelist[i].mask = c.blacklist[i].mask = 65535;
    }
    assert(bvstk_mdio_config_format(&c, out, sizeof(out), &len) == BVSTK_OK);
    assert(bvstk_mdio_config_parse(out, &roundtrip) == BVSTK_OK);
    c.whitelist_len++;
    assert(bvstk_mdio_config_validate(&c) != BVSTK_OK);
}
static void completion_tests(void)
{
    bvstk_mdio_completion_device_t d = {"lan8720", 1};
    bvstk_mdio_completion_result_t r;
    const char *line = "mdio lan8720 policy whitelist a";
    assert(bvstk_mdio_complete_line(line, strlen(line), strlen(line), &d, 1, &r));
    assert(r.match_count == 1 && !strcmp(r.candidates[0], "add"));
    line = "mdio lan";
    assert(bvstk_mdio_complete_line(line, strlen(line), strlen(line), &d, 1, &r));
    assert(r.match_count == 1 && !strcmp(r.candidates[0], "lan8720"));
    line = "mdio @";
    assert(bvstk_mdio_complete_line(line, strlen(line), strlen(line), &d, 1, &r));
    assert(!strcmp(r.candidates[0], "@1"));
    line = "mdio lan8720 settings ";
    assert(bvstk_mdio_complete_line(line, strlen(line), strlen(line), &d, 1, &r) && r.match_count == 4U);
    line = "mdio lan8720 cache inv trailing";
    assert(bvstk_mdio_complete_line(line, strlen(line), 21U, &d, 1, &r));
    assert(!strcmp(r.candidates[0], "invalidate"));
    line = "mdio lan8720 policy whitelist add 0 ";
    assert(!bvstk_mdio_complete_line(line, strlen(line), strlen(line), &d, 1, &r));
    line = "smi ";
    assert(!bvstk_mdio_complete_line(line, strlen(line), strlen(line), &d, 1, &r));
}
static void driver_tests(void)
{
    uint16_t v = 0;
    uint64_t start;
    phy[1][2] = 7; phy[1][3] = 0xC0F1;
    assert(bvstk_mdio_core_init(&core, NULL, NULL) == BVSTK_OK);
    assert(core.master.physical_base == 0x43C10000U && core.slave.physical_base == 0x43C20000U);
    assert(bvstk_mdio_core_read(&core, 1, 2, &v, 5) == BVSTK_OK && v == 7);
    assert(bvstk_mdio_core_read(&core, 1, 3, &v, 5) == BVSTK_OK && v == 0xC0F1);
    assert(bvstk_mdio_core_read(&core, 32, 0, &v, 5) == BVSTK_ERR_RANGE);
    wrong_address = 1; v = 123;
    assert(bvstk_mdio_core_read(&core, 1, 2, &v, 5) == BVSTK_ERR_IO && v == 123);
    wrong_address = 0; suppress_result = 1; start = now;
    assert(bvstk_mdio_core_read(&core, 1, 2, &v, 5) == BVSTK_ERR_TIMEOUT);
    assert(now - start <= 7U && !pending);
    suppress_result = 0;
    assert(bvstk_mdio_core_read(&core, 1, 2, &v, 5) == BVSTK_OK && v == 7);
    suppress_result = 1;
    assert(bvstk_mdio_core_write(&core, 1, 4, 999, 5) == BVSTK_ERR_TIMEOUT);
    suppress_result = 0;
    assert(writes == 0U); /* No automatic write retry. */
    assert(bvstk_mdio_core_write(&core, 1, 4, 481, 5) == BVSTK_OK && phy[1][4] == 481);
}
static void service_tests(void)
{
    bvstk_mdio_stats_t stats;
    bvstk_mdio_cache_entry_t e;
    mdio_phy_config_t c;
    uint16_t v;
    unsigned count;
    size_t id;
    assert(bvstk_mdio_config_parse(json, &config) == BVSTK_OK);
    assert(bvstk_mdio_service_init(&service, &core, NULL, &config, 1, NULL) == BVSTK_OK);
    service.mutex = (bvstk_mutex_t){NULL, lock_service, unlock_service};
    assert(host_read(0, 2) == 0xFFFFU && host_read(1, 2) == 0xFFFFU);
    assert(host_read(1, 17) == 55U);
    assert(bvstk_mdio_service_cache_entry(&service, 0, 17, &e) == BVSTK_OK && e.seeded);
    assert(bvstk_mdio_service_read_cached(&service, 0, 2, &v) == BVSTK_ERR_NOT_READY);
    assert(bvstk_mdio_service_find_by_phy(&service, 33, &id) == BVSTK_ERR_RANGE);
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK);
    assert(host_read(1, 2) == 7 && host_read(1, 3) == 0xC0F1);

    count = writes;
    assert(bvstk_mdio_service_write(&service, 0, 4, 482, BVSTK_EVENT_SOURCE_CONSOLE, 5) == BVSTK_ERR_DENIED);
    host_write(1, 4, 482);
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK);
    assert(writes == count && phy[1][4] == 481);
    host_write(1, 4, 481);
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK && writes == ++count);
    host_write(1, 4, 481); /* Equal-value repetition must still be forwarded. */
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK && writes == ++count);
    host_write(2, 0, 0x8000);
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK && writes == count);
    host_write(1, 0, 0x8000);
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK);
    assert(host_read(1, 0) == 0x3100U); /* Not cached command 0x8000. */
    assert(bvstk_mdio_service_get_config(&service, 0, &c) == BVSTK_OK);
    assert(c.settings_len == 1 && c.settings[0].reg == 17); /* No persisted transient commands. */

    c.autopoll_enabled = false;
    assert(bvstk_mdio_service_set_config(&service, 0, &c) == BVSTK_OK);
    count = service.stats.reads;
    assert(bvstk_mdio_service_step(&service, now + 10000U, 5) == BVSTK_OK && service.stats.reads == count + 2U);
    suppress_result = 1;
    assert(bvstk_mdio_service_read(&service, 0, 4, &v, 5) == BVSTK_ERR_TIMEOUT);
    assert(bvstk_mdio_service_read_cached(&service, 0, 4, &v) == BVSTK_ERR_NOT_READY);
    assert(host_read(1, 4) == 0xFFFFU);
    suppress_result = 0;
    c.policy = MDIO_POLICY_BLACKLIST;
    c.blacklist_len = 1; c.blacklist[0] = (mdio_rule_entry_t){4, 0x8000, 0x8000};
    assert(bvstk_mdio_service_set_config(&service, 0, &c) == BVSTK_OK);
    assert(bvstk_mdio_service_write(&service, 0, 4, 0x8001, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_ERR_DENIED);
    assert(bvstk_mdio_service_write(&service, 0, 4, 0x1234, BVSTK_EVENT_SOURCE_DCP, 5) == BVSTK_OK);
    assert(host_read(1, 4) == 0x1234);
    assert(bvstk_mdio_service_stats(&service, &stats) == BVSTK_OK);
    assert(stats.observed == 5U && stats.denied == 4U && stats.unknown_phy == 1U && stats.errors == 1U);
    c.whitelist_len = 999U;
    assert(bvstk_mdio_service_set_config(&service, 0, &c) != BVSTK_OK);
    assert(service.devices[0].whitelist_len == 2U);
    /* Missing PHY invalidates old replies; identity polling recovers it. */
    now += 20000U;
    phy[1][2] = phy[1][3] = 0xFFFFU;
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_ERR_NOT_READY);
    assert(!service.online[0] && host_read(1, 4) == 0xFFFFU);
    assert(bvstk_mdio_service_read_cached(&service, 0, 4, &v) == BVSTK_ERR_NOT_READY);
    count = service.stats.reads;
    assert(bvstk_mdio_service_step(&service, now + 100U, 5) == BVSTK_OK);
    assert(service.stats.reads == count);
    now += 1000U;
    phy[1][2] = 7U; phy[1][3] = 0xC0F1U;
    assert(bvstk_mdio_service_step(&service, now, 5) == BVSTK_OK);
    assert(service.online[0] && host_read(1, 2) == 7U);
    assert(!depth && locks > 0);
    bvstk_mdio_service_shutdown(&service);
    {
        mdio_phy_config_t duplicate[2] = {config, config};
        assert(bvstk_mdio_service_init(&service, &core, NULL, duplicate, 2, NULL) != BVSTK_OK);
    }
    bvstk_mdio_core_shutdown(&core);
}
int main(void)
{
    codec_tests(); completion_tests(); driver_tests(); service_tests();
    puts("MDIO driver/BRAM/worker/policy/cache/config/completion tests passed");
    return 0;
}
