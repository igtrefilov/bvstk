#include "apps/freertos/services/dcp2/dcp2_server.h"

#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "xil_io.h"
#include "xil_printf.h"

#include "apps/freertos/config/config_store.h"
#include "apps/freertos/runtime/bvstk_runtime.h"
#include "apps/freertos/services/dcp2/dcp2_notify.h"
#include "apps/freertos/services/dcp2/dcp2_policy_store.h"
#include "apps/freertos/runtime/bvstk_observer.h"
#include "apps/freertos/services/dcp2/dcp2_stream_sim.h"
#include "apps/freertos/services/lan/bvstk_lan.h"
#include "apps/freertos/storage/fs/fs_dcp2_backend.h"
#include "protocols/dcp2/bvstk_dcp2_fs.h"
#include "protocols/dcp2/bvstk_dcp2_cache.h"
#include "protocols/dcp2/bvstk_dcp2_control.h"
#include "protocols/dcp2/bvstk_dcp2_session.h"
#include "shared/interfaces/bvstk_clock.h"
#include "hardware/boards/ax7020/bvstk_hw_config.h"

enum { DCP2_PORT_DEFAULT = 8889, DCP2_THREAD_STACK = 4096,
    DCP2_HDR_LEN = 8, DCP2_MAX_PAYLOAD = BVSTK_DCP2_MAX_PAYLOAD, DCP2_MIN_PAYLOAD = 4 };
static uint16_t s_port = DCP2_PORT_DEFAULT;
static volatile int s_startup_done;
static uint8_t s_rx_buf[DCP2_HDR_LEN + DCP2_MAX_PAYLOAD];
static bvstk_dcp2_session_t session;
static bvstk_dcp2_adapter_t adapter;
int dcp2_server_startup_done(void) { return s_startup_done != 0; }
static bvstk_status_t sync_device(void *context, uint8_t service, size_t id, int cache)
{
    (void)context;
    return service != BVSTK_DCP2_SERVICE_I2C ||
        (cache ? bvstk_runtime_i2c_sync_device(id) : bvstk_runtime_i2c_sync_policy(id)) ?
        BVSTK_OK : BVSTK_ERR_INTERNAL;
}
static uint64_t observation_head(void *context, unsigned bus, unsigned channel)
{ (void)context; return bvstk_observer_head(bus, channel); }
static void observation_read(void *context, unsigned bus, unsigned channel,
    uint64_t after, bvstk_journal_batch_t *out)
{ (void)context; bvstk_observer_read(bus, channel, -1, after, out); }
static void refresh_adapter(void)
{
    adapter.control.i2c = bvstk_runtime_i2c_master_service();
    adapter.control.mdio = bvstk_runtime_mdio_service();
    adapter.control.memory = bvstk_platform_memory();
    adapter.policy = (bvstk_dcp2_policy_adapter_t){adapter.control.i2c, adapter.control.mdio,
        NULL, bvstk_freertos_dcp2_policy_catalog, bvstk_freertos_dcp2_policy_changed,
        bvstk_freertos_dcp2_policy_save};
    adapter.i2c_supported = BVSTK_PL_HAS_I2C_CORE;
    adapter.mdio_supported = BVSTK_PL_HAS_MDIO_CORE;
    adapter.sync = sync_device;
    adapter.files = fs_dcp2_backend();
    adapter.cache = bvstk_runtime_cache_registry();
    adapter.head = observation_head; adapter.read = observation_read;
    adapter.streams = dcp2_stream_sim_instance();
}

static int dcp2_wait_io(int fd, bool writing, uint32_t started)
{
    fd_set fds;
    struct timeval timeout;
    uint32_t elapsed = (uint32_t)bvstk_platform_now_ms() - started;
    uint32_t remaining;
    if (elapsed >= 5000U) return -1;
    remaining = 5000U - elapsed;
    timeout.tv_sec = remaining / 1000U;
    timeout.tv_usec = (remaining % 1000U) * 1000U;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    return lwip_select(fd + 1, writing ? NULL : &fds,
                        writing ? &fds : NULL, NULL, &timeout) > 0 ? 0 : -1;
}

static int sock_read_exact(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    uint32_t started = (uint32_t)bvstk_platform_now_ms();
    while (got < len) {
        if (dcp2_wait_io(fd, false, started) < 0) return -1;
        int r = lwip_read(fd, p + got, (int)(len - got));
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

static int sock_write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;
    uint32_t started = (uint32_t)bvstk_platform_now_ms();
    while (sent < len) {
        int w = lwip_send(fd, p + sent, (int)(len - sent), MSG_DONTWAIT);
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (dcp2_wait_io(fd, true, started) < 0) return -1;
            continue;
        }
        if (w <= 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}

static int dcp2_wait_readable(int fd, long timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    int rc;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    rc = lwip_select(fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0) return rc;
    return FD_ISSET(fd, &rfds) ? 1 : 0;
}


static int send_frame(void *context, const void *data, size_t size)
{ return sock_write_all(*(int *)context, data, size); }
static void dcp2_handle_client(int fd)
{
    refresh_adapter();
    bvstk_dcp2_session_init(&session, &adapter, send_frame, &fd);
    for (;;) {
        refresh_adapter();
        if (bvstk_dcp2_session_poll(&session, bvstk_platform_now_ms())) break;
        int ready = dcp2_wait_readable(fd, 100);
        if (ready < 0) break;
        if (!ready) continue;
        if (sock_read_exact(fd, s_rx_buf, DCP2_HDR_LEN)) break;
        size_t size = bvstk_dcp2_read_be16(s_rx_buf + 6U);
        if (memcmp(s_rx_buf, "DCP2", 4U) || size < DCP2_MIN_PAYLOAD || size > DCP2_MAX_PAYLOAD) break;
        if (sock_read_exact(fd, s_rx_buf + 8U, size) ||
            bvstk_dcp2_session_frame(&session, s_rx_buf, size + 8U, bvstk_platform_now_ms())) break;
    }
    bvstk_dcp2_session_destroy(&session);
}

static void dcp2_server_thread(void *arg)
{
    int s;
    struct sockaddr_in addr;

    (void)arg;

    s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
		s_startup_done = 1;
        vTaskDelete(NULL);
        return;
    }

    {
        int opt = 1;
        (void)lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (lwip_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lwip_close(s);
		s_startup_done = 1;
        vTaskDelete(NULL);
        return;
    }

    if (lwip_listen(s, 1) < 0) {
        lwip_close(s);
		s_startup_done = 1;
        vTaskDelete(NULL);
        return;
    }
    xil_printf("DCP2: listening on %u\r\n", (unsigned)s_port);
	s_startup_done = 1;

    for (;;) {
        struct sockaddr_in remote;
        socklen_t rlen = sizeof(remote);
        int c = lwip_accept(s, (struct sockaddr *)&remote, &rlen);
        if (c < 0) continue;
        dcp2_handle_client(c);
        lwip_close(c);
    }
}

uint16_t dcp2_server_port(void)
{
    return s_port;
}

void start_dcp2_server(void)
{
    bool stream_ok = dcp2_stream_sim_init();
    sys_thread_t th;

    s_startup_done = 0;
    th = sys_thread_new("dcp2", dcp2_server_thread, 0, DCP2_THREAD_STACK, tskIDLE_PRIORITY + 1);
    if (!stream_ok) {
        xil_printf("DCP2: failed to start\r\n");
    }
    if (!th) {
		s_startup_done = 1;
    }
}
