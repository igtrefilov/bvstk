#ifndef BVSTK_DCP2_NOTIFY_H
#define BVSTK_DCP2_NOTIFY_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

enum {
    DCP2_NOTIFY_FLAG_WITH_TIMESTAMP = 0x01u,
    DCP2_NOTIFY_FLAG_SNAPSHOT_ON_SUBSCRIBE = 0x02u,
};

enum {
    DCP2_NOTIFY_CLASS_REG_ATTEMPT  = (1u << 0),
    DCP2_NOTIFY_CLASS_REG_COMMIT   = (1u << 1),
    DCP2_NOTIFY_CLASS_REG_DENIED   = (1u << 2),
    DCP2_NOTIFY_CLASS_STATE_CHANGED = (1u << 3),
    DCP2_NOTIFY_CLASS_FAULT        = (1u << 4),
};

typedef enum {
    DCP2_NOTIFY_SOURCE_TELNET = 0,
    DCP2_NOTIFY_SOURCE_HOST = 1,
    DCP2_NOTIFY_SOURCE_DCP = 2,
    DCP2_NOTIFY_SOURCE_INTERNAL = 3,
} dcp2_notify_source_t;

typedef enum {
    DCP2_NOTIFY_BUS_I2C = 0,
    DCP2_NOTIFY_BUS_SMI = 1,
    DCP2_NOTIFY_BUS_SPI = 2,
    DCP2_NOTIFY_BUS_UART = 3,
    DCP2_NOTIFY_BUS_SYS = 4,
} dcp2_notify_bus_t;

typedef enum {
    DCP2_NOTIFY_OP_READ = 0,
    DCP2_NOTIFY_OP_WRITE = 1,
    DCP2_NOTIFY_OP_POLICY_CHANGE = 2,
    DCP2_NOTIFY_OP_CONFIG_APPLY = 3,
    DCP2_NOTIFY_OP_STATE_TOGGLE = 4,
} dcp2_notify_op_t;

typedef enum {
    DCP2_NOTIFY_EV_REG_ATTEMPT = 0x0001,
    DCP2_NOTIFY_EV_REG_COMMIT = 0x0002,
    DCP2_NOTIFY_EV_REG_DENIED = 0x0003,
    DCP2_NOTIFY_EV_STATE_CHANGED = 0x0004,
    DCP2_NOTIFY_EV_FAULT = 0x0005,
} dcp2_notify_ev_type_t;

typedef struct {
    uint64_t time_us;
    uint16_t ev_type;
    uint16_t status;
    uint8_t source;
    uint8_t bus;
    uint8_t op_kind;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
} dcp2_notify_event_t;

typedef struct {
    uint32_t class_mask;
    uint32_t source_mask;
    uint32_t bus_mask;
    uint8_t flags;
} dcp2_notify_filter_t;

bool dcp2_notify_init(void);
bool dcp2_notify_receive(dcp2_notify_event_t *out, TickType_t timeout_ticks);
void dcp2_notify_publish(const dcp2_notify_event_t *event);
void dcp2_notify_publish_simple(uint16_t ev_type,
                                uint16_t status,
                                dcp2_notify_source_t source,
                                dcp2_notify_bus_t bus,
                                dcp2_notify_op_t op_kind,
                                uint32_t arg0,
                                uint32_t arg1,
                                uint32_t arg2);
uint32_t dcp2_notify_event_class_mask(uint16_t ev_type);

#endif /* BVSTK_DCP2_NOTIFY_H */
