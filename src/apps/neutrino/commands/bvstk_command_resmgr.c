#include "apps/neutrino/commands/bvstk_command_resmgr.h"
#include "apps/neutrino/commands/bvstk_command_ipc.h"
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iomsg.h>
#include <sys/neutrino.h>
#include <sys/resmgr.h>

enum { OUTPUT_MAX = 256U * 1024U };
typedef struct {
    iofunc_ocb_t base;
    uint8_t *data;
    size_t size, capacity;
    uint32_t transfer;
    int result;
} command_ocb_t;
static bvstk_command_resmgr_t *manager;
static iofunc_ocb_t *allocate(resmgr_context_t *context, iofunc_attr_t *attribute)
{
    (void)context; (void)attribute;
    return (iofunc_ocb_t *)calloc(1, sizeof(command_ocb_t));
}
static void release(iofunc_ocb_t *base)
{
    command_ocb_t *ocb = (command_ocb_t *)base;
    free(ocb->data); free(ocb);
}
static int output(void *context, const void *data, size_t size)
{
    command_ocb_t *ocb = context;
    if (size > OUTPUT_MAX - ocb->size) return -1;
    if (ocb->size + size > ocb->capacity) {
        size_t capacity = ocb->capacity ? ocb->capacity : 4096U;
        while (capacity < ocb->size + size) capacity *= 2U;
        void *memory = realloc(ocb->data, capacity);
        if (!memory) return -1;
        ocb->data = memory; ocb->capacity = capacity;
    }
    if (size) memcpy(ocb->data + ocb->size, data, size);
    ocb->size += size; return (int)size;
}
static int io_devctl(resmgr_context_t *context, io_devctl_t *message, iofunc_ocb_t *base)
{
    int rc = iofunc_devctl_default(context, message, base);
    if (rc != (int)_RESMGR_DEFAULT) return rc;
    if (message->i.dcmd != BVSTK_DCMD_COMMAND) return ENOTTY;
    if (!manager || !base || message->i.nbytes != sizeof(bvstk_command_ipc_t) ||
        context->info.msglen < 0 || (size_t)context->info.msglen < sizeof(message->i) + sizeof(bvstk_command_ipc_t)) return EINVAL;
    bvstk_command_ipc_t *data = (bvstk_command_ipc_t *)_DEVCTL_DATA(message->i);
    command_ocb_t *ocb = (command_ocb_t *)base;
    if (data->version != BVSTK_COMMAND_VERSION) return EINVAL;
    if (data->operation == BVSTK_COMMAND_START) {
        if (!memchr(data->command, '\0', sizeof(data->command)) ||
            !memchr(data->cwd, '\0', sizeof(data->cwd))) return EINVAL;
        free(ocb->data); ocb->data = NULL; ocb->size = ocb->capacity = 0;
        if (!++ocb->transfer) ++ocb->transfer;
        const bvstk_output_t sink = {output, ocb};
        ocb->result = bvstk_neutrino_command_execute(manager->runtime, data->cwd, data->command, &sink);
        data->offset = 0;
    } else if (data->operation != BVSTK_COMMAND_READ || data->transfer != ocb->transfer || !ocb->transfer)
        return EINVAL;
    if (data->offset > ocb->size) return EINVAL;
    data->transfer = ocb->transfer; data->total = (uint32_t)ocb->size; data->result = ocb->result;
    size_t size = ocb->size - data->offset;
    if (size > sizeof(data->data)) size = sizeof(data->data);
    memset(data->command, 0, sizeof(data->command)); memset(data->cwd, 0, sizeof(data->cwd));
    memset(data->data, 0, sizeof(data->data));
    if (size) memcpy(data->data, ocb->data + data->offset, size);
    data->length = (uint32_t)size;
    message->o.ret_val = 0; message->o.nbytes = sizeof(*data);
    return _RESMGR_PTR(context, &message->o, sizeof(message->o) + sizeof(*data));
}
static void *run(void *argument)
{
    bvstk_command_resmgr_t *m = argument;
    dispatch_context_t *context = m->context;
    while ((context = dispatch_block(context))) dispatch_handler(context);
    m->context = context; return NULL;
}
int bvstk_command_resmgr_start(bvstk_command_resmgr_t *m, bvstk_neutrino_command_t *runtime)
{
    if (!m || !runtime || manager) return 0;
    memset(m, 0, sizeof(*m)); m->attach_id = -1; m->runtime = runtime;
    m->dispatch = dispatch_create();
    if (!m->dispatch) return 0;
    m->attributes.nparts_max = 1;
    m->attributes.msg_max_size = sizeof(io_devctl_t) + sizeof(bvstk_command_ipc_t);
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &m->connect, _RESMGR_IO_NFUNCS, &m->io);
    m->io.devctl = io_devctl;
    iofunc_attr_init(&m->node, S_IFCHR | 0660, NULL, NULL);
    m->functions = (iofunc_funcs_t){_IOFUNC_NFUNCS, allocate, release,
        iofunc_attr_lock, iofunc_attr_unlock, iofunc_attr_trylock};
    m->mount.funcs = &m->functions; m->node.mount = &m->mount;
    m->attach_id = resmgr_attach(m->dispatch, &m->attributes, BVSTK_COMMAND_PATH,
        _FTYPE_ANY, 0, &m->connect, &m->io, &m->node);
    if (m->attach_id < 0) goto fail;
    m->context = dispatch_context_alloc(m->dispatch);
    if (!m->context) goto fail;
    manager = m;
    if (!pthread_create(&m->thread, NULL, run, m)) return 1;
    manager = NULL; dispatch_context_free(m->context);
fail:
    if (m->attach_id >= 0) resmgr_detach(m->dispatch, m->attach_id, 0);
    dispatch_destroy(m->dispatch); memset(m, 0, sizeof(*m)); return 0;
}
