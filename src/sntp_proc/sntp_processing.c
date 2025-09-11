#include "sntp_processing.h"
#include <time.h>
#include <sys/time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#include "sntp.h"

void time_sync_task(void *param)
{

	ip_addr_t server_ip;
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    IP4_ADDR(&server_ip, 51, 250, 68, 198);
    sntp_setserver(0, &server_ip);
    sntp_init();

    vTaskDelete(NULL);
}
