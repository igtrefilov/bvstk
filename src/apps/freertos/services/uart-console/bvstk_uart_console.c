#include "apps/freertos/services/uart-console/bvstk_uart_console.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "apps/freertos/console/console_common.h"
#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/console/utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xparameters.h"
#include "xuartps_hw.h"

#define UART_CONSOLE_FD CONSOLE_STREAM_FD_MIN
#define UART_CONSOLE_LINE_MAX 256U
#define UART_CONSOLE_STACK 4096U
#define UART_CONSOLE_PRIORITY (tskIDLE_PRIORITY + 2)

static int uart_write(void *context, const void *data, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    (void)context;
    if (bytes == NULL && length != 0U) return -1;
    for (index = 0; index < length; ++index) {
        XUartPs_SendByte(STDOUT_BASEADDRESS, bytes[index]);
    }
    return (int)length;
}

static void uart_console_task(void *argument)
{
    char line[UART_CONSOLE_LINE_MAX];
    size_t length = 0U;
    bool overflow = false;
    bool previous_cr = false;
    console_session_t session;
    (void)argument;

    if (console_stream_register(UART_CONSOLE_FD, uart_write, NULL) != 0) {
        vTaskDelete(NULL);
        return;
    }
    console_session_init(&session);
    console_print_banner(UART_CONSOLE_FD);
    console_print_prompt(UART_CONSOLE_FD, &session);

    for (;;) {
        unsigned char character;
        if (!XUartPs_IsReceiveData(STDOUT_BASEADDRESS)) {
            vTaskDelay(1);
            continue;
        }
        character = (unsigned char)XUartPs_ReadReg(STDOUT_BASEADDRESS,
                                                   XUARTPS_FIFO_OFFSET);
        if (character == '\n' && previous_cr) {
            previous_cr = false;
            continue;
        }
        previous_cr = character == '\r';
        if (character == '\r' || character == '\n') {
            line[length] = '\0';
            write_str(UART_CONSOLE_FD, "\r\n");
            if (overflow) {
                write_str(UART_CONSOLE_FD,
                          "ERR: line too long; command discarded\r\n");
            } else if (length != 0U) {
                process_console_line(line, UART_CONSOLE_FD, &session);
            }
            length = 0U;
            overflow = false;
            console_print_prompt(UART_CONSOLE_FD, &session);
        } else if (character == 3U) {
            length = 0U;
            overflow = false;
            write_str(UART_CONSOLE_FD, "^C\r\n");
            console_print_prompt(UART_CONSOLE_FD, &session);
        } else if ((character == 8U || character == 127U) &&
                   length != 0U && !overflow) {
            --length;
            write_str(UART_CONSOLE_FD, "\b \b");
        } else if (character >= 32U && character <= 126U) {
            if (length >= sizeof(line) - 1U) {
                overflow = true;
            } else if (!overflow) {
                line[length++] = (char)character;
                (void)uart_write(NULL, &character, 1U);
            }
        }
    }
}

int start_uart_console(void)
{
    return xTaskCreate(uart_console_task, "uart-console", UART_CONSOLE_STACK,
                       NULL, UART_CONSOLE_PRIORITY, NULL) == pdPASS ? 0 : -1;
}
