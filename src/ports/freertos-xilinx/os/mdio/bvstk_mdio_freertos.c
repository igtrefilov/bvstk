#include "ports/freertos-xilinx/os/mdio/bvstk_mdio_freertos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sleep.h"
#include "xtime_l.h"

static TaskHandle_t worker;
static uint64_t now_ms(void *context)
{
    XTime ticks;
    (void)context;
    XTime_GetTime(&ticks);
    return (uint64_t)ticks / (COUNTS_PER_SECOND / 1000U);
}
static void wait_ms(void *context, uint32_t ms)
{
    (void)context;
    /* One millisecond must not round down to zero at a 100 Hz RTOS tick.
     * MDIO waits are short; interrupts stay enabled, worker yields each pass. */
    usleep(ms * 1000U);
}
const bvstk_clock_t *bvstk_mdio_freertos_clock(void)
{
    static const bvstk_clock_t clock = {NULL, now_ms, wait_ms};
    return &clock;
}
static void run(void *argument)
{
    bvstk_mdio_service_t *service = argument;
    for (;;) {
        (void)bvstk_mdio_service_step(service, bvstk_platform_now_ms(), 20U);
        vTaskDelay(1U);
    }
}
bvstk_status_t bvstk_mdio_freertos_start(bvstk_mdio_service_t *service)
{
    if (!service || !service->initialized) return BVSTK_ERR_NOT_READY;
    if (worker) return BVSTK_ERR_BUSY;
    return xTaskCreate(run, "mdio", 1536U, service, tskIDLE_PRIORITY + 2U, &worker) == pdPASS
        ? BVSTK_OK : BVSTK_ERR_INTERNAL;
}
