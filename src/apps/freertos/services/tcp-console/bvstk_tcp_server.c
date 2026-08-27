#include "apps/freertos/services/tcp-console/bvstk_tcp_server.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "apps/freertos/config/config_store.h"
#include "apps/freertos/storage/sd/sd_card.h"
#include "shared/cli/bvstk_i2c_completion.h"
#include "shared/cli/bvstk_line_editor.h"
#include "xstatus.h"

#ifndef TCP_CONSOLE_TAB_DEBUG
#define TCP_CONSOLE_TAB_DEBUG 0
#endif

int eth_socket;
int client_socket = -1;
u16_t echo_port = 8888;

__attribute__((weak)) void process_received_data(uint8_t *data_buffer, int data_length, int socket_fd)
{
    (void)data_buffer;
    (void)data_length;
    (void)socket_fd;
}

static char s_dir_candidates[16][FS_NAME_MAX];

static char s_buffer[BUFFER_SIZE];

enum { CONSOLE_PATH_MAX = 128 };

static const char *const s_commands[] = {
    "fs", "tar", "ip", "smi", "mem", "i2c",
    "pwd", "ls", "cd", "mkdir", "touch", "cat", "rm", "cp", "mv",
    "help", "reboot", "quit", "exit"
};

typedef struct {
    int fd;
    console_session_t *session;
} tcp_editor_context_t;

typedef struct {
    const char *s;
    size_t len;
} tok_view_t;

static size_t split_tokens_upto(const char *line, size_t upto, tok_view_t *out, size_t out_max)
{
    if (!out || out_max == 0) return 0;
    size_t n = 0;
    size_t p = 0;
    while (p < upto) {
        while (p < upto && (line[p] == ' ' || line[p] == '\t')) p++;
        if (p >= upto) break;
        size_t start = p;
        while (p < upto && line[p] != ' ' && line[p] != '\t') p++;
        if (n < out_max) {
            out[n].s = line + start;
            out[n].len = p - start;
            n++;
        } else {
            break;
        }
    }
    return n;
}

static void tok_to_cstr(tok_view_t t, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!t.s || t.len == 0) return;
    size_t n = (t.len < out_sz - 1) ? t.len : (out_sz - 1);
    memcpy(out, t.s, n);
    out[n] = '\0';
}

static int add_match_ci(const char *prefix, const char *cand, char out[][FS_NAME_MAX], int out_max, int *inout_total)
{
    if (!prefix || !cand || !out || out_max <= 0 || !inout_total) return 0;
    size_t pl = strlen(prefix);
    if (pl > 0 && strncasecmp(cand, prefix, pl) != 0) return 0;
    (*inout_total)++;
    int stored = (*inout_total <= out_max) ? 1 : 0;
    if (stored) {
        int idx = *inout_total - 1;
        strncpy(out[idx], cand, FS_NAME_MAX - 1);
        out[idx][FS_NAME_MAX - 1] = '\0';
    }
    return 1;
}

static int complete_words(const char *prefix, const char *const *words, size_t words_n, char out[][FS_NAME_MAX], int out_max, int *out_total)
{
    if (out_total) *out_total = 0;
    if (!out_total || !words || words_n == 0) return 0;
    int total = 0;
    for (size_t i = 0; i < words_n; ++i) {
        (void)add_match_ci(prefix, words[i], out, out_max, &total);
    }
    *out_total = total;
    return total;
}

static int complete_smi_selector(const char *prefix, char out[][FS_NAME_MAX], int out_max, int *out_total)
{
    if (out_total) *out_total = 0;
    if (!out_total) return 0;

    int total = 0;
    static const char *const head_words[] = { "list", "r", "w", "-h", "--help" };
    (void)complete_words(prefix, head_words, sizeof(head_words) / sizeof(head_words[0]), out, out_max, &total);

    if (!config_store_is_ready()) { *out_total = total; return total; }
    const smi_phy_config_t *cfgs = config_store_get_smi_devices();
    size_t n = config_store_get_smi_device_count();
    if (!cfgs || n == 0) { *out_total = total; return total; }

    if (prefix && prefix[0] == '@') {
        for (size_t i = 0; i < n; ++i) {
            char tmp[FS_NAME_MAX];
            snprintf(tmp, sizeof(tmp), "@%u", (unsigned)(cfgs[i].phy_addr & 0x1Fu));
            (void)add_match_ci(prefix, tmp, out, out_max, &total);
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            if (!cfgs[i].name[0]) continue;
            (void)add_match_ci(prefix, cfgs[i].name, out, out_max, &total);
        }
    }

    *out_total = total;
    return total;
}

static size_t copy_completion_candidates(
    bvstk_line_editor_completion_t *result,
    const char candidates[][FS_NAME_MAX],
    size_t candidate_count)
{
    size_t index;
    size_t stored_count = candidate_count;

    if (result == NULL || candidates == NULL) {
        return 0U;
    }
    if (stored_count > BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES) {
        stored_count = BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES;
    }
    for (index = 0U; index < stored_count; ++index) {
        strncpy(result->candidates[index],
                candidates[index],
                BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U);
        result->candidates[index]
                [BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U] = '\0';
    }
    result->match_count = stored_count;
    return stored_count;
}

static int tcp_editor_complete(void *context,
                               const char *line,
                               size_t line_length,
                               size_t cursor,
                               bvstk_line_editor_completion_t *result)
{
    tcp_editor_context_t *editor_context = (tcp_editor_context_t *)context;
    const console_session_t *session;
    size_t start;
    size_t prefix_length;
    tok_view_t tokens_before[4];
    size_t token_count;
    char prefix[FS_NAME_MAX];

    if (editor_context == NULL || line == NULL || result == NULL ||
        editor_context->session == NULL) {
        return 0;
    }
    session = editor_context->session;
    memset(result, 0, sizeof(*result));
    if (cursor > line_length) {
        cursor = line_length;
    }
    start = cursor;
    while (start > 0U && line[start - 1U] != ' ' && line[start - 1U] != '\t') {
        --start;
    }
    prefix_length = cursor - start;
    if (prefix_length >= sizeof(prefix)) {
        return 0;
    }
    memcpy(prefix, line + start, prefix_length);
    prefix[prefix_length] = '\0';
    result->token_start = start;
    result->token_prefix_length = prefix_length;
    token_count = split_tokens_upto(line,
                                   start,
                                   tokens_before,
                                   sizeof(tokens_before) / sizeof(tokens_before[0]));

    if (token_count == 0U) {
        size_t command_count = 0U;
        size_t command_index;

        for (command_index = 0U;
             command_index < sizeof(s_commands) / sizeof(s_commands[0]);
             ++command_index) {
            if (strncasecmp(s_commands[command_index],
                            prefix,
                            prefix_length) != 0 ||
                command_count >= BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES) {
                continue;
            }
            strncpy(result->candidates[command_count],
                    s_commands[command_index],
                    BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U);
            result->candidates[command_count]
                    [BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U] = '\0';
            ++command_count;
        }
        result->match_count = command_count;
        return command_count != 0U;
    }

    {
        char prefix_part_token[FS_NAME_MAX];
        char cmd0[16] = {0};
        char tok1[FS_NAME_MAX] = {0};
        char tok2[FS_NAME_MAX] = {0};
        char tok3[FS_NAME_MAX] = {0};
        size_t token_index = token_count;
        int total = 0;
        int handled = 0;

        if (prefix_length >= sizeof(prefix_part_token)) {
            return 0;
        }
        memcpy(prefix_part_token, line + start, prefix_length);
        prefix_part_token[prefix_length] = '\0';
        tok_to_cstr(tokens_before[0], cmd0, sizeof(cmd0));
        if (token_count > 1U) {
            tok_to_cstr(tokens_before[1], tok1, sizeof(tok1));
        }
        if (token_count > 2U) {
            tok_to_cstr(tokens_before[2], tok2, sizeof(tok2));
        }
        if (token_count > 3U) {
            tok_to_cstr(tokens_before[3], tok3, sizeof(tok3));
        }

        if (strcasecmp(cmd0, "tar") == 0 && token_index == 1U) {
            static const char *const tar_words[] = {
                "c", "x", "t", "-h", "--help"
            };
            handled = 1;
            (void)complete_words(prefix_part_token,
                                  tar_words,
                                  sizeof(tar_words) / sizeof(tar_words[0]),
                                  s_dir_candidates,
                                  16,
                                  &total);
        } else if (strcasecmp(cmd0, "ip") == 0) {
            handled = 1;
            if (token_index == 1U) {
                static const char *const ip_words[] = {"addr", "-h", "--help"};
                (void)complete_words(prefix_part_token,
                                      ip_words,
                                      sizeof(ip_words) / sizeof(ip_words[0]),
                                      s_dir_candidates,
                                      16,
                                      &total);
            } else if (token_index == 2U && strcasecmp(tok1, "addr") == 0) {
                static const char *const ip_words[] = {"show", "set"};
                (void)complete_words(prefix_part_token,
                                      ip_words,
                                      sizeof(ip_words) / sizeof(ip_words[0]),
                                      s_dir_candidates,
                                      16,
                                      &total);
            } else {
                handled = 0;
            }
        } else if (strcasecmp(cmd0, "mem") == 0 && token_index == 1U) {
            static const char *const mem_words[] = {"r", "w", "-h", "--help"};
            handled = 1;
            (void)complete_words(prefix_part_token,
                                  mem_words,
                                  sizeof(mem_words) / sizeof(mem_words[0]),
                                  s_dir_candidates,
                                  16,
                                  &total);
        } else if (strcasecmp(cmd0, "i2c") == 0) {
            bvstk_i2c_completion_device_t devices[I2C_CFG_MAX_DEVICES];
            bvstk_i2c_completion_result_t i2c_result;
            const i2c_device_config_t *configs = NULL;
            size_t device_count = 0U;
            size_t device_index;
            size_t candidate_index;

            memset(devices, 0, sizeof(devices));
            if (config_store_is_ready()) {
                configs = config_store_get_i2c_devices();
                device_count = config_store_get_i2c_device_count();
                if (configs == NULL) {
                    device_count = 0U;
                }
                if (device_count > I2C_CFG_MAX_DEVICES) {
                    device_count = I2C_CFG_MAX_DEVICES;
                }
                for (device_index = 0U;
                     device_index < device_count;
                     ++device_index) {
                    strncpy(devices[device_index].name,
                            configs[device_index].name,
                            sizeof(devices[device_index].name) - 1U);
                    devices[device_index].name
                            [sizeof(devices[device_index].name) - 1U] = '\0';
                    devices[device_index].addr_7b =
                        configs[device_index].addr_7b;
                }
            }
            if (!bvstk_i2c_complete_line(line,
                                         line_length,
                                         cursor,
                                         devices,
                                         device_count,
                                         &i2c_result)) {
                return 0;
            }
            result->token_start = i2c_result.token_start;
            result->token_prefix_length = i2c_result.token_prefix_length;
            result->match_count = i2c_result.match_count;
            if (result->match_count > BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES) {
                result->match_count = BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES;
            }
            for (candidate_index = 0U;
                 candidate_index < result->match_count;
                 ++candidate_index) {
                strncpy(result->candidates[candidate_index],
                        i2c_result.candidates[candidate_index],
                        BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U);
                result->candidates[candidate_index]
                        [BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U] = '\0';
            }
            return result->match_count != 0U;
        } else if (strcasecmp(cmd0, "smi") == 0) {
            handled = 1;
            if (token_index == 1U) {
                (void)complete_smi_selector(prefix_part_token,
                                             s_dir_candidates,
                                             16,
                                             &total);
            } else if (token_index == 2U &&
                       strcasecmp(tok1, "list") != 0 &&
                       strcasecmp(tok1, "r") != 0 &&
                       strcasecmp(tok1, "w") != 0) {
                static const char *const smi_words[] = {
                    "info", "r", "w", "phy", "addr", "address",
                    "rules", "policy", "allow", "deny", "clear",
                    "autopoll", "settings", "save"
                };
                (void)complete_words(prefix_part_token,
                                      smi_words,
                                      sizeof(smi_words) / sizeof(smi_words[0]),
                                      s_dir_candidates,
                                      16,
                                      &total);
            } else if (token_index == 3U &&
                       strcasecmp(tok2, "policy") == 0) {
                static const char *const policy_words[] = {
                    "whitelist", "blacklist"
                };
                (void)complete_words(prefix_part_token,
                                      policy_words,
                                      sizeof(policy_words) / sizeof(policy_words[0]),
                                      s_dir_candidates,
                                      16,
                                      &total);
            } else if (token_index == 3U &&
                       strcasecmp(tok2, "autopoll") == 0) {
                static const char *const autopoll_words[] = {
                    "on", "off", "reg_delay", "cycle_delay", "regs"
                };
                (void)complete_words(prefix_part_token,
                                      autopoll_words,
                                      sizeof(autopoll_words) /
                                          sizeof(autopoll_words[0]),
                                      s_dir_candidates,
                                      16,
                                      &total);
            } else if (token_index == 3U &&
                       strcasecmp(tok2, "settings") == 0) {
                static const char *const settings_words[] = {"clear"};
                (void)complete_words(prefix_part_token,
                                      settings_words,
                                      sizeof(settings_words) /
                                          sizeof(settings_words[0]),
                                      s_dir_candidates,
                                      16,
                                      &total);
            } else {
                handled = 0;
            }
        }
        if (handled && total > 0) {
            (void)copy_completion_candidates(result,
                                              s_dir_candidates,
                                              (size_t)total);
            return result->match_count != 0U;
        }

        {
            const fs_shared_ctx_t *fs_context =
                console_session_get_fs(session);
            const char *root;
            const char *cwd;
            char directory_part[CONSOLE_PATH_MAX];
            char prefix_part[CONSOLE_PATH_MAX];
            char full_directory[CONSOLE_PATH_MAX];
            const char *last_slash;
            int status;

            if (fs_context == NULL) {
                return 0;
            }
            root = console_session_get_root(session);
            cwd = session->cwd[0] != '\0' ? session->cwd : root;
            if (prefix_length >= sizeof(prefix_part)) {
                return 0;
            }
            memcpy(prefix_part, line + start, prefix_length);
            prefix_part[prefix_length] = '\0';
            last_slash = strrchr(prefix_part, '/');
            if (last_slash != NULL) {
                size_t directory_length = (size_t)(last_slash - prefix_part);

                if (directory_length == 0U) {
                    (void)snprintf(directory_part,
                                   sizeof(directory_part),
                                   "%s",
                                   root);
                } else {
                    (void)snprintf(directory_part,
                                   sizeof(directory_part),
                                   "%.*s",
                                   (int)directory_length,
                                   prefix_part);
                }
                if ((size_t)(last_slash - prefix_part) + 1U >=
                    sizeof(prefix_part)) {
                    return 0;
                }
                memmove(prefix_part,
                        last_slash + 1U,
                        strlen(last_slash + 1U) + 1U);
            } else {
                (void)snprintf(directory_part,
                               sizeof(directory_part),
                               "%s",
                               cwd);
            }
            if (directory_part[0] == '/' && directory_part[1] != '\0') {
                (void)snprintf(full_directory,
                               sizeof(full_directory),
                               "%s%s",
                               root,
                               directory_part + 1);
            } else if (strchr(directory_part, ':') != NULL) {
                (void)snprintf(full_directory,
                               sizeof(full_directory),
                               "%s",
                               directory_part);
            } else {
                (void)snprintf(full_directory,
                               sizeof(full_directory),
                               "%s%s",
                               directory_part,
                               directory_part[0] != '\0' &&
                                       directory_part[strlen(directory_part) - 1U] !=
                                           '/' ? "/" : "");
            }
            status = fs_shared_fs_complete(fs_context,
                                           full_directory,
                                           prefix_part,
                                           s_dir_candidates,
                                           16,
                                           &total);
            if (status != XST_SUCCESS || total <= 0) {
                return 0;
            }
            (void)copy_completion_candidates(result,
                                              s_dir_candidates,
                                              (size_t)total);
            return result->match_count != 0U;
        }
    }
}

static int tcp_editor_write(void *context, const void *data, size_t length)
{
    tcp_editor_context_t *editor_context = (tcp_editor_context_t *)context;
    const unsigned char *cursor = (const unsigned char *)data;

    if (editor_context == NULL || data == NULL) {
        return -1;
    }
    while (length != 0U) {
        int written = lwip_write(editor_context->fd, cursor, (int)length);

        if (written <= 0) {
            return -1;
        }
        cursor += written;
        length -= (size_t)written;
    }
    return 0;
}

static void tcp_editor_prompt(void *context)
{
    tcp_editor_context_t *editor_context = (tcp_editor_context_t *)context;

    if (editor_context != NULL && editor_context->session != NULL) {
        console_print_prompt(editor_context->fd, editor_context->session);
    }
}

static int tcp_editor_submit(void *context, const char *line, size_t length)
{
    tcp_editor_context_t *editor_context = (tcp_editor_context_t *)context;

    (void)length;
    if (editor_context == NULL || editor_context->session == NULL ||
        line == NULL) {
        return BVSTK_LINE_EDITOR_SUBMIT_STOP;
    }
    process_console_line(line,
                         editor_context->fd,
                         editor_context->session);
    return utils_should_close() ? BVSTK_LINE_EDITOR_SUBMIT_STOP
                                 : BVSTK_LINE_EDITOR_SUBMIT_PROMPT;
}

static void run_client_session(int fd)
{
    static bvstk_line_editor_t editor;
    bvstk_line_editor_config_t editor_config;
    tcp_editor_context_t editor_context;
    console_session_t session;
    int bytes_received;
    int telnet_subnegotiation = 0;

    console_session_init(&session);
    editor_context.fd = fd;
    editor_context.session = &session;
    memset(&editor_config, 0, sizeof(editor_config));
    editor_config.context = &editor_context;
    editor_config.write = tcp_editor_write;
    editor_config.prompt = tcp_editor_prompt;
    editor_config.submit = tcp_editor_submit;
    editor_config.complete = tcp_editor_complete;
    editor_config.eof_on_empty = 0;
    bvstk_line_editor_init(&editor, &editor_config);
    {
        const unsigned char opts[] = {
            0xFF, 0xFB, 0x01,
            0xFF, 0xFB, 0x03,
            0xFF, 0xFD, 0x03,
            0xFF, 0xFE, 0x22
        };
        (void)lwip_write(fd, opts, sizeof(opts));
    }
    console_print_banner(fd);
    console_print_prompt(fd, &session);
    utils_reset_close();
    for (;;) {
        int index;

        bytes_received = lwip_read(fd, s_buffer, sizeof(s_buffer));
        if (bytes_received <= 0) {
            break;
        }
        for (index = 0; index < bytes_received; ++index) {
            unsigned char character = (unsigned char)s_buffer[index];

            if (telnet_subnegotiation) {
                if (character == 0xFFU && index + 1 < bytes_received &&
                    (unsigned char)s_buffer[index + 1] == 0xF0U) {
                    ++index;
                    telnet_subnegotiation = 0;
                }
                continue;
            }
            if (character == 0xFFU) {
                unsigned char command;
                unsigned char option;
                unsigned char reply[3] = {0xFFU, 0U, 0U};
                int send_reply = 0;

                if (index + 2 >= bytes_received) {
                    break;
                }
                command = (unsigned char)s_buffer[index + 1];
                option = (unsigned char)s_buffer[index + 2];
                index += 2;
                if (command == 0xFAU) {
                    telnet_subnegotiation = 1;
                } else if (command == 0xFDU) {
                    reply[1] = option == 0x01U || option == 0x03U
                                   ? 0xFBU : 0xFCU;
                    reply[2] = option;
                    send_reply = 1;
                } else if (command == 0xFBU) {
                    reply[1] = option == 0x01U || option == 0x03U
                                   ? 0xFDU : 0xFEU;
                    reply[2] = option;
                    send_reply = 1;
                }
                if (send_reply) {
                    (void)lwip_write(fd, reply, sizeof(reply));
                }
                continue;
            }
            if (character == '\r') {
                if (index + 1 < bytes_received &&
                    (unsigned char)s_buffer[index + 1] == '\n') {
                    ++index;
                }
                character = '\n';
            }
            if (bvstk_line_editor_handle_byte(&editor, character) != 0) {
                return;
            }
        }
    }
}

void start_tcp_server(void)
{
    sys_thread_new("tcp_server_thrd", tcp_server_thread, 0, TCP_THREAD_STACKSIZE, tskIDLE_PRIORITY + 1);
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
