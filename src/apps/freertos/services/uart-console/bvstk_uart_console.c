#include "apps/freertos/services/uart-console/bvstk_uart_console.h"

#include <stdbool.h>
#include <stddef.h>

#include "apps/freertos/console/console_completion.h"
#include "apps/freertos/console/console_common.h"
#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/console/utils.h"
#include "shared/cli/bvstk_line_editor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xparameters.h"
#include "xuartps_hw.h"

#define UART_CONSOLE_FD CONSOLE_STREAM_FD_MIN
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

static void uart_editor_prompt(void *context)
{
    console_session_t *session = (console_session_t *)context;

    if (session != NULL) {
        console_print_prompt(UART_CONSOLE_FD, session);
    }
}

static int uart_editor_submit(void *context, const char *line, size_t length)
{
    console_session_t *session = (console_session_t *)context;

    if (session == NULL || line == NULL) {
        return BVSTK_LINE_EDITOR_SUBMIT_STOP;
    }
    if (length != 0U) {
        process_console_line(line, UART_CONSOLE_FD, session);
        /* The dispatcher uses one legacy process-wide close flag for socket
         * sessions.  A physical UART cannot be closed, so consume that flag
         * here after commands such as `quit` or `reboot`. */
        if (utils_should_close()) {
            utils_reset_close();
        }
    }
    /* Unlike a socket session, the physical UART remains available after
     * `quit`; keep the console task alive and print a fresh prompt. */
    return BVSTK_LINE_EDITOR_SUBMIT_PROMPT;
}

static int uart_editor_complete(void *context,
                                const char *line,
                                size_t line_length,
                                size_t cursor,
                                bvstk_line_editor_completion_t *result)
{
    const console_session_t *session =
        (const console_session_t *)context;

    return bvstk_console_complete(session,
                                  line,
                                  line_length,
                                  cursor,
                                  result);
}

static void uart_console_task(void *argument)
{
    static bvstk_line_editor_t editor;
    bvstk_line_editor_config_t editor_config = {0};
    bool previous_cr = false;
    console_session_t session;
    (void)argument;

    if (console_stream_register(UART_CONSOLE_FD, uart_write, NULL) != 0) {
        vTaskDelete(NULL);
        return;
    }
    console_session_init(&session);
    editor_config.context = &session;
    editor_config.write = uart_write;
    editor_config.prompt = uart_editor_prompt;
    editor_config.submit = uart_editor_submit;
    editor_config.complete = uart_editor_complete;
    editor_config.tab = NULL;
    editor_config.eof_on_empty = 0;
    bvstk_line_editor_init(&editor, &editor_config);
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
        if (character == '\r') {
            previous_cr = true;
            character = '\n';
        } else {
            previous_cr = false;
        }
        if (bvstk_line_editor_handle_byte(&editor, character) != 0) {
            break;
        }
    }
    vTaskDelete(NULL);
}

int start_uart_console(void)
{
    return xTaskCreate(uart_console_task, "uart-console", UART_CONSOLE_STACK,
                       NULL, UART_CONSOLE_PRIORITY, NULL) == pdPASS ? 0 : -1;
}
