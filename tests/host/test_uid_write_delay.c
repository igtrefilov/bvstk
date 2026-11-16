/* EEPROM's 5 ms write cycle must survive a 100 Hz FreeRTOS scheduler. */
#include <assert.h>
#include <stdio.h>
#include "FreeRTOS.h"
#undef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((ms) / 10U)
#define portTICK_PERIOD_MS 10U
#include "ports/freertos-xilinx/os/bvstk_platform_freertos.c"

static unsigned microseconds, delays;
static TickType_t last_ticks;
void usleep(unsigned long us) { microseconds += (unsigned)us; }
void vTaskDelay(TickType_t ticks) { ++delays; last_ticks = ticks; }
TickType_t xTaskGetTickCount(void) { return 0; }

int main(void)
{
    bvstk_platform_sleep_ms(5);
    assert(microseconds == 5000 && !delays);
    bvstk_platform_sleep_ms(20);
    assert(microseconds == 5000 && delays == 1 && last_ticks == 2);
    bvstk_platform_sleep_ms(0);
    assert(microseconds == 5000 && delays == 2 && last_ticks == 0);
    puts("EEPROM write delay: 5 ms retained with 100 Hz FreeRTOS; tick waits/yield preserved");
    return 0;
}
