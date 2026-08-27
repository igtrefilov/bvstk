#include "apps/neutrino/i2c/bvstk_i2c_client.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "apps/neutrino/i2c/bvstk_i2c_ipc.h"

static int argument_valid(const char *argument)
{
    const unsigned char *cursor = (const unsigned char *)argument;

    if (argument == NULL || argument[0] == '\0') {
        return 0;
    }
    while (*cursor != '\0') {
        if (isspace(*cursor)) {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int build_command(bvstk_i2c_ipc_message_t *message,
                         int argument_count,
                         char *const arguments[])
{
    size_t length = strlen("i2c");
    int index;

    strcpy(message->command, "i2c");
    for (index = 0; index < argument_count; ++index) {
        size_t argument_length;

        if (!argument_valid(arguments[index])) {
            return 0;
        }
        argument_length = strlen(arguments[index]);
        if (length + 1U + argument_length >= sizeof(message->command)) {
            return 0;
        }
        message->command[length++] = ' ';
        memcpy(message->command + length,
               arguments[index],
               argument_length + 1U);
        length += argument_length;
    }
    return 1;
}

int bvstk_i2c_client_execute(int argument_count,
                             char *const arguments[],
                             FILE *output,
                             FILE *error_output)
{
    bvstk_i2c_ipc_message_t *message;
    int fd;
    int devctl_result;
    int result;

    if (argument_count < 0 || (argument_count != 0 && arguments == NULL) ||
        output == NULL || error_output == NULL) {
        return 2;
    }
    message = (bvstk_i2c_ipc_message_t *)calloc(1U, sizeof(*message));
    if (message == NULL) {
        fprintf(error_output, "i2c: out of memory\n");
        return 1;
    }
    message->version = BVSTK_I2C_IPC_VERSION;
    if (!build_command(message, argument_count, arguments)) {
        fprintf(error_output, "i2c: invalid or overlong command\n");
        free(message);
        return 2;
    }
    fd = open(BVSTK_I2C_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(error_output,
                "i2c: cannot open %s: %s\n",
                BVSTK_I2C_DEVICE_PATH,
                strerror(errno));
        free(message);
        return 1;
    }
    devctl_result = devctl(fd,
                           BVSTK_DCMD_I2C_EXECUTE,
                           message,
                           sizeof(*message),
                           NULL);
    if (devctl_result != EOK) {
        fprintf(error_output,
                "i2c: command transport failed: %s\n",
                strerror(devctl_result));
        close(fd);
        free(message);
        return 1;
    }
    close(fd);
    if (message->version != BVSTK_I2C_IPC_VERSION ||
        message->response_length >= sizeof(message->response)) {
        fprintf(error_output, "i2c: invalid response from bvstkd\n");
        free(message);
        return 1;
    }
    message->response[message->response_length] = '\0';
    if (message->response_length != 0U) {
        (void)fwrite(message->response,
                     1U,
                     message->response_length,
                     output);
    }
    result = message->result;
    free(message);
    return result >= 0 && result <= 2 ? result : 1;
}
