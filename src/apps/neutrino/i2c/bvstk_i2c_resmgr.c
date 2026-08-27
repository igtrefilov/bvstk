#include "apps/neutrino/i2c/bvstk_i2c_resmgr.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/iomsg.h>
#include <sys/resmgr.h>
#include <unistd.h>

#include "apps/neutrino/i2c/bvstk_i2c_ipc.h"

static bvstk_i2c_resmgr_t *s_manager;

static int io_devctl(resmgr_context_t *context,
                     io_devctl_t *message,
                     iofunc_ocb_t *ocb)
{
    bvstk_i2c_ipc_message_t *data;
    int default_result;

    default_result = iofunc_devctl_default(context, message, ocb);
    if (default_result != (int)_RESMGR_DEFAULT) {
        return default_result;
    }
    if (message->i.dcmd != BVSTK_DCMD_I2C_EXECUTE) {
        return ENOTTY;
    }
    if (s_manager == NULL || s_manager->runtime == NULL ||
        message->i.nbytes < 0 ||
        (size_t)message->i.nbytes < sizeof(bvstk_i2c_ipc_message_t)) {
        return EINVAL;
    }
    data = (bvstk_i2c_ipc_message_t *)_DEVCTL_DATA(message->i);
    if (data->version != BVSTK_I2C_IPC_VERSION ||
        memchr(data->command, '\0', sizeof(data->command)) == NULL) {
        return EINVAL;
    }
    memset(data->response, 0, sizeof(data->response));
    data->result = bvstk_neutrino_i2c_runtime_command(
        s_manager->runtime,
        data->command,
        data->response,
        sizeof(data->response));
    data->response_length = (uint32_t)strlen(data->response);
    data->version = BVSTK_I2C_IPC_VERSION;
    message->o.ret_val = 0;
    message->o.nbytes = sizeof(*data);
    return _RESMGR_PTR(context,
                       &message->o,
                       sizeof(message->o) + sizeof(*data));
}

static void *dispatch_thread(void *argument)
{
    bvstk_i2c_resmgr_t *manager = (bvstk_i2c_resmgr_t *)argument;
    dispatch_context_t *context = manager->context;

    while ((context = dispatch_block(context)) != NULL) {
        dispatch_handler(context);
    }
    manager->context = context;
    return NULL;
}

int bvstk_i2c_resmgr_start(bvstk_i2c_resmgr_t *manager,
                           bvstk_neutrino_i2c_runtime_t *runtime)
{
    if (manager == NULL || runtime == NULL ||
        !bvstk_neutrino_i2c_runtime_ready(runtime) || s_manager != NULL) {
        return 0;
    }
    memset(manager, 0, sizeof(*manager));
    manager->attach_id = -1;
    manager->runtime = runtime;
    manager->dispatch = dispatch_create();
    if (manager->dispatch == NULL) {
        return 0;
    }
    memset(&manager->resmgr_attributes,
           0,
           sizeof(manager->resmgr_attributes));
    manager->resmgr_attributes.nparts_max = 1;
    manager->resmgr_attributes.msg_max_size =
        sizeof(io_devctl_t) + sizeof(bvstk_i2c_ipc_message_t);
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS,
                     &manager->connect_functions,
                     _RESMGR_IO_NFUNCS,
                     &manager->io_functions);
    manager->io_functions.devctl = io_devctl;
    iofunc_attr_init(&manager->attribute,
                     S_IFCHR | 0660,
                     NULL,
                     NULL);
    manager->attach_id = resmgr_attach(manager->dispatch,
                                        &manager->resmgr_attributes,
                                        BVSTK_I2C_DEVICE_PATH,
                                        _FTYPE_ANY,
                                        0,
                                        &manager->connect_functions,
                                        &manager->io_functions,
                                        &manager->attribute);
    if (manager->attach_id == -1) {
        dispatch_destroy(manager->dispatch);
        memset(manager, 0, sizeof(*manager));
        return 0;
    }
    manager->context = dispatch_context_alloc(manager->dispatch);
    if (manager->context == NULL) {
        resmgr_detach(manager->dispatch, manager->attach_id, 0);
        dispatch_destroy(manager->dispatch);
        memset(manager, 0, sizeof(*manager));
        return 0;
    }
    s_manager = manager;
    if (pthread_create(&manager->thread,
                       NULL,
                       dispatch_thread,
                       manager) != 0) {
        s_manager = NULL;
        dispatch_context_free(manager->context);
        resmgr_detach(manager->dispatch, manager->attach_id, 0);
        dispatch_destroy(manager->dispatch);
        memset(manager, 0, sizeof(*manager));
        return 0;
    }
    manager->initialized = 1U;
    return 1;
}
