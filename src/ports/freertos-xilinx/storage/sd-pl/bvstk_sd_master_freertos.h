#ifndef BVSTK_SD_MASTER_FREERTOS_H
#define BVSTK_SD_MASTER_FREERTOS_H

#include "services/sd/bvstk_sd_service.h"

/* Call once from a startup task, AFTER the scheduler initialized its GIC,
 * before starting console clients. Exclusively owns SPI master/DMA/BRAM/IRQ.
 * It does not initialize or write the card. */
bvstk_status_t bvstk_sd_master_freertos_start(void);
/* Schedule one startup task. The GIC is initialized by the FreeRTOS port
 * when the scheduler starts. Raw-only applications can use this entry point;
 * normal filesystem builds call start() from the sd_pl_card startup task.
 * Never call start() from main() before vTaskStartScheduler. */
bvstk_status_t bvstk_sd_master_freertos_schedule_start(void);
int bvstk_sd_master_freertos_startup_done(void);
bvstk_sd_service_t *bvstk_sd_master_freertos_service(void);

#endif
