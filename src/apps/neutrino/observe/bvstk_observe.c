#include "apps/neutrino/observe/bvstk_observe.h"
#include <string.h>

void bvstk_neutrino_observe_query(bvstk_observation_t *observer,
    bvstk_neutrino_dcp2_runtime_t *runtime, bvstk_observe_message_t *m)
{
    bvstk_status_t status = BVSTK_OK;
    int slot = -1, address;
    if (!m) return;
    memset(&m->batch, 0, sizeof(m->batch));
    m->uptime_ms = bvstk_platform_now_ms();
    if (!observer || !runtime) status = BVSTK_ERR_NOT_READY;
    else if (m->version != BVSTK_OBSERVE_VERSION ||
        m->bus >= BVSTK_JOURNAL_BUSES || m->channel > BVSTK_JOURNAL_TELEMETRY ||
        m->address < -1 || m->address > (m->bus == BVSTK_EVENT_BUS_I2C ? 127 : 31) ||
        !memchr(m->device, 0, sizeof(m->device))) status = BVSTK_ERR_MALFORMED;
    address = m->address;
    if (status == BVSTK_OK && m->device[0]) {
        size_t id;
        if (m->bus == BVSTK_EVENT_BUS_I2C) {
            bvstk_neutrino_i2c_runtime_lock(runtime->i2c);
            status = bvstk_i2c_master_service_find_by_name(
                bvstk_neutrino_i2c_runtime_service(runtime->i2c), m->device, &id);
            if (status == BVSTK_OK) slot = (int)id;
            bvstk_neutrino_i2c_runtime_unlock(runtime->i2c);
        } else {
            bvstk_mdio_device_info_t device;
            bvstk_neutrino_mdio_runtime_lock(runtime->mdio);
            bvstk_mdio_service_t *s = bvstk_neutrino_mdio_runtime_service(runtime->mdio);
            status = bvstk_mdio_service_find_by_name(s, m->device, &id);
            if (status == BVSTK_OK) status = bvstk_mdio_service_device_info(s, id, &device);
            if (status == BVSTK_OK) {
                if (address >= 0 && address != device.phy_addr) status = BVSTK_ERR_NOT_FOUND;
                else address = device.phy_addr;
            }
            bvstk_neutrino_mdio_runtime_unlock(runtime->mdio);
        }
    }
    if (status == BVSTK_OK)
        bvstk_observation_read(observer, m->bus, m->channel, address, slot, m->after, &m->batch);
    m->status = (uint32_t)status;
}
