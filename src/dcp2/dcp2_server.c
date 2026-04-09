#include "dcp2_server.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "xil_io.h"
#include "xil_printf.h"

#include "../bvstk_i2c/bvstk_i2c.h"
#include "../bvstk_smi/bvstk_smi.h"
#include "../bvstk_spi/bvstk_spi.h"
#include "../config/config_store.h"

enum {
    DCP2_PORT_DEFAULT = 8889,
    DCP2_THREAD_STACK = 8192,
    DCP2_VER = 0x0001,
    DCP2_HDR_LEN = 8,
    DCP2_MAX_PAYLOAD = 4096,
    DCP2_MIN_PAYLOAD = 4,
};

enum {
    DCP2_SRV_PING = 0x00,
    DCP2_SRV_MEM = 0x01,
    DCP2_SRV_I2C = 0x02,
    DCP2_SRV_SMI = 0x03,
    DCP2_SRV_SPI = 0x04,
    DCP2_SRV_UART = 0x05,
    DCP2_SRV_VENDOR = 0x7F,
};

enum {
    DCP2_OP_PING = 0x00,
    DCP2_OP_MEM_READ = 0x00,
    DCP2_OP_MEM_WRITE = 0x01,
    DCP2_OP_I2C_READ_REG = 0x00,
    DCP2_OP_I2C_WRITE_REG = 0x01,
    DCP2_OP_I2C_POLICY_SET = 0x02,
    DCP2_OP_SMI_READ = 0x00,
    DCP2_OP_SMI_WRITE = 0x01,
    DCP2_OP_PL_SUBSCRIBE_STREAM = 0x10,
    DCP2_OP_PL_UNSUBSCRIBE_STREAM = 0x11,
};

enum {
    DCP2_STATUS_OK = 0x0000,
    DCP2_STATUS_ERR_MALFORMED = 0x0001,
    DCP2_STATUS_ERR_UNSUPPORTED = 0x0002,
    DCP2_STATUS_ERR_DENIED = 0x0003,
    DCP2_STATUS_ERR_BUSY = 0x0004,
    DCP2_STATUS_ERR_TIMEOUT = 0x0005,
    DCP2_STATUS_ERR_RANGE = 0x0006,
    DCP2_STATUS_ERR_INTERNAL = 0x0007,
};

enum {
    DCP2_OP_RESP_BIT = 0x80,
    DCP2_OP_EVENT_BIT = 0x40,
    DCP2_OP_CODE_MASK = 0x3F,
};

enum {
    DCP2_MEM_FLAG_AUTOINC = 0x01,
    DCP2_SUB_RAW_WORDS = 0x01,
    DCP2_SUB_WITH_TIMESTAMP = 0x02,
    DCP2_SUB_RESET_LOST_COUNTERS = 0x04,
};

typedef struct {
    bool stream_enabled[4];
    uint8_t stream_flags[4];
} dcp2_conn_state_t;

typedef struct {
    uint32_t base;
    uint32_t size;
} mmio_range_t;

static uint16_t s_port = DCP2_PORT_DEFAULT;
static uint8_t s_rx_buf[DCP2_MAX_PAYLOAD];
static uint8_t s_tx_buf[DCP2_HDR_LEN + DCP2_MAX_PAYLOAD];
static uint8_t s_mem_read_buf[DCP2_MAX_PAYLOAD];

static uint16_t be16_read(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t be32_read(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static uint64_t be64_read(const uint8_t *p)
{
    return ((uint64_t)be32_read(p) << 32) | (uint64_t)be32_read(p + 4);
}

static void be16_write(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static void be32_write(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static void be64_write(uint8_t *p, uint64_t v)
{
    be32_write(p, (uint32_t)(v >> 32));
    be32_write(p + 4, (uint32_t)(v & 0xFFFFFFFFu));
}

static int sock_read_exact(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    while (got < len) {
        int r = lwip_read(fd, p + got, (int)(len - got));
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

static int sock_write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;
    while (sent < len) {
        int w = lwip_write(fd, p + sent, (int)(len - sent));
        if (w <= 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}

static int dcp2_send_response(int fd,
                              uint8_t srv,
                              uint8_t opcode,
                              uint16_t seq,
                              uint16_t status,
                              const uint8_t *body,
                              uint16_t body_len)
{
    uint16_t payload_len = (uint16_t)(1u + 1u + 2u + 2u + body_len);
    size_t frame_len = (size_t)DCP2_HDR_LEN + (size_t)payload_len;

    if ((size_t)payload_len > DCP2_MAX_PAYLOAD) return -1;

    s_tx_buf[0] = 'D';
    s_tx_buf[1] = 'C';
    s_tx_buf[2] = 'P';
    s_tx_buf[3] = '2';
    be16_write(s_tx_buf + 4, DCP2_VER);
    be16_write(s_tx_buf + 6, payload_len);
    s_tx_buf[8] = srv;
    s_tx_buf[9] = (uint8_t)(DCP2_OP_RESP_BIT | (opcode & DCP2_OP_CODE_MASK));
    be16_write(s_tx_buf + 10, seq);
    be16_write(s_tx_buf + 12, status);
    if (body_len && body) {
        memcpy(s_tx_buf + 14, body, body_len);
    }
    return sock_write_all(fd, s_tx_buf, frame_len);
}

static bool dcp2_rule_contains_i2c(const i2c_rule_entry_t *rules, size_t len, uint8_t reg, uint8_t val)
{
    size_t i;
    if (!rules) return false;
    for (i = 0; i < len; ++i) {
        if (rules[i].reg == reg && rules[i].val == val) return true;
    }
    return false;
}

static bool dcp2_i2c_value_allowed(const i2c_device_config_t *cfg, uint8_t reg, uint8_t val)
{
    if (!cfg) return false;
    if (cfg->policy == I2C_POLICY_WHITELIST) {
        return dcp2_rule_contains_i2c(cfg->whitelist, cfg->whitelist_len, reg, val);
    }
    return !dcp2_rule_contains_i2c(cfg->blacklist, cfg->blacklist_len, reg, val);
}

static bool dcp2_smi_reg_in_list(const uint8_t *regs, size_t len, uint8_t reg)
{
    size_t i;
    if (!regs) return false;
    for (i = 0; i < len; ++i) {
        if (regs[i] == reg) return true;
    }
    return false;
}

static bool dcp2_smi_write_allowed(const smi_phy_config_t *cfg, uint8_t reg)
{
    if (!cfg) return false;
    if (cfg->policy == SMI_POLICY_WHITELIST) {
        return dcp2_smi_reg_in_list(cfg->write_allow_regs, cfg->write_allow_regs_len, reg);
    }
    return !dcp2_smi_reg_in_list(cfg->write_deny_regs, cfg->write_deny_regs_len, reg);
}

static int dcp2_stream_index(uint8_t srv)
{
    if (srv < DCP2_SRV_I2C || srv > DCP2_SRV_UART) return -1;
    return (int)(srv - DCP2_SRV_I2C);
}

static bool dcp2_mem_width_valid(uint8_t width_bits)
{
    return width_bits == 8u || width_bits == 16u || width_bits == 32u || width_bits == 64u;
}

static bool dcp2_mmio_allowed(uint32_t addr, uint32_t size_bytes)
{
    static const mmio_range_t ranges[] = {
        { (uint32_t)I2C_MASTER_BASE, 0x1000u },
        { (uint32_t)I2C_SLAVE_BASE,  0x1000u },
        { (uint32_t)BRAM_BASE_ADDR,  0x3000u },
        { (uint32_t)MASTER_BASEADDR, 0x1000u },
        { (uint32_t)SLAVE_BASEADDR,  0x1000u },
        { (uint32_t)BRAM_BASEADDR,   (uint32_t)(BRAM_HIGHADDR - BRAM_BASEADDR + 1u) },
        { (uint32_t)SPI_BASEADDR,    0x1000u },
        { (uint32_t)SPI_BRAM_BASEADDR, 0x2000u },
    };
    uint64_t start = (uint64_t)addr;
    uint64_t end = start + (uint64_t)size_bytes;
    size_t i;

    if (size_bytes == 0u || end < start) return false;

    for (i = 0; i < (sizeof(ranges) / sizeof(ranges[0])); ++i) {
        uint64_t r_start = (uint64_t)ranges[i].base;
        uint64_t r_end = r_start + (uint64_t)ranges[i].size;
        if (start >= r_start && end <= r_end) return true;
    }
    return false;
}

static int dcp2_handle_ping(int fd, uint8_t srv, uint8_t opcode, uint16_t seq, const uint8_t *body, uint16_t body_len)
{
    (void)body;
    if (opcode != DCP2_OP_PING) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    }
    if (body_len != 0u) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    }
    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
}

static int dcp2_handle_mem_read(int fd, uint8_t srv, uint8_t opcode, uint16_t seq, const uint8_t *body, uint16_t body_len)
{
    uint8_t flags;
    uint8_t width_bits;
    uint32_t addr;
    uint16_t count;
    uint32_t width_bytes;
    uint32_t total_bytes;
    uint32_t i;

    if (body_len != 8u) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    }

    flags = body[0];
    width_bits = body[1];
    addr = be32_read(body + 2);
    count = be16_read(body + 6);

    if ((flags & (uint8_t)~DCP2_MEM_FLAG_AUTOINC) != 0u) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    }
    if (!dcp2_mem_width_valid(width_bits)) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    }

    width_bytes = (uint32_t)width_bits / 8u;
    total_bytes = (uint32_t)count * width_bytes;
    if (total_bytes > (DCP2_MAX_PAYLOAD - 6u)) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
    }

    for (i = 0; i < count; ++i) {
        uint32_t cur_addr = addr + (((flags & DCP2_MEM_FLAG_AUTOINC) != 0u) ? (i * width_bytes) : 0u);
        uint8_t *dst = s_mem_read_buf + (i * width_bytes);
        if ((cur_addr & (width_bytes - 1u)) != 0u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (!dcp2_mmio_allowed(cur_addr, width_bytes)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }

        if (width_bits == 8u) {
            dst[0] = Xil_In8((UINTPTR)cur_addr);
        } else if (width_bits == 16u) {
            be16_write(dst, Xil_In16((UINTPTR)cur_addr));
        } else if (width_bits == 32u) {
            be32_write(dst, Xil_In32((UINTPTR)cur_addr));
        } else {
            uint64_t hi = (uint64_t)Xil_In32((UINTPTR)cur_addr);
            uint64_t lo = (uint64_t)Xil_In32((UINTPTR)(cur_addr + 4u));
            be64_write(dst, (hi << 32) | lo);
        }
    }

    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, s_mem_read_buf, (uint16_t)total_bytes);
}

static int dcp2_handle_mem_write(int fd, uint8_t srv, uint8_t opcode, uint16_t seq, const uint8_t *body, uint16_t body_len)
{
    uint8_t flags;
    uint8_t width_bits;
    uint32_t addr;
    uint16_t count;
    uint32_t width_bytes;
    uint32_t data_len;
    uint32_t i;

    if (body_len < 8u) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    }

    flags = body[0];
    width_bits = body[1];
    addr = be32_read(body + 2);
    count = be16_read(body + 6);

    if ((flags & (uint8_t)~DCP2_MEM_FLAG_AUTOINC) != 0u) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    }
    if (!dcp2_mem_width_valid(width_bits)) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    }

    width_bytes = (uint32_t)width_bits / 8u;
    data_len = (uint32_t)count * width_bytes;
    if (body_len != (uint16_t)(8u + data_len)) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
    }

    for (i = 0; i < count; ++i) {
        uint32_t cur_addr = addr + (((flags & DCP2_MEM_FLAG_AUTOINC) != 0u) ? (i * width_bytes) : 0u);
        const uint8_t *src = body + 8u + (i * width_bytes);
        if ((cur_addr & (width_bytes - 1u)) != 0u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (!dcp2_mmio_allowed(cur_addr, width_bytes)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }

        if (width_bits == 8u) {
            Xil_Out8((UINTPTR)cur_addr, src[0]);
        } else if (width_bits == 16u) {
            Xil_Out16((UINTPTR)cur_addr, be16_read(src));
        } else if (width_bits == 32u) {
            Xil_Out32((UINTPTR)cur_addr, be32_read(src));
        } else {
            uint64_t v = be64_read(src);
            Xil_Out32((UINTPTR)cur_addr, (uint32_t)(v >> 32));
            Xil_Out32((UINTPTR)(cur_addr + 4u), (uint32_t)(v & 0xFFFFFFFFu));
        }
    }

    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
}

static int dcp2_handle_mem(int fd, uint8_t srv, uint8_t opcode, uint16_t seq, const uint8_t *body, uint16_t body_len)
{
    if (opcode == DCP2_OP_MEM_READ) {
        return dcp2_handle_mem_read(fd, srv, opcode, seq, body, body_len);
    }
    if (opcode == DCP2_OP_MEM_WRITE) {
        return dcp2_handle_mem_write(fd, srv, opcode, seq, body, body_len);
    }
    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
}

static int dcp2_handle_i2c(int fd, uint8_t srv, uint8_t opcode, uint16_t seq, const uint8_t *body, uint16_t body_len)
{
    size_t dev_idx = 0;
    const i2c_device_config_t *cfg = NULL;

    if (!config_store_is_ready()) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_BUSY, NULL, 0);
    }

    if (opcode == DCP2_OP_I2C_READ_REG) {
        uint8_t val = 0;
        if (body_len != 2u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        if (!i2cdev_find_device_index_by_addr(body[0], &dev_idx)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        cfg = config_store_find_i2c_device_by_addr(body[0]);
        if (!cfg || body[1] >= cfg->reg_count) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (!i2cdev_read_reg_dev(dev_idx, body[1], &val)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_TIMEOUT, NULL, 0);
        }
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, &val, 1);
    }

    if (opcode == DCP2_OP_I2C_WRITE_REG) {
        if (body_len != 3u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        if (!i2cdev_find_device_index_by_addr(body[0], &dev_idx)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        cfg = config_store_find_i2c_device_by_addr(body[0]);
        if (!cfg || body[1] >= cfg->reg_count || body[2] > cfg->max_value_code) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (!dcp2_i2c_value_allowed(cfg, body[1], body[2])) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_DENIED, NULL, 0);
        }
        if (!i2cdev_write_reg_dev(dev_idx, body[1], body[2])) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_INTERNAL, NULL, 0);
        }
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    if (opcode == DCP2_OP_I2C_POLICY_SET) {
        i2cdev_policy_t policy;
        if (body_len != 2u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        if (!i2cdev_find_device_index_by_addr(body[0], &dev_idx)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        cfg = config_store_find_i2c_device_by_addr(body[0]);
        if (!cfg) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (body[1] == 0x00u) {
            policy = I2CDEV_POLICY_WHITELIST;
        } else if (body[1] == 0x01u) {
            policy = I2CDEV_POLICY_BLACKLIST;
        } else {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
        }
        if (!i2cdev_set_policy_dev(dev_idx, policy)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_INTERNAL, NULL, 0);
        }
        (void)config_store_save_i2c_device(cfg);
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
}

static int dcp2_handle_smi(int fd, uint8_t srv, uint8_t opcode, uint16_t seq, const uint8_t *body, uint16_t body_len)
{
    smi_phy_config_t *cfg = NULL;

    if (!config_store_is_ready()) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_BUSY, NULL, 0);
    }

    if (opcode == DCP2_OP_SMI_READ) {
        uint16_t val = 0;
        uint8_t out[2];
        if (body_len != 2u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        cfg = (smi_phy_config_t *)config_store_find_smi_device_by_phy(body[0] & 0x1Fu);
        if (!cfg || body[1] >= cfg->reg_count) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (!mdio_read_blocking(body[0], body[1], &val, pdMS_TO_TICKS(100))) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_TIMEOUT, NULL, 0);
        }
        be16_write(out, val);
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, out, 2);
    }

    if (opcode == DCP2_OP_SMI_WRITE) {
        uint16_t val16;
        if (body_len != 4u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        cfg = (smi_phy_config_t *)config_store_find_smi_device_by_phy(body[0] & 0x1Fu);
        if (!cfg || body[1] >= cfg->reg_count) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_RANGE, NULL, 0);
        }
        if (!dcp2_smi_write_allowed(cfg, body[1])) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_DENIED, NULL, 0);
        }
        val16 = be16_read(body + 2);
        if (!smi_write_checked(body[0], body[1], val16)) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_INTERNAL, NULL, 0);
        }
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
}

static int dcp2_handle_stream_ctl(int fd,
                                  dcp2_conn_state_t *state,
                                  uint8_t srv,
                                  uint8_t opcode,
                                  uint16_t seq,
                                  const uint8_t *body,
                                  uint16_t body_len)
{
    int idx = dcp2_stream_index(srv);
    if (idx < 0) {
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    }

    if (opcode == DCP2_OP_PL_SUBSCRIBE_STREAM) {
        if (body_len != 1u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        if ((body[0] & (uint8_t)~(DCP2_SUB_RAW_WORDS | DCP2_SUB_WITH_TIMESTAMP | DCP2_SUB_RESET_LOST_COUNTERS)) != 0u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        state->stream_enabled[idx] = true;
        state->stream_flags[idx] = body[0];
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    if (opcode == DCP2_OP_PL_UNSUBSCRIBE_STREAM) {
        if (body_len != 0u) {
            return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        }
        state->stream_enabled[idx] = false;
        state->stream_flags[idx] = 0u;
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_OK, NULL, 0);
    }

    return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
}

static int dcp2_dispatch_request(int fd,
                                 dcp2_conn_state_t *state,
                                 uint8_t srv,
                                 uint8_t opcode,
                                 uint16_t seq,
                                 const uint8_t *body,
                                 uint16_t body_len)
{
    switch (srv) {
    case DCP2_SRV_PING:
        return dcp2_handle_ping(fd, srv, opcode, seq, body, body_len);
    case DCP2_SRV_MEM:
        return dcp2_handle_mem(fd, srv, opcode, seq, body, body_len);
    case DCP2_SRV_I2C:
        if (opcode == DCP2_OP_PL_SUBSCRIBE_STREAM || opcode == DCP2_OP_PL_UNSUBSCRIBE_STREAM) {
            return dcp2_handle_stream_ctl(fd, state, srv, opcode, seq, body, body_len);
        }
        return dcp2_handle_i2c(fd, srv, opcode, seq, body, body_len);
    case DCP2_SRV_SMI:
        if (opcode == DCP2_OP_PL_SUBSCRIBE_STREAM || opcode == DCP2_OP_PL_UNSUBSCRIBE_STREAM) {
            return dcp2_handle_stream_ctl(fd, state, srv, opcode, seq, body, body_len);
        }
        return dcp2_handle_smi(fd, srv, opcode, seq, body, body_len);
    case DCP2_SRV_SPI:
    case DCP2_SRV_UART:
        if (opcode == DCP2_OP_PL_SUBSCRIBE_STREAM || opcode == DCP2_OP_PL_UNSUBSCRIBE_STREAM) {
            return dcp2_handle_stream_ctl(fd, state, srv, opcode, seq, body, body_len);
        }
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    case DCP2_SRV_VENDOR:
    default:
        return dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_UNSUPPORTED, NULL, 0);
    }
}

static int dcp2_handle_frame(int fd, dcp2_conn_state_t *state, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t srv;
    uint8_t op;
    uint8_t opcode;
    uint16_t seq;
    const uint8_t *body;
    uint16_t body_len;

    if (payload_len < DCP2_MIN_PAYLOAD) return -1;

    srv = payload[0];
    op = payload[1];
    opcode = (uint8_t)(op & DCP2_OP_CODE_MASK);
    seq = be16_read(payload + 2);
    body = payload + 4;
    body_len = (uint16_t)(payload_len - 4u);

    if ((op & DCP2_OP_EVENT_BIT) != 0u || (op & DCP2_OP_RESP_BIT) != 0u) {
        (void)dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        return 0;
    }
    if (seq == 0u) {
        (void)dcp2_send_response(fd, srv, opcode, seq, DCP2_STATUS_ERR_MALFORMED, NULL, 0);
        return 0;
    }

    return dcp2_dispatch_request(fd, state, srv, opcode, seq, body, body_len);
}

static void dcp2_handle_client(int fd)
{
    dcp2_conn_state_t state;
    memset(&state, 0, sizeof(state));

    for (;;) {
        uint8_t hdr[DCP2_HDR_LEN];
        uint16_t ver;
        uint16_t payload_len;

        if (sock_read_exact(fd, hdr, sizeof(hdr)) < 0) return;
        if (hdr[0] != 'D' || hdr[1] != 'C' || hdr[2] != 'P' || hdr[3] != '2') return;

        ver = be16_read(hdr + 4);
        payload_len = be16_read(hdr + 6);

        if (ver != DCP2_VER) return;
        if (payload_len < DCP2_MIN_PAYLOAD || payload_len > DCP2_MAX_PAYLOAD) return;
        if (sock_read_exact(fd, s_rx_buf, payload_len) < 0) return;
        if (dcp2_handle_frame(fd, &state, s_rx_buf, payload_len) < 0) return;
    }
}

static void dcp2_server_thread(void *arg)
{
    int s;
    struct sockaddr_in addr;

    (void)arg;

    s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        vTaskDelete(NULL);
        return;
    }

    {
        int opt = 1;
        (void)lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (lwip_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lwip_close(s);
        vTaskDelete(NULL);
        return;
    }

    lwip_listen(s, 1);
    xil_printf("DCP2: listening on %u\r\n", (unsigned)s_port);

    for (;;) {
        struct sockaddr_in remote;
        socklen_t rlen = sizeof(remote);
        int c = lwip_accept(s, (struct sockaddr *)&remote, &rlen);
        if (c < 0) continue;
        dcp2_handle_client(c);
        lwip_close(c);
    }
}

uint16_t dcp2_server_port(void)
{
    return s_port;
}

void start_dcp2_server(void)
{
    sys_thread_new("dcp2", dcp2_server_thread, 0, DCP2_THREAD_STACK, tskIDLE_PRIORITY + 1);
}
