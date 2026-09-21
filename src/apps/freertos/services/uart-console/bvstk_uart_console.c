#include "apps/freertos/services/uart-console/bvstk_uart_console.h"

#include <stdbool.h>
#include <stddef.h>

#include "apps/freertos/console/console_completion.h"
#include "apps/freertos/console/console_common.h"
#include "apps/freertos/console/console_stream.h"
#include "apps/freertos/console/utils.h"
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/services/dcp2/dcp2_server.h"
#include "apps/freertos/services/http/http_server.h"
#include "apps/freertos/services/lan/bvstk_lan.h"
#include "apps/freertos/services/ssh/bvstk_ssh_server.h"
#include "apps/freertos/storage/qspi/qspi_fs.h"
#include "apps/freertos/storage/sd-pl/sd_pl_card.h"
#include "apps/freertos/storage/sd/sd_card.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"
#include "shared/cli/bvstk_line_editor.h"
#include "ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "xuartps_hw.h"

#define UART_CONSOLE_FD CONSOLE_STREAM_FD_MIN
#define UART_CONSOLE_STACK 4096U
#define UART_CONSOLE_PRIORITY (tskIDLE_PRIORITY + 2)
#define UART_STARTUP_WAIT_MS 60000U

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

static bool uart_startup_diagnostics_done(void)
{
    if (!sd_card_startup_done() ||
        !qspi_fs_startup_done() ||
        !config_store_startup_done() ||
        !lan_startup_done() ||
        !http_server_startup_done() ||
        !dcp2_server_startup_done() ||
        !ssh_server_startup_done()) {
        return false;
    }
#if BVSTK_PL_HAS_SD_CONTROLLER && BVSTK_PL_SD_AUTOSTART_FILESYSTEM
    if (!sd_pl_card_startup_done()) return false;
#elif !BVSTK_PL_SD_AUTOSTART_FILESYSTEM
    if (!bvstk_sd_master_freertos_startup_done()) return false;
#endif
    return true;
}

static void uart_wait_for_startup_diagnostics(void)
{
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(UART_STARTUP_WAIT_MS);
    const TickType_t step = pdMS_TO_TICKS(10U) ? pdMS_TO_TICKS(10U) : 1U;

    while (!uart_startup_diagnostics_done() &&
           (xTaskGetTickCount() - start) < timeout) {
        vTaskDelay(step);
    }
    if (!uart_startup_diagnostics_done()) {
        xil_printf("UART: startup diagnostics wait timed out; opening console\r\n");
    }
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
    uart_wait_for_startup_diagnostics();
    write_str(UART_CONSOLE_FD, "Hello from bvstk\r\n\r\n");
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
