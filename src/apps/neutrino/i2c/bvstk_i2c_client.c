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

int bvstk_i2c_client_list_devices(
    bvstk_i2c_completion_device_t *devices,
    size_t device_capacity,
    size_t *out_device_count)
{
    FILE *response;
    FILE *error_output;
    char line[256];
    char *arguments[] = {"list"};
    size_t device_count = 0U;
    int result;

    if (out_device_count != NULL) {
        *out_device_count = 0U;
    }
    if ((devices == NULL && device_capacity != 0U) ||
        out_device_count == NULL) {
        return 2;
    }
    response = tmpfile();
    error_output = tmpfile();
    if (response == NULL || error_output == NULL) {
        if (response != NULL) {
            fclose(response);
        }
        if (error_output != NULL) {
            fclose(error_output);
        }
        return 1;
    }
    result = bvstk_i2c_client_execute(1,
                                      arguments,
                                      response,
                                      error_output);
    if (result == 0) {
        rewind(response);
        while (fgets(line, sizeof(line), response) != NULL) {
            unsigned int index;
            unsigned int address;
            char name[I2C_CFG_NAME_MAX];

            if (sscanf(line,
                       " %u: %31s addr=0x%x",
                       &index,
                       name,
                       &address) != 3 ||
                address > 0x7FU || device_count >= device_capacity) {
                continue;
            }
            (void)index;
            strncpy(devices[device_count].name,
                    name,
                    sizeof(devices[device_count].name) - 1U);
            devices[device_count].name[
                sizeof(devices[device_count].name) - 1U] = '\0';
            devices[device_count].addr_7b = (uint8_t)address;
            ++device_count;
        }
    }
    fclose(error_output);
    fclose(response);
    *out_device_count = device_count;
    return result;
}
