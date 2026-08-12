#include "ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.h"

#include <string.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"

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
        bvstk_i2c_slave_hw_capture_irq(adapter->hardware, &event) != BVSTK_OK) {
        return;
    }
    (void)xQueueSendFromISR(adapter->event_queue,
                            &event,
                            &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void slave_task(void *argument)
{
    bvstk_i2c_slave_freertos_t *adapter =
        (bvstk_i2c_slave_freertos_t *)argument;
    uint8_t frame[BVSTK_I2C_SLAVE_MAX_FRAME_BYTES];
    uint8_t read_window[BVSTK_I2C_SLAVE_READ_WINDOW_BYTES];

    for (;;) {
        bvstk_i2c_slave_irq_event_t event;
        size_t read_window_size = 0U;
        bvstk_status_t status;

        if (xQueueReceive(adapter->event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        status = bvstk_i2c_slave_hw_read_frame(adapter->hardware,
                                               event.frame_size,
                                               frame,
                                               sizeof(frame));
        if (status == BVSTK_OK) {
            status = bvstk_i2c_slave_service_handle_frame(
                adapter->service,
                event.frame_size == 0U ? NULL : frame,
                event.frame_size,
                event.read_phase,
                read_window,
                sizeof(read_window),
                &read_window_size,
                100U);
        }
        if (status == BVSTK_OK && read_window_size != 0U) {
            (void)bvstk_i2c_slave_hw_write_read_window(adapter->hardware,
                                                       read_window,
                                                       read_window_size);
        }
        (void)bvstk_i2c_slave_hw_clear_frame(adapter->hardware,
                                             event.frame_size);
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
    if (bvstk_i2c_slave_hw_enable_irq(hardware) != BVSTK_OK) {
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
