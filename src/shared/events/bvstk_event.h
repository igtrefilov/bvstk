#ifndef BVSTK_SHARED_EVENT_H
#define BVSTK_SHARED_EVENT_H

#include <stdint.h>

typedef enum {
    BVSTK_EVENT_SOURCE_CONSOLE = 0,
    BVSTK_EVENT_SOURCE_HOST = 1,
    BVSTK_EVENT_SOURCE_DCP = 2,
    BVSTK_EVENT_SOURCE_INTERNAL = 3
} bvstk_event_source_t;

typedef enum {
    BVSTK_EVENT_BUS_I2C = 0,
    BVSTK_EVENT_BUS_SMI = 1,
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
    BVSTK_EVENT_FAULT = 0x0005
} bvstk_event_type_t;

typedef struct {
    uint64_t time_us;
    uint16_t type;
    uint16_t status;
    uint8_t source;
    uint8_t bus;
    uint8_t operation;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
} bvstk_event_t;

typedef struct {
    void *context;
    void (*publish)(void *context, const bvstk_event_t *event);
} bvstk_event_sink_t;

#endif
