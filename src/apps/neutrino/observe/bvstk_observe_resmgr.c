#include "apps/neutrino/observe/bvstk_observe_resmgr.h"
#include "apps/neutrino/observe/bvstk_observe_ipc.h"
#include <errno.h>
#include <string.h>
#include <sys/iomsg.h>
#include <sys/resmgr.h>
#include <unistd.h>

static bvstk_observe_resmgr_t *owner;

static int query(resmgr_context_t *context, io_devctl_t *message, iofunc_ocb_t *ocb)
{
    int result = iofunc_devctl_default(context, message, ocb);
    if (result != (int)_RESMGR_DEFAULT) return result;
    if (message->i.dcmd != BVSTK_DCMD_OBSERVE) return ENOTTY;
    if (!owner || message->i.nbytes != (int)sizeof(bvstk_observe_message_t) ||
        context->info.msglen < (int)(sizeof(message->i) + sizeof(bvstk_observe_message_t))) return EINVAL;
    bvstk_observe_message_t *data = _DEVCTL_DATA(message->i);
    bvstk_neutrino_observe_query(owner->observer, owner->runtime, data);
    message->o.ret_val = 0;
    message->o.nbytes = sizeof(*data);
    return _RESMGR_PTR(context, &message->o, sizeof(message->o) + sizeof(*data));
}
static void *run(void *argument)
{
    bvstk_observe_resmgr_t *m = argument;
    dispatch_context_t *context = m->context;
    while ((context = dispatch_block(context)) != NULL) dispatch_handler(context);
    m->context = context;
    return NULL;
}
int bvstk_observe_resmgr_start(bvstk_observe_resmgr_t *m, bvstk_observation_t *observer,
    bvstk_neutrino_dcp2_runtime_t *runtime)
{
    if (!m || !observer || !runtime || owner) return 0;
    memset(m, 0, sizeof(*m));
    m->attach_id = -1;
    m->observer = observer; m->runtime = runtime;
    m->dispatch = dispatch_create();
    if (!m->dispatch) return 0;
    m->attributes.nparts_max = 1;
    m->attributes.msg_max_size = sizeof(io_devctl_t) + sizeof(bvstk_observe_message_t);
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &m->connect, _RESMGR_IO_NFUNCS, &m->io);
    m->io.devctl = query;
    iofunc_attr_init(&m->node, S_IFCHR | 0660, NULL, NULL);
    m->attach_id = resmgr_attach(m->dispatch, &m->attributes, BVSTK_OBSERVE_DEVICE_PATH,
        _FTYPE_ANY, 0, &m->connect, &m->io, &m->node);
    if (m->attach_id < 0) goto fail;
    m->context = dispatch_context_alloc(m->dispatch);
    if (!m->context) goto fail;
    owner = m;
    if (pthread_create(&m->thread, NULL, run, m)) { owner = NULL; goto fail; }
    return 1;
fail:
    if (m->context) dispatch_context_free(m->context);
    if (m->attach_id >= 0) resmgr_detach(m->dispatch, m->attach_id, 0);
    dispatch_destroy(m->dispatch);
    memset(m, 0, sizeof(*m));
    return 0;
}
