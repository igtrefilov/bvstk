#include "protocols/dcp2/bvstk_dcp2_dispatch.h"

int bvstk_dcp2_dispatch(const bvstk_dcp2_adapter_t *adapter,
    bvstk_dcp2_policy_session_t *session, const bvstk_dcp2_request_t *request,
    uint64_t now_ms, uint8_t *response, size_t capacity, size_t *response_size)
{
    if (!adapter || !session || !request || !response || !response_size) return -1;
    unsigned scope = request->service == BVSTK_DCP2_SERVICE_POLICY ? 3U :
        request->service == BVSTK_DCP2_SERVICE_I2C ? 1U :
        request->service == BVSTK_DCP2_SERVICE_MDIO ? 2U : 0U;
    if (adapter->lock) adapter->lock(adapter->context, scope);
    int result;
    uint16_t status = 0, size = 0;
    /* Keep the large named-policy reply off the small FreeRTOS task stack. */
    uint8_t *body = capacity >= 14U ? response + 14U : NULL;
    size_t body_capacity = capacity >= 14U ? capacity - 14U : 0U;
    bvstk_dcp2_policy_adapter_t policy = adapter->policy;
    policy.i2c = adapter->control.i2c;
    policy.mdio = adapter->control.mdio;
    if ((request->service == BVSTK_DCP2_SERVICE_I2C && !adapter->i2c_supported) ||
        (request->service == BVSTK_DCP2_SERVICE_MDIO && !adapter->mdio_supported) ||
        request->service == BVSTK_DCP2_SERVICE_SPI ||
        request->service == BVSTK_DCP2_SERVICE_UART) {
        status = bvstk_dcp2_status_from_bvstk(BVSTK_ERR_UNSUPPORTED);
        result = bvstk_dcp2_encode_response(response, capacity, request, status,
            NULL, 0, response_size);
    } else if (request->service == BVSTK_DCP2_SERVICE_POLICY) {
        status = bvstk_dcp2_policy_request(session, &policy, request->opcode,
            request->body, request->body_size, now_ms, body, body_capacity, &size);
        result = bvstk_dcp2_encode_response(response, capacity, request, status,
            status ? NULL : body, status ? 0U : size, response_size);
    } else if (scope && request->opcode == BVSTK_DCP2_OP_I2C_POLICY_SAVE) {
        status = bvstk_dcp2_status_from_bvstk(bvstk_dcp2_process_policy_save(
            policy.i2c, policy.mdio, request, policy.save, policy.context));
        result = bvstk_dcp2_encode_response(response, capacity, request, status,
            NULL, 0, response_size);
    } else {
        bvstk_control_api_t control = adapter->control;
        result = bvstk_dcp2_process_request(&control, request, response,
            capacity, response_size);
        if (!result && *response_size >= 14U &&
            !bvstk_dcp2_read_be16(response + 12U) && adapter->sync &&
            request->body_size && scope &&
            (request->opcode == BVSTK_DCP2_OP_I2C_WRITE_REG ||
             request->opcode == BVSTK_DCP2_OP_I2C_POLICY_SET ||
             request->opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_ADD ||
             request->opcode == BVSTK_DCP2_OP_I2C_POLICY_RULE_DELETE ||
             request->opcode == BVSTK_DCP2_OP_I2C_POLICY_CLEAR)) {
            size_t id;
            bvstk_status_t sync = scope == 1U ?
                bvstk_i2c_master_service_find_by_addr(control.i2c, request->body[0], &id) :
                bvstk_mdio_service_find_by_phy(control.mdio, request->body[0], &id);
            if (sync == BVSTK_OK) sync = adapter->sync(adapter->context,
                request->service, id, scope == 1U && request->opcode <= BVSTK_DCP2_OP_I2C_POLICY_SET);
            if (sync != BVSTK_OK) result = bvstk_dcp2_encode_response(response,
                capacity, request, bvstk_dcp2_status_from_bvstk(sync), NULL, 0, response_size);
        }
    }
    if (adapter->unlock) adapter->unlock(adapter->context, scope);
    return result;
}
