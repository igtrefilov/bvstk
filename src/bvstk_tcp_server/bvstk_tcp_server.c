#include "bvstk_tcp_server.h"

int eth_socket;
int client_socket = -1;
u16_t echo_port = 8888;

__attribute__((weak)) void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd)
{
    (void)data_buffer;
    (void)data_length;
    (void)socket_fd;
}

static void run_client_session(int fd)
{
    char buffer[BUFFER_SIZE];
    char linebuf[256];
    size_t linelen = 0;
    int bytes_received;
    console_session_t session;
    console_session_init(&session);
    const char *prompt = "Zynq>";
    write_str(fd, prompt);
    utils_reset_close();
    for (;;) {
        bytes_received = lwip_read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        bool looks_text = false;
        for (int i = 0; i < bytes_received; ++i) {
            unsigned char c = (unsigned char)buffer[i];
            if (c == '\n' || c == '\r' || isprint(c)) looks_text = true;
            else { looks_text = false; break; }
        }
        if (looks_text) {
            for (int i = 0; i < bytes_received; ++i) {
                char c = buffer[i];
                if (c == '\r') continue;
                if (c == '\n') {
                    linebuf[linelen] = '\0';
                    if (linelen > 0) {
                        process_console_line(linebuf, fd, &session);
                        linelen = 0;
                    }
                    if (utils_should_close()) return;
                    write_str(fd, prompt);
                } else if (linelen + 1 < sizeof(linebuf)) {
                    linebuf[linelen++] = c;
                }
            }
        } else {
            process_received_data((uint8_t *)buffer, bytes_received, fd);
        }
    }
}

void start_tcp_server(void)
{
    sys_thread_new("tcp_server_thrd", tcp_server_thread, 0, THREAD_STACKSIZE, tskIDLE_PRIORITY + 1);
}

void tcp_server_thread(void *p)
{
    struct sockaddr_in address, remote;
    int size = sizeof(remote);
    eth_socket = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (eth_socket < 0) {
        vTaskDelete(NULL);
        return;
    }
    address.sin_family = AF_INET;
    address.sin_port = htons(echo_port);
    address.sin_addr.s_addr = INADDR_ANY;
    if (lwip_bind(eth_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        lwip_close(eth_socket);
        vTaskDelete(NULL);
        return;
    }
    lwip_listen(eth_socket, 1);
    for (;;) {
        client_socket = lwip_accept(eth_socket, (struct sockaddr *)&remote, (socklen_t *)&size);
        if (client_socket < 0) continue;
        run_client_session(client_socket);
        lwip_close(client_socket);
        client_socket = -1;
    }
}
