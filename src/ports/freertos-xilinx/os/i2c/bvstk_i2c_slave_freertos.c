#include "ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.h"

#include <string.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/i2c/bvstk_i2c_regs.h"

enum {
    BVSTK_I2C_SLAVE_FREERTOS_QUEUE_LENGTH = 16,
    BVSTK_I2C_SLAVE_FREERTOS_STACK = 768,
    BVSTK_I2C_SLAVE_FREERTOS_PRIORITY = tskIDLE_PRIORITY + 1
};

static void slave_isr(void *callback_ref)
{
    bvstk_i2c_slave_freertos_t *adapter =
        (bvstk_i2c_slave_freertos_t *)callback_ref;
    bvstk_i2c_slave_irq_event_t event;
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (adapter == NULL || adapter->event_queue == NULL ||
        bvstk_i2c_slave_hw_capture_irq(adapter->hardware, &event) != BVSTK_OK ||
        event.irq_flags == 0U) {
        return;
    }
    (void)xQueueSendFromISR(adapter->event_queue,
                            &event,
                            &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static int target_address_matches(
    const bvstk_i2c_slave_service_t *service,
    uint8_t addr_7b)
{
    bvstk_i2c_device_t device;

    if (service == NULL || service->master == NULL ||
        bvstk_i2c_master_service_device_info(
            service->master,
            bvstk_i2c_slave_service_target(service),
            &device) != BVSTK_OK) {
        return 0;
    }
    return device.addr_7b == (addr_7b & UINT8_C(0x7F));
}

static bvstk_status_t process_write_event(
    bvstk_i2c_slave_freertos_t *adapter,
    uint8_t *frame,
    size_t frame_capacity,
    uint8_t *read_window,
    size_t read_window_capacity)
{
    size_t frame_size = 0U;
    size_t ignored_read_window_size = 0U;
    uint8_t addr_7b = 0U;
    bvstk_status_t status;

    status = bvstk_i2c_slave_hw_read_frame(adapter->hardware,
                                           frame,
                                           frame_capacity,
                                           &frame_size,
                                           &addr_7b);
    if (status == BVSTK_OK &&
        !target_address_matches(adapter->service, addr_7b)) {
        status = BVSTK_ERR_NOT_FOUND;
    }
    if (status == BVSTK_OK) {
        /* A write frame may arm a subsequent repeated-start read. */
        status = bvstk_i2c_slave_service_handle_frame(
            adapter->service,
            frame_size == 0U ? NULL : frame,
            frame_size,
            0U,
            read_window,
            read_window_capacity,
            &ignored_read_window_size,
            100U);
    }
    (void)bvstk_i2c_slave_hw_clear_frame(adapter->hardware, frame_size);
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
    uint8_t frame[BVSTK_I2C_SLAVE_MAX_FRAME_BYTES];
    uint8_t read_window[BVSTK_I2C_SLAVE_READ_WINDOW_BYTES];

    for (;;) {
        bvstk_i2c_slave_irq_event_t event;
        bvstk_status_t status;

        if (xQueueReceive(adapter->event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        status = BVSTK_OK;
        if ((event.irq_flags & BVSTK_I2C_SLV_IRQ_DATA_VALID) != 0U) {
            status = process_write_event(adapter,
                                         frame,
                                         sizeof(frame),
                                         read_window,
                                         sizeof(read_window));
        }
        if (status == BVSTK_OK &&
            (event.irq_flags & BVSTK_I2C_SLV_IRQ_RD_REQUEST) != 0U) {
            status = process_read_event(adapter,
                                        &event,
                                        read_window,
                                        sizeof(read_window));
        }
        if ((event.irq_flags & (BVSTK_I2C_SLV_IRQ_FINAL_PACKET |
                                BVSTK_I2C_SLV_IRQ_ERR_ACK)) != 0U) {
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
                                        sizeof(bvstk_i2c_slave_irq_event_t));
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
