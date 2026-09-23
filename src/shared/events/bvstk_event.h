#ifndef BVSTK_SHARED_EVENT_H
#define BVSTK_SHARED_EVENT_H

#include <stdint.h>

typedef enum {
    BVSTK_EVENT_SOURCE_CONSOLE = 0,
    BVSTK_EVENT_SOURCE_HOST = 1,
    BVSTK_EVENT_SOURCE_DCP = 2,
    BVSTK_EVENT_SOURCE_INTERNAL = 3,
    BVSTK_EVENT_SOURCE_WEB = 4
} bvstk_event_source_t;

typedef enum {
    BVSTK_EVENT_BUS_I2C = 0,
    BVSTK_EVENT_BUS_MDIO = 1,
    BVSTK_EVENT_BUS_SPI = 2,
    BVSTK_EVENT_BUS_UART = 3,
    BVSTK_EVENT_BUS_SYSTEM = 4
} bvstk_event_bus_t;

typedef enum {
    BVSTK_EVENT_OP_READ = 0,
    BVSTK_EVENT_OP_WRITE = 1,
    BVSTK_EVENT_OP_POLICY_CHANGE = 2,
    BVSTK_EVENT_OP_CONFIG_APPLY = 3,
    BVSTK_EVENT_OP_STATE_CHANGE = 4
} bvstk_event_op_t;

typedef enum {
    BVSTK_EVENT_REG_ATTEMPT = 0x0001,
    BVSTK_EVENT_REG_COMMIT = 0x0002,
    BVSTK_EVENT_REG_DENIED = 0x0003,
    BVSTK_EVENT_STATE_CHANGED = 0x0004,
    BVSTK_EVENT_FAULT = 0x0005,
    /* Software observations, NOT a lossless logic-analyser trace. */
    BVSTK_EVENT_CAPTURE_WRITE = 0x0101,
    BVSTK_EVENT_MASTER_READ = 0x0102,
    BVSTK_EVENT_MASTER_WRITE = 0x0103,
    BVSTK_EVENT_SLAVE_POINTER = 0x0104,
    BVSTK_EVENT_SLAVE_READ_REQUEST = 0x0105,
    BVSTK_EVENT_SLAVE_REPLY_PREPARED = 0x0106
} bvstk_event_type_t;

/* Flags carried with software observations. They do not change the public
 * event type or the DCP2 wire record. The FreeRTOS observer uses the first
 * flag to keep unconfirmed/probe traffic out of per-device telemetry while
 * retaining the event for local service diagnostics and tests. */
enum {
    BVSTK_EVENT_FLAG_NONE = 0x00,
    BVSTK_EVENT_FLAG_TELEMETRY_UNCONFIRMED = 0x01
};

typedef struct {
    uint64_t time_us;
    uint16_t type;
    uint16_t status;
    uint8_t source;
    uint8_t bus;
    uint8_t operation;
    uint8_t flags;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
} bvstk_event_t;

typedef struct {
    void *context;
    void (*publish)(void *context, const bvstk_event_t *event);
} bvstk_event_sink_t;

#endif
