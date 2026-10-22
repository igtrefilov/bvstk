#ifndef BVSTK_SHARED_CONFIG_MODEL_H
#define BVSTK_SHARED_CONFIG_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "shared/config/bvstk_acquisition_model.h"

typedef struct {
    uint8_t mac[6];
    uint32_t ip_be;
    uint32_t netmask_be;
    uint32_t gateway_be;
    bool has_ip;
    bool has_netmask;
    bool has_gateway;
    bool has_mac;
} network_config_t;

#define I2C_CFG_MAX_DEVICES 32u
#define I2C_CFG_NAME_MAX 32u
#define I2C_CFG_FILE_NAME_MAX 224u
#define I2C_CFG_MAX_REG_COUNT 256u
#define I2C_CFG_RULES_MAX 256u
#define I2C_CFG_SETTINGS_MAX 256u
#define BVSTK_I2C_ROUTE_MAX 4u
#define BVSTK_I2C_SWITCH_CHANNELS_MAX 16u
#define BVSTK_ADDRESS_SPACES_MAX 8u

/* Named windows of one logical device. first is an internal cache index;
 * addresses in the profile and on the wire remain relative to each window. */
typedef struct {
    char id[I2C_CFG_NAME_MAX];
    uint16_t first, count;
    uint8_t address;
} bvstk_address_space_t;

typedef enum {
    BVSTK_I2C_SEQUENCE_LINEAR = 0,
    BVSTK_I2C_SEQUENCE_FIXED,
    BVSTK_I2C_SEQUENCE_WRAP
} bvstk_i2c_sequence_mode_t;

typedef struct {
    uint16_t span; /* zero means the complete register space */
    uint8_t mode;
} bvstk_i2c_sequence_t;

typedef struct {
    uint16_t address_base;
    uint16_t write_cycle_ms;
    bvstk_i2c_sequence_t read_sequence, write_sequence;
    uint8_t configured;
    uint8_t address_bytes; /* 0: unaddressed data; 1 or 2: register prefix */
    uint8_t little_endian;
    uint8_t stop_before_read;
    uint8_t verify_readback;
    uint8_t invalidate_after_write;
    uint8_t host_read_fresh;
    uint8_t host_current_read;
} bvstk_i2c_transfer_t;

typedef struct {
    uint8_t select;
    uint8_t match_mask;
} bvstk_i2c_channel_t;

typedef struct {
    uint8_t channel_count; /* zero: endpoint without downstream channels */
    uint8_t reg;
    uint8_t select_mask;
    uint8_t preserve_mask;
    bvstk_i2c_channel_t channels[BVSTK_I2C_SWITCH_CHANNELS_MAX];
} bvstk_i2c_switch_t;

/* parent is a profile ID; an empty parent means the controller's root bus. */
typedef struct {
    char parent[I2C_CFG_NAME_MAX];
    uint8_t channel;
    bvstk_i2c_transfer_t transfer;
    bvstk_i2c_switch_t router;
    uint8_t read_only[I2C_CFG_MAX_REG_COUNT / 8u];
    uint8_t space_count;
    bvstk_address_space_t spaces[BVSTK_ADDRESS_SPACES_MAX];
} bvstk_i2c_endpoint_t;

static inline const bvstk_address_space_t *bvstk_i2c_register_space(
    const bvstk_i2c_endpoint_t *e, unsigned reg)
{
    for (unsigned i = 0; i < e->space_count; ++i)
        if (reg >= e->spaces[i].first && reg - e->spaces[i].first < e->spaces[i].count)
            return &e->spaces[i];
    return NULL;
}

static inline int bvstk_i2c_address_space(const bvstk_i2c_endpoint_t *e,
    uint8_t primary, unsigned address)
{
    if (!e->space_count) return address == primary ? 0 : -1;
    for (unsigned i = 0; i < e->space_count; ++i)
        if (e->spaces[i].address == address) return (int)i;
    return -1;
}

static inline int bvstk_i2c_address_register(const bvstk_i2c_endpoint_t *e,
    uint8_t primary, unsigned count, unsigned address, unsigned reg, uint8_t *index)
{
    int space = bvstk_i2c_address_space(e, primary, address);
    if (space < 0 || !index) return 0;
    unsigned first = e->space_count ? e->spaces[space].first : 0;
    unsigned size = e->space_count ? e->spaces[space].count : count;
    if (reg >= size || first + reg >= count) return 0;
    *index = (uint8_t)(first + reg);
    return 1;
}

/* Zero-initialized legacy profiles retain the original byte-register bridge. */
static inline bvstk_i2c_transfer_t bvstk_i2c_transfer_options(const bvstk_i2c_endpoint_t *endpoint)
{
    bvstk_i2c_transfer_t value = endpoint->transfer;
    if (!value.configured) {
        value.address_bytes = 1;
        value.read_sequence.mode = BVSTK_I2C_SEQUENCE_WRAP;
    }
    return value;
}

/* A block can use auto-incrementing wrap mode as long as no byte crosses its
 * wrap boundary. Preserve the external host's pointer semantics. */
static inline int bvstk_i2c_read_block_contiguous(const bvstk_i2c_endpoint_t *e,
    unsigned reg_count, unsigned first, unsigned count)
{
    if (!e || !count || first >= reg_count || count > reg_count - first) return 0;
    const bvstk_address_space_t *space = bvstk_i2c_register_space(e, first);
    if ((e->space_count && !space) ||
        space != bvstk_i2c_register_space(e, first + count - 1U)) return 0;
    if (count == 1) return 1;
    if (!e->transfer.configured) return 0;
    const bvstk_i2c_sequence_t *sequence = &e->transfer.read_sequence;
    if (sequence->mode == BVSTK_I2C_SEQUENCE_LINEAR) return 1;
    if (sequence->mode != BVSTK_I2C_SEQUENCE_WRAP) return 0;
    unsigned span = sequence->span ? sequence->span : space ? space->count : reg_count;
    unsigned local = first - (space ? space->first : 0U);
    return span && count <= span - local % span;
}

/* File-format metadata is kept separate from the established bus-facing model.
 * The bridge adapters retain its policy/cache/polling semantics exactly. */
#define BVSTK_PROFILE_JSON_MAX 65536u
#define BVSTK_PROFILE_IDS_CAP 2048u
#define BVSTK_REGISTER_MAP_NAME_MAX 64u
typedef struct {
    uint8_t version; /* zero: unversioned legacy projection, two: profile v2 */
    char vendor[65];
    char model[65];
    char device_class[16];
    char label[97];
    char register_map[BVSTK_REGISTER_MAP_NAME_MAX];
    char view[BVSTK_REGISTER_MAP_NAME_MAX];
    char icon[BVSTK_REGISTER_MAP_NAME_MAX];
    uint16_t id_offset[I2C_CFG_MAX_REG_COUNT];
    uint16_t ids_size;
    char ids[BVSTK_PROFILE_IDS_CAP];
    bvstk_acquisition_plan_t acquisition;
} bvstk_profile_metadata_t;

typedef enum {
    I2C_POLICY_WHITELIST = 0,
    I2C_POLICY_BLACKLIST = 1
} i2c_policy_t;

typedef struct {
    uint8_t reg;
    uint8_t val;
} i2c_rule_entry_t;

typedef struct {
    char name[I2C_CFG_NAME_MAX];
    char file_name[I2C_CFG_FILE_NAME_MAX];
    uint8_t addr_7b;
    uint16_t reg_count;
    uint8_t max_value_code;
    /* Read-only physical presence check.  The register is a profile resource
     * chosen for safe reads; disabled checks leave presence unknown. */
    uint8_t presence_check_enabled;
    uint8_t presence_check_reg;
    bvstk_i2c_endpoint_t endpoint;
    i2c_policy_t policy;
    i2c_rule_entry_t whitelist[I2C_CFG_RULES_MAX];
    size_t whitelist_len;
    i2c_rule_entry_t blacklist[I2C_CFG_RULES_MAX];
    size_t blacklist_len;
    i2c_rule_entry_t settings[I2C_CFG_SETTINGS_MAX];
    size_t settings_len;
    bvstk_profile_metadata_t profile;
} i2c_device_config_t;

#define MDIO_CFG_MAX_DEVICES 8u
#define MDIO_CFG_NAME_MAX 32u
#define MDIO_CFG_FILE_NAME_MAX I2C_CFG_FILE_NAME_MAX
#define MDIO_CFG_AUTOPOLL_REGS_MAX 32u
#define MDIO_CFG_RULES_MAX 64u
#define MDIO_CFG_SETTINGS_MAX 32u

typedef enum {
    MDIO_POLICY_WHITELIST = 0,
    MDIO_POLICY_BLACKLIST = 1
} mdio_policy_t;

typedef struct {
    uint8_t reg;
    uint16_t val;
} mdio_setting_entry_t;

/* mask=0xffff matches an exact value; mask=0 matches any value of reg. */
typedef struct {
    uint8_t reg;
    uint16_t val;
    uint16_t mask;
} mdio_rule_entry_t;

typedef struct {
    char name[MDIO_CFG_NAME_MAX];
    char file_name[MDIO_CFG_FILE_NAME_MAX];
    uint8_t phy_addr;
    uint8_t reg_count;
    mdio_policy_t policy;
    bool autopoll_enabled;
    uint8_t autopoll_regs[MDIO_CFG_AUTOPOLL_REGS_MAX];
    size_t autopoll_regs_len;
    uint32_t autopoll_reg_delay_ms;
    uint32_t autopoll_cycle_delay_ms;
    mdio_rule_entry_t whitelist[MDIO_CFG_RULES_MAX];
    size_t whitelist_len;
    mdio_rule_entry_t blacklist[MDIO_CFG_RULES_MAX];
    size_t blacklist_len;
    mdio_setting_entry_t settings[MDIO_CFG_SETTINGS_MAX];
    size_t settings_len;
    bvstk_profile_metadata_t profile;
} mdio_phy_config_t;

#endif /* BVSTK_SHARED_CONFIG_MODEL_H */
