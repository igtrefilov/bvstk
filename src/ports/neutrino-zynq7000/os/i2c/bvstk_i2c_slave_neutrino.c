#include "ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/neutrino.h>

#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "hardware/pl/i2c/bvstk_i2c_regs.h"

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
    bvstk_i2c_slave_neutrino_t *adapter,
    uint8_t *frame,
    size_t frame_capacity,
    uint8_t *read_window,
    size_t read_window_capacity)
{
    size_t frame_size = 0U;
    size_t ignored_window_size = 0U;
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
        status = bvstk_i2c_slave_service_handle_frame(
            adapter->service,
            frame_size == 0U ? NULL : frame,
            frame_size,
            0U,
            read_window,
            read_window_capacity,
            &ignored_window_size,
            100U);
    }
    (void)bvstk_i2c_slave_hw_clear_frame(adapter->hardware, frame_size);
    return status;
}

static bvstk_status_t process_read_event(
    bvstk_i2c_slave_neutrino_t *adapter,
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
    status = bvstk_i2c_slave_hw_write_read_window(adapter->hardware,
                                                   event->request_addr,
                                                   read_window,
                                                   read_window_size);
    if (status == BVSTK_OK) {
        status = bvstk_i2c_slave_hw_accept_read(adapter->hardware);
    }
    return status;
}

static void report_startup(bvstk_i2c_slave_neutrino_t *adapter,
                           bvstk_status_t status)
{
    (void)pthread_mutex_lock(&adapter->startup_mutex);
    adapter->startup_status = status;
    adapter->startup_complete = 1;
    (void)pthread_cond_signal(&adapter->startup_condition);
    (void)pthread_mutex_unlock(&adapter->startup_mutex);
}

static void *interrupt_thread(void *argument)
{
    bvstk_i2c_slave_neutrino_t *adapter =
        (bvstk_i2c_slave_neutrino_t *)argument;
    struct sigevent event;
    uint8_t frame[BVSTK_I2C_SLAVE_MAX_FRAME_BYTES];
    uint8_t read_window[BVSTK_I2C_SLAVE_READ_WINDOW_BYTES];
    int interrupt_id;

    if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1) {
        report_startup(adapter, BVSTK_ERR_DENIED);
        return NULL;
    }
    SIGEV_INTR_INIT(&event);
    interrupt_id = InterruptAttachEvent((int)BVSTK_IRQ_I2C_SLAVE,
                                        &event,
                                        0U);
    if (interrupt_id == -1) {
        report_startup(adapter, BVSTK_ERR_IO);
        return NULL;
    }
    adapter->interrupt_id = interrupt_id;
    report_startup(adapter, BVSTK_OK);

    for (;;) {
        bvstk_i2c_slave_irq_event_t irq_event;
        bvstk_status_t status = BVSTK_OK;

        memset(&irq_event, 0, sizeof(irq_event));

        if (InterruptWait(0, NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr,
                    "bvstkd: I2C slave InterruptWait failed: %s\n",
                    strerror(errno));
            continue;
        }
        status = bvstk_i2c_slave_hw_capture_irq(adapter->hardware,
                                                 &irq_event);
        if (status == BVSTK_OK && irq_event.irq_flags != 0U) {
            (void)pthread_mutex_lock(adapter->state_mutex);
            if ((irq_event.irq_flags & BVSTK_I2C_SLV_IRQ_DATA_VALID) != 0U) {
                status = process_write_event(adapter,
                                             frame,
                                             sizeof(frame),
                                             read_window,
                                             sizeof(read_window));
            }
            if (status == BVSTK_OK &&
                (irq_event.irq_flags & BVSTK_I2C_SLV_IRQ_RD_REQUEST) != 0U) {
                status = process_read_event(adapter,
                                            &irq_event,
                                            read_window,
                                            sizeof(read_window));
            }
            if ((irq_event.irq_flags &
                 (BVSTK_I2C_SLV_IRQ_FINAL_PACKET |
                  BVSTK_I2C_SLV_IRQ_ERR_ACK)) != 0U) {
                bvstk_i2c_slave_service_end_transaction(adapter->service);
            }
            (void)pthread_mutex_unlock(adapter->state_mutex);
        }
        if (status != BVSTK_OK && status != BVSTK_ERR_NOT_FOUND &&
            status != BVSTK_ERR_NOT_READY) {
            fprintf(stderr,
                    "bvstkd: I2C slave event failed: %s flags=0x%08x\n",
                    bvstk_status_string(status),
                    irq_event.irq_flags);
        }
        if (InterruptUnmask((int)BVSTK_IRQ_I2C_SLAVE,
                            interrupt_id) == -1) {
            fprintf(stderr,
                    "bvstkd: I2C slave InterruptUnmask failed: %s\n",
                    strerror(errno));
        }
    }
    return NULL;
}

bvstk_status_t bvstk_i2c_slave_neutrino_start(
    bvstk_i2c_slave_neutrino_t *adapter,
    bvstk_i2c_slave_hw_t *hardware,
    bvstk_i2c_slave_service_t *service,
    pthread_mutex_t *state_mutex)
{
    int create_result;
    bvstk_status_t status;

    if (adapter == NULL || hardware == NULL || service == NULL ||
        state_mutex == NULL || hardware->initialized == 0U ||
        service->initialized == 0U) {
        return BVSTK_ERR_MALFORMED;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->interrupt_id = -1;
    adapter->hardware = hardware;
    adapter->service = service;
    adapter->state_mutex = state_mutex;
    if (pthread_mutex_init(&adapter->startup_mutex, NULL) != 0) {
        return BVSTK_ERR_INTERNAL;
    }
    if (pthread_cond_init(&adapter->startup_condition, NULL) != 0) {
        (void)pthread_mutex_destroy(&adapter->startup_mutex);
        return BVSTK_ERR_INTERNAL;
    }
    if (bvstk_i2c_slave_hw_clear_irq(hardware) != BVSTK_OK) {
        (void)pthread_cond_destroy(&adapter->startup_condition);
        (void)pthread_mutex_destroy(&adapter->startup_mutex);
        return BVSTK_ERR_IO;
    }

    (void)pthread_mutex_lock(&adapter->startup_mutex);
    create_result = pthread_create(&adapter->thread,
                                   NULL,
                                   interrupt_thread,
                                   adapter);
    if (create_result != 0) {
        (void)pthread_mutex_unlock(&adapter->startup_mutex);
        (void)pthread_cond_destroy(&adapter->startup_condition);
        (void)pthread_mutex_destroy(&adapter->startup_mutex);
        return BVSTK_ERR_INTERNAL;
    }
    while (adapter->startup_complete == 0) {
        (void)pthread_cond_wait(&adapter->startup_condition,
                                &adapter->startup_mutex);
    }
    status = adapter->startup_status;
    (void)pthread_mutex_unlock(&adapter->startup_mutex);
    if (status != BVSTK_OK) {
        (void)pthread_join(adapter->thread, NULL);
        (void)pthread_cond_destroy(&adapter->startup_condition);
        (void)pthread_mutex_destroy(&adapter->startup_mutex);
        memset(adapter, 0, sizeof(*adapter));
        return status;
    }
    adapter->initialized = 1U;
    return BVSTK_OK;
}

void bvstk_i2c_slave_neutrino_stop(
    bvstk_i2c_slave_neutrino_t *adapter)
{
    /* bvstkd owns the IRQ thread until process termination. */
    (void)adapter;
}
