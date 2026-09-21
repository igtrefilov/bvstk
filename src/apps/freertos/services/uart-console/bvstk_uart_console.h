#ifndef BVSTK_UART_CONSOLE_SERVICE_H
#define BVSTK_UART_CONSOLE_SERVICE_H

/* Start the common BVSTK command dispatcher on the already configured PS
 * UART. The task is part of the normal FreeRTOS application; it is not an
 * SD-only application. */
int start_uart_console(void);

#endif
