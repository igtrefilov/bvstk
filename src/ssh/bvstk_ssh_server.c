#include "bvstk_ssh_server.h"

#ifdef BVSTK_SSH_ENABLE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"

#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssh/ssh.h>

#include "../bvstk_tcp_server/utils/console_common.h"
#include "../bvstk_tcp_server/utils/console_stream.h"
#include "../bvstk_tcp_server/utils/utils.h"
#include "bvstk_ssh_generated.h"

#ifndef SSH_THREAD_STACKSIZE
#define SSH_THREAD_STACKSIZE 12288
#endif

#define SSH_CONSOLE_FD (-1)
#define SSH_LINE_SIZE 256
#define SSH_RX_SIZE 1024

typedef struct {
    WOLFSSH *ssh;
    WOLFSSH_CHANNEL *channel;
    word32 channel_id;
    console_session_t console;
    char line[SSH_LINE_SIZE];
    size_t line_len;
    int shell_requested;
    int exec_requested;
    int exec_done;
    int banner_sent;
    int close_requested;
    int last_was_cr;
    char exec_command[SSH_LINE_SIZE];
} bvstk_ssh_session_t;

static WOLFSSH_CTX *s_ssh_ctx;

extern int bvstk_ssh_seed(OS_Seed *os, byte *output, word32 size);

static int ssh_user_auth(byte auth_type, WS_UserAuthData *auth_data, void *ctx)
{
    (void)ctx;
    if (!auth_data || auth_type != WOLFSSH_USERAUTH_PASSWORD) {
        return WOLFSSH_USERAUTH_FAILURE;
    }
    if (auth_data->usernameSz != (word32)strlen(BVSTK_SSH_USER) ||
        memcmp(auth_data->username, BVSTK_SSH_USER, auth_data->usernameSz) != 0) {
        return WOLFSSH_USERAUTH_FAILURE;
    }

    byte digest[WC_SHA256_DIGEST_SIZE];
    if (wc_Sha256Hash(auth_data->sf.password.password,
                      auth_data->sf.password.passwordSz, digest) != 0) {
        return WOLFSSH_USERAUTH_FAILURE;
    }
    return (sizeof(digest) == BVSTK_SSH_PASSWORD_SHA256_SIZE &&
            memcmp(digest, bvstk_ssh_password_sha256, sizeof(digest)) == 0) ?
           WOLFSSH_USERAUTH_SUCCESS : WOLFSSH_USERAUTH_FAILURE;
}

static int ssh_channel_store(bvstk_ssh_session_t *session,
                              WOLFSSH_CHANNEL *channel)
{
    if (!session || !channel) return WS_BAD_ARGUMENT;
    session->channel = channel;
    if (wolfSSH_ChannelGetId(channel, &session->channel_id,
                             WS_CHANNEL_ID_SELF) != WS_SUCCESS) {
        return WS_INVALID_CHANID;
    }
    return WS_SUCCESS;
}

static int ssh_shell_request(WOLFSSH_CHANNEL *channel, void *ctx)
{
    bvstk_ssh_session_t *session = (bvstk_ssh_session_t *)ctx;
    int ret = ssh_channel_store(session, channel);
    if (ret == WS_SUCCESS) session->shell_requested = 1;
    return ret;
}

static int ssh_exec_request(WOLFSSH_CHANNEL *channel, void *ctx)
{
    bvstk_ssh_session_t *session = (bvstk_ssh_session_t *)ctx;
    int ret = ssh_channel_store(session, channel);
    const char *command = wolfSSH_ChannelGetSessionCommand(channel);
    if (ret != WS_SUCCESS || !command) return WS_BAD_ARGUMENT;
    strncpy(session->exec_command, command, sizeof(session->exec_command) - 1);
    session->exec_command[sizeof(session->exec_command) - 1] = '\0';
    session->exec_requested = 1;
    return WS_SUCCESS;
}

static int ssh_write(void *ctx, const void *data, size_t len)
{
    bvstk_ssh_session_t *session = (bvstk_ssh_session_t *)ctx;
    if (!session || !session->channel || (!data && len != 0)) return -1;
    const byte *ptr = (const byte *)data;
    size_t sent = 0;
    while (sent < len) {
        word32 chunk = (word32)((len - sent > 1024) ? 1024 : (len - sent));
        int ret = wolfSSH_ChannelSend(session->channel, ptr + sent, chunk);
        if (ret <= 0) return -1;
        sent += (size_t)ret;
    }
    return (int)sent;
}

static void ssh_send_text(bvstk_ssh_session_t *session, const char *text)
{
    if (session && text) (void)ssh_write(session, text, strlen(text));
}

static void ssh_process_line(bvstk_ssh_session_t *session)
{
    if (!session) return;
    session->line[session->line_len] = '\0';
    if (session->line_len != 0) {
        process_console_line(session->line, SSH_CONSOLE_FD, &session->console);
    }
    session->line_len = 0;
    if (utils_should_close()) session->close_requested = 1;
}

static void ssh_process_input(bvstk_ssh_session_t *session,
                              const byte *data, size_t len)
{
    if (!session || !data) return;
    for (size_t i = 0; i < len && !session->close_requested; ++i) {
        byte c = data[i];
        if (c == '\r' || c == '\n') {
            if (c == '\n' && session->last_was_cr) {
                session->last_was_cr = 0;
                continue;
            }
            ssh_send_text(session, "\r\n");
            ssh_process_line(session);
            session->last_was_cr = (c == '\r');
            if (!session->close_requested) console_print_prompt(SSH_CONSOLE_FD, &session->console);
        } else if (c == '\b' || c == 0x7f) {
            session->last_was_cr = 0;
            if (session->line_len > 0) {
                --session->line_len;
                ssh_send_text(session, "\b \b");
            }
        } else if (c >= 0x20 && c != 0x7f) {
            session->last_was_cr = 0;
            if (session->line_len + 1 < sizeof(session->line)) {
                session->line[session->line_len++] = (char)c;
                (void)ssh_write(session, &c, 1);
            }
        }
    }
}

static void ssh_finish_session(bvstk_ssh_session_t *session, int status)
{
    if (!session || !session->ssh) return;
    (void)wolfSSH_stream_exit(session->ssh, status);
}

static void ssh_service_client(int fd)
{
    bvstk_ssh_session_t session;
    memset(&session, 0, sizeof(session));
    console_session_init(&session.console);
    utils_reset_close();

    WOLFSSH *ssh = wolfSSH_new(s_ssh_ctx);
    if (!ssh) {
        xil_printf("SSH: client context allocation failed\r\n");
        return;
    }
    session.ssh = ssh;
    wolfSSH_set_fd(ssh, fd);
    wolfSSH_SetUserAuthCtx(ssh, NULL);
    wolfSSH_SetChannelReqCtx(ssh, &session);

    xil_printf("SSH: accepting client\r\n");
    int accept_ret = wolfSSH_accept(ssh);
    if (accept_ret != WS_SUCCESS) {
        xil_printf("SSH: accept failed ret=%d err=%d\r\n",
                   accept_ret, wolfSSH_get_error(ssh));
        wolfSSH_free(ssh);
        return;
    }

    /* wolfSSH_worker() is a non-blocking state machine.  Keep the SSH
     * handshake blocking, then let the worker flush exec output and observe
     * the peer close without waiting forever for another input packet. */
    unsigned long nonblocking = 1;
    if (lwip_ioctl(fd, FIONBIO, &nonblocking) != 0) {
        xil_printf("SSH: cannot enable nonblocking client socket\r\n");
        wolfSSH_free(ssh);
        return;
    }

    if (console_stream_register(SSH_CONSOLE_FD, ssh_write, &session) != 0) {
        wolfSSH_free(ssh);
        return;
    }

    for (;;) {
        word32 last_channel = 0;
        int ret = wolfSSH_worker(ssh, &last_channel);
        int error = wolfSSH_get_error(ssh);

        if (session.exec_requested && !session.exec_done) {
            session.exec_done = 1;
            process_console_line(session.exec_command, SSH_CONSOLE_FD, &session.console);
            ssh_finish_session(&session, utils_should_close() ? 1 : 0);
        }

        /* wolfSSH_ChannelSend() queues the command output before the exit
         * packets. Give wolfSSH_worker() a chance to flush that output. */
        if (session.exec_done) {
            if (ret == WS_EOF || error == WS_EOF || ret == WS_CHANNEL_CLOSED ||
                error == WS_CHANNEL_CLOSED) {
                break;
            }
            if (ret < 0 && error != WS_WANT_READ && error != WS_WANT_WRITE &&
                error != WS_REKEYING) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (session.shell_requested && !session.banner_sent) {
            session.banner_sent = 1;
            console_print_banner(SSH_CONSOLE_FD);
            console_print_prompt(SSH_CONSOLE_FD, &session.console);
        }

        if (ret == WS_CHAN_RXD || error == WS_CHAN_RXD) {
            if (session.channel && last_channel == session.channel_id) {
                byte rx[SSH_RX_SIZE];
                int count = wolfSSH_ChannelIdRead(ssh, session.channel_id,
                                                  rx, sizeof(rx));
                if (count <= 0) break;
                ssh_process_input(&session, rx, (size_t)count);
                if (session.close_requested) {
                    ssh_finish_session(&session, 0);
                    break;
                }
            }
            continue;
        }

        if (ret < 0 && error != WS_WANT_READ && error != WS_WANT_WRITE &&
            error != WS_REKEYING) {
            break;
        }
        if (ret == WS_EOF || error == WS_EOF || ret == WS_CHANNEL_CLOSED ||
            error == WS_CHANNEL_CLOSED) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    console_stream_unregister(SSH_CONSOLE_FD);
    wolfSSH_free(ssh);
}

static void ssh_server_thread(void *arg)
{
    (void)arg;
    int listen_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        xil_printf("SSH: socket failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    (void)lwip_setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(BVSTK_SSH_PORT);
    address.sin_addr.s_addr = INADDR_ANY;
    if (lwip_bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        lwip_listen(listen_fd, 1) < 0) {
        xil_printf("SSH: bind/listen failed\r\n");
        lwip_close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    xil_printf("SSH: listening on port %u\r\n", (unsigned)BVSTK_SSH_PORT);
    for (;;) {
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        int client_fd = lwip_accept(listen_fd, (struct sockaddr *)&remote, &remote_len);
        if (client_fd < 0) continue;
        xil_printf("SSH: client connected fd=%d\r\n", client_fd);
        ssh_service_client(client_fd);
        xil_printf("SSH: client closed\r\n");
        lwip_close(client_fd);
    }
}

void start_ssh_server(void)
{
    if (wc_SetSeed_Cb(bvstk_ssh_seed) != 0) {
        xil_printf("SSH: RNG callback setup failed\r\n");
        return;
    }
    if (wolfSSH_Init() != WS_SUCCESS) {
        xil_printf("SSH: library init failed\r\n");
        return;
    }
    s_ssh_ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
    if (!s_ssh_ctx) {
        xil_printf("SSH: context allocation failed\r\n");
        return;
    }
    if (wolfSSH_CTX_UsePrivateKey_buffer(s_ssh_ctx, bvstk_ssh_host_key,
                                         BVSTK_SSH_HOST_KEY_SIZE,
                                         WOLFSSH_FORMAT_PEM) != WS_SUCCESS) {
        xil_printf("SSH: host key rejected\r\n");
        wolfSSH_CTX_free(s_ssh_ctx);
        s_ssh_ctx = NULL;
        return;
    }
    if (wolfSSH_CTX_SetAlgoListKex(s_ssh_ctx,
                                   "ecdh-sha2-nistp256") != WS_SUCCESS) {
        xil_printf("SSH: KEX configuration failed\r\n");
        wolfSSH_CTX_free(s_ssh_ctx);
        s_ssh_ctx = NULL;
        return;
    }
    wolfSSH_SetUserAuth(s_ssh_ctx, ssh_user_auth);
    wolfSSH_CTX_SetChannelReqShellCb(s_ssh_ctx, ssh_shell_request);
    wolfSSH_CTX_SetChannelReqExecCb(s_ssh_ctx, ssh_exec_request);
    sys_thread_new("ssh_server_thrd", ssh_server_thread, NULL,
                   SSH_THREAD_STACKSIZE, tskIDLE_PRIORITY + 1);
}

#else

void start_ssh_server(void)
{
}

#endif
