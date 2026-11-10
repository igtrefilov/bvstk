#include "ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.h"

#include <string.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/i2c/bvstk_i2c_regs.h"

enum {
    BVSTK_I2C_SLAVE_FREERTOS_QUEUE_LENGTH = 16,
    /* Words, not bytes.  A host read runs the deepest path in the firmware:
     * slave_task -> handle_frame -> load_window -> master read_from ->
     * routed_transfer -> record/cache_response -> event journal, and several
     * of these frames hold a full bvstk_i2c_device_t copy (~0.5 KiB each).
     * Measured with -fstack-usage (Debug, -O0) at about 3 KiB plus interrupt
     * context: the former 768 words (3 KiB) overflowed on the first host
     * read, the stack-overflow hook masked interrupts and the PS stopped
     * answering (PL core NACK after its 50 ms stretch, HTTP/console dead). */
    BVSTK_I2C_SLAVE_FREERTOS_STACK = 2048,
    /* The slave's 100 us stretch budget cannot wait for network/UI tasks
     * at the same priority to finish their scheduler time slice. */
    BVSTK_I2C_SLAVE_FREERTOS_PRIORITY = tskIDLE_PRIORITY + 5
};

/* One interrupt event plus the write frame copied out of BRAM inside the ISR.
 * The PL core has a single write window and reuses it for the next
 * STOP/repeated START.  Linux issues the PCA9545A select, the register
 * pointer and the read back to back, so a frame read later by the task (or
 * erased by it after a slow downstream forward) belonged to the next
 * transfer: the pointer write was lost and the host received the byte after
 * the previous read (SFP A2h 0x70 instead of 0x6E). */
typedef struct {
    bvstk_i2c_slave_irq_event_t irq;
    bvstk_status_t frame_status;
    uint16_t frame_size;
    uint8_t frame_addr;
    uint8_t frame[BVSTK_I2C_SLAVE_MAX_FRAME_BYTES];
} slave_queue_item_t;

/* ISR scratch: too large for the 1 KiB IRQ stack, and this interrupt does
 * not nest with itself. */
static slave_queue_item_t s_isr_item;

static void slave_isr(void *callback_ref)
{
    bvstk_i2c_slave_freertos_t *adapter =
        (bvstk_i2c_slave_freertos_t *)callback_ref;
    BaseType_t higher_priority_task_woken = pdFALSE;
    size_t frame_size = 0U;

    if (adapter == NULL || adapter->event_queue == NULL ||
        bvstk_i2c_slave_hw_capture_irq_frame(adapter->hardware,
                                             &s_isr_item.irq,
                                             s_isr_item.frame,
                                             sizeof(s_isr_item.frame),
                                             &frame_size,
                                             &s_isr_item.frame_addr,
                                             &s_isr_item.frame_status) != BVSTK_OK ||
        s_isr_item.irq.irq_flags == 0U) {
        return;
    }
    s_isr_item.frame_size = (uint16_t)frame_size;
    (void)xQueueSendFromISR(adapter->event_queue,
                            &s_isr_item,
                            &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static int target_address_matches(
    bvstk_i2c_slave_service_t *service,
    uint8_t addr_7b)
{
    return bvstk_i2c_slave_service_select_address(service, addr_7b & 0x7fU) == BVSTK_OK;
}

static bvstk_status_t process_write_event(
    bvstk_i2c_slave_freertos_t *adapter,
    const slave_queue_item_t *item,
    uint8_t *read_window,
    size_t read_window_capacity)
{
    size_t ignored_read_window_size = 0U;
    bvstk_status_t status = item->frame_status;

    if (status == BVSTK_OK &&
        !target_address_matches(adapter->service, item->frame_addr)) {
        status = BVSTK_ERR_NOT_FOUND;
    }
    if (status == BVSTK_OK) {
        /* A write frame may arm a subsequent repeated-start read. */
        status = bvstk_i2c_slave_service_handle_frame(
            adapter->service,
            item->frame_size == 0U ? NULL : item->frame,
            item->frame_size,
            0U,
            read_window,
            read_window_capacity,
            &ignored_read_window_size,
            100U);
    }
    /* The window is not cleared here: by now it may already hold the next
     * frame.  The core overwrites the header with every new frame. */
    return status;
}

static bvstk_status_t process_read_event(
    bvstk_i2c_slave_freertos_t *adapter,
    const bvstk_i2c_slave_irq_event_t *event,
    uint8_t *read_window,
    size_t read_window_capacity)
{
    size_t read_window_size = 0U;
    bvstk_status_t status;

    /* After each offered window the core asks for the next byte at once.
     * If the host has already ended that read (NACK + STOP), a late reply
     * would be taken by the following read request.  Skip stale requests
     * before the downstream I/O and check again before offering data. */
    if (!bvstk_i2c_slave_hw_request_pending(adapter->hardware, event->request_header)) {
        return BVSTK_OK;
    }
    if (!target_address_matches(adapter->service, event->request_addr)) {
        return BVSTK_ERR_NOT_FOUND;
    }
    status = bvstk_i2c_slave_service_handle_frame(
        adapter->service,
        NULL,
        0U,
        1U,
        read_window,
        read_window_capacity,
        &read_window_size,
        100U);
    if (status != BVSTK_OK || read_window_size == 0U) {
        return status;
    }
    if (!bvstk_i2c_slave_hw_request_pending(adapter->hardware, event->request_header)) {
        return BVSTK_OK;
    }
    status = bvstk_i2c_slave_hw_write_read_window(
        adapter->hardware,
        event->request_addr,
        read_window,
        read_window_size);
    if (status == BVSTK_OK) {
        status = bvstk_i2c_slave_hw_accept_read(adapter->hardware);
    }
    return status;
}

static void slave_task(void *argument)
{
    bvstk_i2c_slave_freertos_t *adapter =
        (bvstk_i2c_slave_freertos_t *)argument;
    static slave_queue_item_t item;   /* task-owned; keeps 0.3 KiB off the stack */
    uint8_t read_window[BVSTK_I2C_SLAVE_READ_WINDOW_BYTES];

    for (;;) {
        bvstk_status_t status;

        if (xQueueReceive(adapter->event_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        status = BVSTK_OK;
        if ((item.irq.irq_flags & BVSTK_I2C_SLV_IRQ_DATA_VALID) != 0U) {
            status = process_write_event(adapter,
                                         &item,
                                         read_window,
                                         sizeof(read_window));
        }
        if (status == BVSTK_OK &&
            (item.irq.irq_flags & (BVSTK_I2C_SLV_IRQ_ERR_ACK |
                                   BVSTK_I2C_SLV_IRQ_MEM_OVERFLOW)) == 0U &&
            (item.irq.irq_flags & BVSTK_I2C_SLV_IRQ_RD_REQUEST) != 0U) {
            status = process_read_event(adapter,
                                        &item.irq,
                                        read_window,
                                        sizeof(read_window));
        }
        if (status != BVSTK_OK ||
            bvstk_i2c_slave_irq_ends_read(item.irq.irq_flags)) {
            bvstk_i2c_slave_service_end_transaction(adapter->service);
        }
        (void)status;
    }
}

bvstk_status_t bvstk_i2c_slave_freertos_start(
    bvstk_i2c_slave_freertos_t *adapter,
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_service_t *service)
{
    if (adapter == NULL || hardware == NULL || service == NULL ||
        hardware->initialized == 0U || service->initialized == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->hardware = hardware;
    adapter->service = service;
    adapter->event_queue = xQueueCreate(BVSTK_I2C_SLAVE_FREERTOS_QUEUE_LENGTH,
                                        sizeof(slave_queue_item_t));
    if (adapter->event_queue == NULL) {
        return BVSTK_ERR_INTERNAL;
    }
    if (xTaskCreate(slave_task,
                    "i2c_slave",
                    BVSTK_I2C_SLAVE_FREERTOS_STACK,
                    adapter,
                    BVSTK_I2C_SLAVE_FREERTOS_PRIORITY,
                    &adapter->task) != pdPASS) {
        vQueueDelete(adapter->event_queue);
        memset(adapter, 0, sizeof(*adapter));
        return BVSTK_ERR_INTERNAL;
    }
    xPortInstallInterruptHandler(BVSTK_IRQ_I2C_SLAVE, slave_isr, adapter);
    if (bvstk_i2c_slave_hw_clear_irq(hardware) != BVSTK_OK) {
        return BVSTK_ERR_IO;
    }
    vPortEnableInterrupt(BVSTK_IRQ_I2C_SLAVE);
    adapter->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_slave_freertos_stop(bvstk_i2c_slave_freertos_t *adapter)
{
    /* The runtime owns this adapter for the lifetime of the firmware image. */
    (void)adapter;
}
