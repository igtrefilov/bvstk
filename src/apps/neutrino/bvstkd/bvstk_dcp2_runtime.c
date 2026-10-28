#include "apps/neutrino/bvstkd/bvstk_dcp2_runtime.h"

#include <stdio.h>
#include "hardware/boards/ax7020/bvstk_hw_config.h"

static int request_mutates_i2c(const bvstk_dcp2_request_t *request)
{
    return request != NULL &&
           request->service == BVSTK_DCP2_SERVICE_I2C &&
           (request->opcode == BVSTK_DCP2_OP_I2C_WRITE_REG ||
            request->opcode == BVSTK_DCP2_OP_I2C_POLICY_SET ||
            request->opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_ADD ||
            request->opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ||
            request->opcode == BVSTK_DCP2_OP_I2C_POLICY_CLEAR);
}

static void sync_dcp_i2c_mutation(
    bvstk_neutrino_i2c_runtime_t *runtime,
    const bvstk_dcp2_request_t *request,
    uint8_t *response,
    size_t response_size)
{
    size_t device_id;

    if (!request_mutates_i2c(request) || request->body_size == 0U ||
        response == NULL || response_size < 14U ||
        bvstk_dcp2_read_be16(response + 12U) != 0U ||
        bvstk_i2c_master_service_find_by_addr(
            bvstk_neutrino_i2c_runtime_service(runtime),
            request->body[0],
            &device_id) != BVSTK_OK) {
        return;
    }
    if (request->opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_ADD ||
        request->opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ||
        request->opcode == BVSTK_DCP2_OP_I2C_POLICY_CLEAR) {
        if (!bvstk_neutrino_i2c_runtime_sync_policy_locked(runtime, device_id)) {
            fprintf(stderr,
                    "bvstkd: failed to mirror DCP2 I2C policy for address 0x%02x\n",
                    (unsigned)request->body[0]);
            bvstk_dcp2_write_be16(response + 12U,
                bvstk_dcp2_status_from_bvstk(BVSTK_ERR_INTERNAL));
        }
        return;
    }
    /* Physical writes and legacy mode changes only update the RAM mirror. */
    if (!bvstk_neutrino_i2c_runtime_sync_locked(runtime, device_id)) {
        fprintf(stderr,
                "bvstkd: failed to mirror DCP2 I2C mutation for address "
                "0x%02x\n",
                (unsigned)request->body[0]);
        bvstk_dcp2_write_be16(response + 12U,
            bvstk_dcp2_status_from_bvstk(BVSTK_ERR_INTERNAL));
    }
}

int bvstk_neutrino_dcp2_request_locked(bvstk_neutrino_i2c_runtime_t *runtime,
    bvstk_control_api_t *control, const bvstk_dcp2_request_t *request,
    uint8_t *response, size_t capacity, size_t *response_size)
{
    int result = bvstk_dcp2_process_request(control, request, response, capacity, response_size);
    if (result == 0) sync_dcp_i2c_mutation(runtime, request, response, *response_size);
    return result;
}

static bvstk_status_t save_policy(void *context, uint8_t service, size_t id)
{
    bvstk_neutrino_dcp2_runtime_t *runtime = context;
    if (service == BVSTK_DCP2_SERVICE_I2C) {
        if (!bvstk_neutrino_i2c_runtime_service(runtime->i2c)) return BVSTK_ERR_NOT_READY;
        return bvstk_neutrino_i2c_runtime_save_policy_locked(runtime->i2c, id) ? BVSTK_OK : BVSTK_ERR_IO;
    }
    if (service == BVSTK_DCP2_SERVICE_MDIO) {
        if (!bvstk_neutrino_mdio_runtime_service(runtime->mdio)) return BVSTK_ERR_NOT_READY;
        return bvstk_neutrino_mdio_runtime_save_locked(runtime->mdio, id) ? BVSTK_OK : BVSTK_ERR_IO;
    }
    return BVSTK_ERR_UNSUPPORTED;
}
static bvstk_status_t policy_catalog(void *context, uint8_t service, size_t id,
    bvstk_dcp2_policy_catalog_t *catalog)
{
    bvstk_neutrino_dcp2_runtime_t *runtime = context;
    if (service == BVSTK_DCP2_SERVICE_I2C) {
        if (!bvstk_neutrino_i2c_runtime_service(runtime->i2c)) return BVSTK_ERR_NOT_READY;
        const i2c_device_config_t *config =
            bvstk_neutrino_i2c_config_store_get(&runtime->i2c->config_store, id);
        return config ? bvstk_dcp2_policy_catalog_from_profile(catalog, &config->profile, config->reg_count) :
            BVSTK_ERR_NOT_FOUND;
    }
    if (service == BVSTK_DCP2_SERVICE_MDIO) {
        if (!bvstk_neutrino_mdio_runtime_service(runtime->mdio)) return BVSTK_ERR_NOT_READY;
        const mdio_phy_config_t *config =
            bvstk_neutrino_mdio_config_store_get(&runtime->mdio->config_store, id);
        return config ? bvstk_dcp2_policy_catalog_from_profile(catalog, &config->profile, config->reg_count) :
            BVSTK_ERR_NOT_FOUND;
    }
    return BVSTK_ERR_UNSUPPORTED;
}
static bvstk_status_t policy_changed(void *context, uint8_t service, size_t id)
{
    bvstk_neutrino_dcp2_runtime_t *runtime = context;
    if (service == BVSTK_DCP2_SERVICE_I2C)
        return bvstk_neutrino_i2c_runtime_sync_policy_locked(runtime->i2c, id) ? BVSTK_OK : BVSTK_ERR_INTERNAL;
    if (service == BVSTK_DCP2_SERVICE_MDIO)
        return bvstk_neutrino_mdio_runtime_sync_locked(runtime->mdio, id) ? BVSTK_OK : BVSTK_ERR_INTERNAL;
    return BVSTK_ERR_UNSUPPORTED;
}

static void lock_runtime(void *context, unsigned scope)
{
    bvstk_neutrino_dcp2_runtime_t *r = context;
    if (scope & 1U) bvstk_neutrino_i2c_runtime_lock(r->i2c);
    if (scope & 2U) bvstk_neutrino_mdio_runtime_lock(r->mdio);
}
static void unlock_runtime(void *context, unsigned scope)
{
    bvstk_neutrino_dcp2_runtime_t *r = context;
    if (scope & 2U) bvstk_neutrino_mdio_runtime_unlock(r->mdio);
    if (scope & 1U) bvstk_neutrino_i2c_runtime_unlock(r->i2c);
}
static bvstk_status_t sync_runtime(void *context, uint8_t service, size_t id, int cache)
{
    bvstk_neutrino_dcp2_runtime_t *r = context;
    if (service == BVSTK_DCP2_SERVICE_I2C)
        return (cache ? bvstk_neutrino_i2c_runtime_sync_locked(r->i2c, id) :
            bvstk_neutrino_i2c_runtime_sync_policy_locked(r->i2c, id)) ? BVSTK_OK : BVSTK_ERR_INTERNAL;
    return service == BVSTK_DCP2_SERVICE_MDIO &&
        bvstk_neutrino_mdio_runtime_sync_locked(r->mdio, id) ? BVSTK_OK : BVSTK_ERR_INTERNAL;
}
void bvstk_neutrino_dcp2_adapter(bvstk_dcp2_adapter_t *a,
    bvstk_neutrino_dcp2_runtime_t *runtime, const bvstk_control_api_t *control)
{
    *a = (bvstk_dcp2_adapter_t){0};
    a->context = runtime; a->control = *control;
    a->i2c_supported = BVSTK_PL_HAS_I2C_CORE;
    a->mdio_supported = BVSTK_PL_HAS_MDIO_CORE;
    a->lock = lock_runtime; a->unlock = unlock_runtime; a->sync = sync_runtime;
    a->policy = (bvstk_dcp2_policy_adapter_t){control->i2c, control->mdio,
        runtime, policy_catalog, policy_changed, save_policy};
}
int bvstk_neutrino_dcp2_dispatch(bvstk_neutrino_dcp2_runtime_t *runtime,
    bvstk_dcp2_policy_session_t *session, bvstk_control_api_t *control,
    const bvstk_dcp2_request_t *request, uint8_t *response, size_t capacity, size_t *size)
{
    bvstk_dcp2_adapter_t adapter;
    bvstk_neutrino_dcp2_adapter(&adapter, runtime, control);
    return bvstk_dcp2_dispatch(&adapter, session, request, bvstk_platform_now_ms(), response, capacity, size);
}
