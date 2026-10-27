/* The same wire-level scenarios run with a recording backend and the actual
 * FreeRTOS/Neutrino memory ports, using only host replacements for OS/BSP I/O. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "shared/interfaces/bvstk_platform.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"

#define BASE BVSTK_I2C_BRAM_BASE
#define SIZE BVSTK_I2C_BRAM_SIZE
static _Alignas(8) uint8_t ram[SIZE], control_ram[BVSTK_I2C_MASTER_SIZE];
static unsigned reads, writes, maps;
static uint32_t addresses[32];
static int mapping_denied;

#ifndef TEST_MEMORY_NEUTRINO
static uint64_t read_value(uint32_t address, unsigned width)
{
    uint64_t value = 0U;
    assert(address >= BASE && (uint64_t)address + width / 8U <= (uint64_t)BASE + SIZE);
    memcpy(&value, ram + address - BASE, width / 8U);
    return value;
}
static void write_value(uint32_t address, unsigned width, uint64_t value)
{
    assert(address >= BASE && (uint64_t)address + width / 8U <= (uint64_t)BASE + SIZE);
    memcpy(ram + address - BASE, &value, width / 8U);
}
#endif

#ifdef TEST_MEMORY_FREERTOS
#include "task.h"
#include "xil_io.h"
TickType_t xTaskGetTickCount(void) { return 1000U; }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
#define READ(bits, type) type Xil_In##bits(UINTPTR a) { \
    addresses[reads++ % 32U] = (uint32_t)a; return (type)read_value((uint32_t)a, bits); }
#define WRITE(bits, type) void Xil_Out##bits(UINTPTR a, type v) { \
    addresses[writes++ % 32U] = (uint32_t)a; write_value((uint32_t)a, bits, v); }
READ(8, uint8_t) READ(16, uint16_t) READ(32, uint32_t)
WRITE(8, uint8_t) WRITE(16, uint16_t) WRITE(32, uint32_t)
#elif defined(TEST_MEMORY_NEUTRINO)
int ThreadCtl(int command, void *argument) { (void)command; (void)argument; return 0; }
void *mmap_device_memory(void *p, size_t size, int protection, int flags, uint64_t physical)
{
    (void)p; (void)protection; (void)flags;
    ++maps;
    if (mapping_denied) { errno = EACCES; return MAP_FAILED; }
    if (physical == BASE && size == SIZE) return ram;
    assert(physical == BVSTK_I2C_MASTER_BASE && size == sizeof(control_ram));
    return control_ram;
}
int munmap(void *p, size_t size) { (void)p; (void)size; return 0; }
#else
static bvstk_status_t check(void *context, uint32_t address, size_t span,
                            unsigned width, int writing)
{
    (void)context; (void)width; (void)writing;
    return address >= BASE && (uint64_t)address + span <= (uint64_t)BASE + SIZE ?
        BVSTK_OK : BVSTK_ERR_RANGE;
}
static bvstk_status_t read_memory(void *context, uint32_t address, unsigned width, uint64_t *value)
{
    (void)context;
    addresses[reads++ % 32U] = address;
    *value = width == 64U ? (read_value(address, 32U) << 32U) | read_value(address + 4U, 32U) :
                           read_value(address, width);
    return BVSTK_OK;
}
static bvstk_status_t write_memory(void *context, uint32_t address, unsigned width, uint64_t value)
{
    (void)context;
    addresses[writes++ % 32U] = address;
    if (width == 64U) {
        write_value(address, 32U, value >> 32U); write_value(address + 4U, 32U, value);
    } else write_value(address, width, value);
    return BVSTK_OK;
}
const bvstk_memory_t *bvstk_platform_memory(void)
{
    static const bvstk_memory_t memory = {
        NULL, BVSTK_MEMORY_ALL_WIDTHS, check, read_memory, write_memory
    };
    return &memory;
}
void bvstk_platform_shutdown(void) {}
#endif

static uint8_t request_body[64], response[64];
static bvstk_dcp2_request_t request = {BVSTK_DCP2_SERVICE_MEM, 0, 1, request_body, 8};
static bvstk_control_api_t api;
static uint16_t response_size;

static void prepare(unsigned opcode, unsigned width, unsigned flags, uint32_t address, unsigned count)
{
    memset(request_body, 0, sizeof(request_body));
    request.opcode = (uint8_t)opcode;
    request.body_size = 8U;
    request_body[0] = (uint8_t)flags; request_body[1] = (uint8_t)width;
    bvstk_dcp2_write_be32(request_body + 2U, address);
    bvstk_dcp2_write_be16(request_body + 6U, (uint16_t)count);
    reads = writes = maps = 0;
    response_size = 0xbeefU;
}
static bvstk_status_t run(size_t capacity)
{
    return bvstk_dcp2_process_mem(&api, &request, response, capacity, &response_size);
}
static void rejected(bvstk_status_t expected)
{
    assert(run(sizeof(response)) == expected);
    assert(response_size == 0U && reads == 0U && writes == 0U && maps == 0U);
}
static void check_width(unsigned width, unsigned flags)
{
    unsigned bytes = width / 8U;
    prepare(BVSTK_DCP2_OP_MEM_WRITE, width, flags, BASE + 16U, 2U);
    request.body_size = (uint16_t)(8U + 2U * bytes);
    for (unsigned i = 0; i < 2U * bytes; ++i) request_body[8U + i] = (uint8_t)(0x20U + i);
    assert(run(sizeof(response)) == BVSTK_OK && response_size == 0U);
    if (width == 64U) {
        uint32_t high, low;
        memcpy(&high, ram + 16U, sizeof(high));
        memcpy(&low, ram + 20U, sizeof(low));
        assert(high == (flags ? UINT32_C(0x20212223) : UINT32_C(0x28292a2b)));
        assert(low == (flags ? UINT32_C(0x24252627) : UINT32_C(0x2c2d2e2f)));
    }
#ifndef TEST_MEMORY_NEUTRINO
    unsigned stride = width == 64U ? 2U : 1U;
#ifndef TEST_MEMORY_FREERTOS
    stride = 1U;
#endif
    assert(addresses[0] == BASE + 16U);
    assert(addresses[stride] == BASE + 16U + (flags ? bytes : 0U));
#endif
    uint8_t expected[16];
    memcpy(expected, request_body + 8U + (flags ? 0U : bytes), bytes);
    memcpy(expected + bytes, request_body + 8U + bytes, bytes);
    prepare(BVSTK_DCP2_OP_MEM_READ, width, flags, BASE + 16U, 2U);
    assert(run(sizeof(response)) == BVSTK_OK && response_size == 2U * bytes);
    assert(!memcmp(response, expected, 2U * bytes));
#ifndef TEST_MEMORY_NEUTRINO
    assert(addresses[0] == BASE + 16U);
    assert(addresses[stride] == BASE + 16U + (flags ? bytes : 0U));
#endif
}
int main(void)
{
    (void)addresses;
    api.memory = bvstk_platform_memory();
    for (unsigned width = 8U; width <= 64U; width *= 2U)
        for (unsigned flags = 0U; flags <= 1U; ++flags) check_width(width, flags);
    bvstk_platform_shutdown();
    prepare(BVSTK_DCP2_OP_MEM_READ, 24U, 0U, BASE, 2U); rejected(BVSTK_ERR_UNSUPPORTED);
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 2U, BASE, 2U); rejected(BVSTK_ERR_MALFORMED);
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 0U, BASE + 1U, 2U); rejected(BVSTK_ERR_RANGE);
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 0U, BASE, 0U); rejected(BVSTK_ERR_RANGE);
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 1U, UINT32_C(0xfffffffc), 2U); rejected(BVSTK_ERR_RANGE);
    prepare(BVSTK_DCP2_OP_MEM_WRITE, 32U, 0U, BASE, 2U); rejected(BVSTK_ERR_MALFORMED);
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 0U, BASE, 2U);
    assert(run(4U) == BVSTK_ERR_RANGE && !reads && !maps && !response_size);
#ifndef TEST_MEMORY_FREERTOS
    prepare(BVSTK_DCP2_OP_MEM_WRITE, 32U, 1U, BASE + SIZE - 4U, 2U);
    request.body_size = 16U;
    rejected(BVSTK_ERR_RANGE); /* The valid first element must not be written. */
#endif
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 0U, BASE + SIZE - 4U, 2U);
    assert(run(sizeof(response)) == BVSTK_OK && response_size == 8U);
    assert(!memcmp(response, response + 4U, 4U));
#ifdef TEST_MEMORY_NEUTRINO
    bvstk_platform_shutdown();
    prepare(BVSTK_DCP2_OP_MEM_READ, 8U, 0U, BVSTK_I2C_MASTER_BASE, 1U);
    rejected(BVSTK_ERR_UNSUPPORTED);
    mapping_denied = 1;
    prepare(BVSTK_DCP2_OP_MEM_READ, 32U, 0U, BASE, 1U);
    assert(run(sizeof(response)) == BVSTK_ERR_DENIED && !response_size && maps == 1U);
    mapping_denied = 0;
#else
    (void)control_ram; (void)mapping_denied;
#endif
    bvstk_memory_t narrow = *api.memory;
    narrow.widths = BVSTK_MEMORY_32;
    api.memory = &narrow;
    prepare(BVSTK_DCP2_OP_MEM_READ, 8U, 0U, BASE, 1U); rejected(BVSTK_ERR_UNSUPPORTED);
    puts("DCP2 MEM: all widths, AUTOINC, repeat access, preflight bounds and port errors passed");
    return 0;
}
