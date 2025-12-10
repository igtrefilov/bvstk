#include "bvstk_tcp_server.h"
#include <string.h>

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
    size_t cursor = 0;
    /* simple in-memory history */
    enum { HISTORY_LEN = 16 };
    char history[HISTORY_LEN][sizeof(linebuf)];
    int history_count = 0;   /* number of stored entries */
    int history_pos = -1;    /* -1 = current line, 0..count-1 = offset from newest */
    enum { ESC_NONE = 0, ESC_ESC, ESC_CSI, ESC_SS3, ESC_CSI_PARAM } esc_state = ESC_NONE;
    int iac_skip = 0; /* skip telnet IAC negotiation byte(s) */
    int bytes_received;
    console_session_t session;
    console_session_init(&session);
    /* Request character-at-a-time mode so arrows/TAB приходят сразу, но оставляем свою обработку строки. */
    {
        const unsigned char opts[] = {
            0xFF, 0xFB, 0x01, /* IAC WILL ECHO */
            0xFF, 0xFB, 0x03, /* IAC WILL SUPPRESS-GO-AHEAD */
            0xFF, 0xFD, 0x03, /* IAC DO SUPPRESS-GO-AHEAD */
            0xFF, 0xFE, 0x22  /* IAC DONT LINEMODE */
        };
        lwip_write(fd, opts, sizeof(opts));
    }
    console_print_prompt(fd, &session);
    utils_reset_close();
    for (;;) {
        bytes_received = lwip_read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        bool looks_text = false;
        for (int i = 0; i < bytes_received; ++i) {
            unsigned char c = (unsigned char)buffer[i];
            if (c == '\n' || c == '\r' || c == '\t' || c == 0x00 || isprint(c) || c == 0x1B || c == 0x08 || c == 0x7F || c == (char)0xFF) looks_text = true;
            else { looks_text = false; break; }
        }
        if (looks_text) {
            for (int i = 0; i < bytes_received; ++i) {
                char c = buffer[i];
                if (iac_skip > 0) { iac_skip--; continue; }
                if ((unsigned char)c == 0xFF) { iac_skip = 2; continue; } /* basic telnet IAC cmd+opt */
                if (esc_state != ESC_NONE) {
                    if (esc_state == ESC_ESC) {
                        if (c == '[') { esc_state = ESC_CSI; continue; }
                        if (c == 'O') { esc_state = ESC_SS3; continue; }
                        esc_state = ESC_NONE;
                        continue;
                    }
                    if (esc_state == ESC_CSI || esc_state == ESC_CSI_PARAM) {
                        if ((c >= '0' && c <= '9') || c == ';') { esc_state = ESC_CSI_PARAM; continue; }
                        if (c == 'C') { if (cursor < linelen) { cursor++; lwip_write(fd, "\x1b[C", 3); } }
                        else if (c == 'D') { if (cursor > 0) { cursor--; lwip_write(fd, "\x1b[D", 3); } }
                        else if (c == 'A') { /* arrow up: previous command */ 
                            if (history_count > 0 && history_pos + 1 < history_count) {
                                history_pos++;
                                strcpy(linebuf, history[history_count - 1 - history_pos]);
                                linelen = strlen(linebuf);
                                cursor = linelen;
                                lwip_write(fd, "\r\x1b[K", 3);
                                console_print_prompt(fd, &session);
                                if (linelen) lwip_write(fd, linebuf, linelen);
                            }
                        }
                        else if (c == 'B') { /* arrow down: newer command */ 
                            if (history_pos > 0) {
                                history_pos--;
                                strcpy(linebuf, history[history_count - 1 - history_pos]);
                                linelen = strlen(linebuf);
                            } else if (history_pos == 0) {
                                history_pos = -1;
                                linelen = 0;
                                linebuf[0] = '\0';
                            } else { /* already at current line */ }
                            cursor = linelen;
                            lwip_write(fd, "\r\x1b[K", 3);
                            console_print_prompt(fd, &session);
                            if (linelen) lwip_write(fd, linebuf, linelen);
                        }
                        esc_state = ESC_NONE;
                        continue;
                    }
                    if (esc_state == ESC_SS3) {
                        if (c == 'C') { if (cursor < linelen) { cursor++; lwip_write(fd, "\x1b[C", 3); } }
                        else if (c == 'D') { if (cursor > 0) { cursor--; lwip_write(fd, "\x1b[D", 3); } }
                        else if (c == 'A') { /* arrow up */ 
                            if (history_count > 0 && history_pos + 1 < history_count) {
                                history_pos++;
                                strcpy(linebuf, history[history_count - 1 - history_pos]);
                                linelen = strlen(linebuf);
                                cursor = linelen;
                                lwip_write(fd, "\r\x1b[K", 3);
                                console_print_prompt(fd, &session);
                                if (linelen) lwip_write(fd, linebuf, linelen);
                            }
                        }
                        else if (c == 'B') { /* arrow down */ 
                            if (history_pos > 0) {
                                history_pos--;
                                strcpy(linebuf, history[history_count - 1 - history_pos]);
                                linelen = strlen(linebuf);
                            } else if (history_pos == 0) {
                                history_pos = -1;
                                linelen = 0;
                                linebuf[0] = '\0';
                            } else { /* already at current line */ }
                            cursor = linelen;
                            lwip_write(fd, "\r\x1b[K", 3);
                            console_print_prompt(fd, &session);
                            if (linelen) lwip_write(fd, linebuf, linelen);
                        }
                        esc_state = ESC_NONE;
                        continue;
                    }
                }
                if (c == 0x1B) { esc_state = ESC_ESC; continue; }
                /* Convert CR or CRLF to newline so Enter works in char mode. */
                if (c == '\r') {
                    if ((i + 1) < bytes_received && buffer[i + 1] == '\n') { /* swallow LF part of CRLF */
                        i++;
                    } else if ((i + 1) < bytes_received && buffer[i + 1] == '\0') { /* swallow NUL part of CR-NUL */
                        i++;
                    }
                    c = '\n';
                }
                if (c == '\0') continue; /* ignore stray NUL */
                if (c == '\n') {
                    /* echo newline to client before executing command */
                    lwip_write(fd, "\r\n", 2);
                    linebuf[linelen] = '\0';
                    if (linelen > 0) {
                        /* save to history (no duplicates in a row) */
                        if (history_count == 0 || strcmp(history[history_count - 1], linebuf) != 0) {
                            if (history_count < HISTORY_LEN) {
                                strcpy(history[history_count], linebuf);
                                history_count++;
                            } else {
                                /* shift left and append */
                                for (int h = 1; h < HISTORY_LEN; ++h) strcpy(history[h - 1], history[h]);
                                strcpy(history[HISTORY_LEN - 1], linebuf);
                            }
                        }
                        history_pos = -1;
                        process_console_line(linebuf, fd, &session);
                        linelen = 0;
                        cursor = 0;
                    }
                    if (utils_should_close()) return;
                    console_print_prompt(fd, &session);
                    esc_state = ESC_NONE;
                } else if (c == 0x08 || c == 0x7F) { /* backspace */
                    if (cursor > 0) {
                        cursor--;
                        size_t tail = linelen - cursor - 1;
                        memmove(linebuf + cursor, linebuf + cursor + 1, tail);
                        linelen--;
                        lwip_write(fd, "\b", 1);
                        if (tail > 0) lwip_write(fd, linebuf + cursor, tail);
                        lwip_write(fd, " ", 1);
                        for (size_t k = 0; k < tail + 1; ++k) lwip_write(fd, "\x1b[D", 3);
                    }
                } else if (c == '\t') {
                    xil_printf("tab\r\n");
                    /* do not insert TAB into input line or echo it */
                    continue;
                } else if (isprint((unsigned char)c)) {
                    if (linelen + 1 >= sizeof(linebuf)) continue;
                    size_t tail = linelen - cursor;
                    memmove(linebuf + cursor + 1, linebuf + cursor, tail);
                    linebuf[cursor] = c;
                    linelen++;
                    cursor++;
                    lwip_write(fd, &c, 1);
                    if (tail > 0) {
                        lwip_write(fd, linebuf + cursor, tail);
                        for (size_t k = 0; k < tail; ++k) lwip_write(fd, "\x1b[D", 3);
                    }
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
