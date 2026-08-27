#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "apps/neutrino/i2c/bvstk_i2c_client.h"
#include "shared/cli/bvstk_i2c_completion.h"
#include "shared/cli/bvstk_line_editor.h"

#ifndef BVSTK_SHELL_KSH_PATH
#define BVSTK_SHELL_KSH_PATH "/proc/boot/ksh"
#endif

enum {
    BVSTK_SHELL_PATH_MAX = 256,
    BVSTK_SHELL_IO_MAX = 512
};

static const char prompt_marker[] = "\036BVSTK_PROMPT\036";
static const char prompt_command[] =
    "printf '\\036BVSTK_PROMPT\\036%s\\036' \"$PWD\"\n";

typedef enum {
    OUTPUT_TEXT = 0,
    OUTPUT_MARKER,
    OUTPUT_PATH_END
} output_state_t;

typedef struct {
    bvstk_line_editor_t editor;
    int child_input;
    int child_output;
    pid_t child_pid;
    int waiting_for_prompt;
    int stop_requested;
    int ignore_next_lf;

    char prompt_path[BVSTK_SHELL_PATH_MAX];

    output_state_t output_state;
    size_t marker_length;
    char marker[BVSTK_SHELL_IO_MAX];
    char marker_path[BVSTK_SHELL_PATH_MAX];
    size_t marker_path_length;
} shell_state_t;

enum {
    BVSTK_SHELL_COMPLETION_MAX_MATCHES = 16,
    BVSTK_SHELL_COMPLETION_WORD_MAX = BVSTK_SHELL_PATH_MAX
};

typedef bvstk_line_editor_completion_t shell_completion_result_t;

static int write_all(int fd, const void *data, size_t length)
{
    const char *cursor = (const char *)data;

    while (length != 0U) {
        ssize_t written = write(fd, cursor, length);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return -1;
        }
        cursor += written;
        length -= (size_t)written;
    }
    return 0;
}

static int write_text(int fd, const char *text)
{
    return text != NULL ? write_all(fd, text, strlen(text)) : -1;
}

static void print_prompt(shell_state_t *shell)
{
    char prompt[BVSTK_SHELL_PATH_MAX + 32];
    const char *path = shell->prompt_path;
    int length;

    if (strcmp(path, "/flash") == 0 || strcmp(path, "/flash/") == 0 ||
        strcmp(path, "/root/flash:") == 0 ||
        strcmp(path, "/root/flash:/") == 0) {
        length = snprintf(prompt, sizeof(prompt), "flash:/# ");
    } else if (strncmp(path, "/flash/", strlen("/flash/")) == 0) {
        length = snprintf(prompt,
                          sizeof(prompt),
                          "flash:%s# ",
                          path + strlen("/flash"));
    } else if (strncmp(path,
                       "/root/flash:/",
                       strlen("/root/flash:/")) == 0) {
        length = snprintf(prompt,
                          sizeof(prompt),
                          "flash:%s# ",
                          path + strlen("/root/flash:"));
    } else if (strcmp(path, "/sd") == 0 || strcmp(path, "/sd/") == 0 ||
               strcmp(path, "/root/sd:") == 0 ||
               strcmp(path, "/root/sd:/") == 0) {
        length = snprintf(prompt, sizeof(prompt), "sd:/# ");
    } else if (strncmp(path, "/sd/", strlen("/sd/")) == 0) {
        length = snprintf(prompt,
                          sizeof(prompt),
                          "sd:%s# ",
                          path + strlen("/sd"));
    } else if (strncmp(path, "/root/sd:/", strlen("/root/sd:/")) == 0) {
        length = snprintf(prompt,
                          sizeof(prompt),
                          "sd:%s# ",
                          path + strlen("/root/sd:"));
    } else {
        length = snprintf(prompt, sizeof(prompt), "# ");
    }
    if (length > 0) {
        (void)write_all(STDOUT_FILENO, prompt, (size_t)length);
    }
}

static int line_starts_with_i2c(const char *line,
                                size_t line_length,
                                size_t cursor)
{
    size_t start = 0U;
    size_t i2c_length = strlen("i2c");

    if (line == NULL) {
        return 0;
    }
    if (cursor > line_length) {
        cursor = line_length;
    }
    while (start < cursor && isspace((unsigned char)line[start])) {
        ++start;
    }
    return cursor - start >= i2c_length &&
           strncasecmp(line + start, "i2c", i2c_length) == 0 &&
           (cursor - start == i2c_length ||
            isspace((unsigned char)line[start + i2c_length]));
}

static int shell_completion_copy_fragment(char *output,
                                          size_t output_size,
                                          const char *input,
                                          size_t input_length)
{
    if (output == NULL || input == NULL || output_size == 0U ||
        input_length >= output_size) {
        return -1;
    }
    memcpy(output, input, input_length);
    output[input_length] = '\0';
    return 0;
}

static int shell_completion_join_path(const char *base,
                                      const char *suffix,
                                      char *output,
                                      size_t output_size)
{
    size_t base_length;
    size_t suffix_length;
    size_t offset;

    if (base == NULL || suffix == NULL || output == NULL ||
        output_size == 0U) {
        return -1;
    }
    base_length = strlen(base);
    suffix_length = strlen(suffix);
    if (suffix_length == 0U) {
        if (base_length >= output_size) {
            return -1;
        }
        memcpy(output, base, base_length + 1U);
        return 0;
    }
    if (base_length != 0U && base[base_length - 1U] == '/') {
        if (base_length + suffix_length >= output_size) {
            return -1;
        }
        memcpy(output, base, base_length);
        memcpy(output + base_length, suffix, suffix_length + 1U);
        return 0;
    }
    if (suffix[0] == '/') {
        if (base_length + suffix_length >= output_size) {
            return -1;
        }
        memcpy(output, base, base_length);
        memcpy(output + base_length, suffix, suffix_length + 1U);
        return 0;
    }
    if (base_length + 1U + suffix_length >= output_size) {
        return -1;
    }
    memcpy(output, base, base_length);
    offset = base_length;
    output[offset++] = '/';
    memcpy(output + offset, suffix, suffix_length + 1U);
    return 0;
}

static int shell_completion_resolve_directory(const shell_state_t *shell,
                                              const char *directory_token,
                                              char *output,
                                              size_t output_size)
{
    static const char flash_prefix[] = "flash:/";
    static const char sd_prefix[] = "sd:/";
    const char *current_path;

    if (shell == NULL || directory_token == NULL || output == NULL ||
        output_size == 0U) {
        return -1;
    }
    current_path = shell->prompt_path[0] != '\0' ? shell->prompt_path : "/";
    if (directory_token[0] == '\0') {
        return shell_completion_join_path(current_path, "", output, output_size);
    }
    if (strncmp(directory_token, flash_prefix, sizeof(flash_prefix) - 1U) == 0) {
        return shell_completion_join_path("/root/flash:",
                                          directory_token + sizeof(flash_prefix) - 1U,
                                          output,
                                          output_size);
    }
    if (strncmp(directory_token, sd_prefix, sizeof(sd_prefix) - 1U) == 0) {
        return shell_completion_join_path("/root/sd:",
                                          directory_token + sizeof(sd_prefix) - 1U,
                                          output,
                                          output_size);
    }
    if (directory_token[0] == '~') {
        return shell_completion_join_path("/root",
                                          directory_token + 1U,
                                          output,
                                          output_size);
    }
    if (directory_token[0] == '/') {
        return shell_completion_join_path(directory_token,
                                          "",
                                          output,
                                          output_size);
    }
    return shell_completion_join_path(current_path,
                                      directory_token,
                                      output,
                                      output_size);
}

static int shell_completion_add_match(shell_completion_result_t *result,
                                      const char *prefix,
                                      const char *candidate)
{
    size_t index;
    size_t prefix_length;

    if (result == NULL || prefix == NULL || candidate == NULL ||
        candidate[0] == '\0') {
        return 0;
    }
    prefix_length = strlen(prefix);
    if (strncmp(candidate, prefix, prefix_length) != 0) {
        return 0;
    }
    for (index = 0U; index < result->match_count; ++index) {
        if (strcmp(result->candidates[index], candidate) == 0) {
            return 1;
        }
    }
    if (result->match_count >= BVSTK_SHELL_COMPLETION_MAX_MATCHES ||
        strlen(candidate) >= BVSTK_SHELL_COMPLETION_WORD_MAX) {
        return 0;
    }
    strcpy(result->candidates[result->match_count], candidate);
    ++result->match_count;
    return 1;
}

static int shell_completion_make_candidate(const char *display_prefix,
                                           const char *name,
                                           int is_directory,
                                           char *output,
                                           size_t output_size)
{
    size_t prefix_length;
    size_t name_length;
    size_t length;

    if (display_prefix == NULL || name == NULL || output == NULL ||
        output_size == 0U) {
        return -1;
    }
    prefix_length = strlen(display_prefix);
    name_length = strlen(name);
    length = prefix_length + name_length + (is_directory ? 1U : 0U);
    if (length >= output_size) {
        return -1;
    }
    memcpy(output, display_prefix, prefix_length);
    memcpy(output + prefix_length, name, name_length);
    if (is_directory) {
        output[prefix_length + name_length] = '/';
    }
    output[length] = '\0';
    return 0;
}

static int shell_completion_entry_is_directory(const char *directory,
                                                const char *name)
{
    char path[BVSTK_SHELL_PATH_MAX];
    struct stat status;

    if (shell_completion_join_path(directory,
                                   name,
                                   path,
                                   sizeof(path)) != 0 ||
        stat(path, &status) != 0) {
        return 0;
    }
    return S_ISDIR(status.st_mode) ? 1 : 0;
}

static size_t shell_completion_token_count(const char *line, size_t upto)
{
    size_t count = 0U;
    size_t index = 0U;
    int in_token = 0;

    if (line == NULL) {
        return 0U;
    }
    while (index < upto) {
        if (line[index] == ' ' || line[index] == '\t') {
            in_token = 0;
        } else if (!in_token) {
            in_token = 1;
            ++count;
        }
        ++index;
    }
    return count;
}

static void complete_shell_files(const shell_state_t *shell,
                                 const char *token_prefix,
                                 shell_completion_result_t *result)
{
    char directory_token[BVSTK_SHELL_PATH_MAX];
    char display_prefix[BVSTK_SHELL_PATH_MAX];
    char directory_path[BVSTK_SHELL_PATH_MAX];
    char candidate[BVSTK_SHELL_COMPLETION_WORD_MAX];
    const char *entry_prefix;
    const char *slash;
    DIR *directory;
    struct dirent *entry;
    size_t base_length = 0U;

    if (shell == NULL || token_prefix == NULL || result == NULL) {
        return;
    }
    slash = strrchr(token_prefix, '/');
    if (slash != NULL) {
        base_length = (size_t)(slash - token_prefix) + 1U;
        if (shell_completion_copy_fragment(display_prefix,
                                            sizeof(display_prefix),
                                            token_prefix,
                                            base_length) != 0 ||
            shell_completion_copy_fragment(directory_token,
                                            sizeof(directory_token),
                                            token_prefix,
                                            base_length) != 0) {
            return;
        }
        entry_prefix = slash + 1U;
    } else {
        display_prefix[0] = '\0';
        directory_token[0] = '\0';
        entry_prefix = token_prefix;
        (void)shell_completion_add_match(result, token_prefix, "flash:/");
        (void)shell_completion_add_match(result, token_prefix, "sd:/");
    }
    if (shell_completion_resolve_directory(shell,
                                            directory_token,
                                            directory_path,
                                            sizeof(directory_path)) != 0) {
        return;
    }
    directory = opendir(directory_path);
    if (directory == NULL) {
        return;
    }
    while ((entry = readdir(directory)) != NULL) {
        int is_directory;

        if (entry->d_name[0] == '.' && entry_prefix[0] != '.') {
            continue;
        }
        if (strncmp(entry->d_name,
                    entry_prefix,
                    strlen(entry_prefix)) != 0) {
            continue;
        }
        is_directory = shell_completion_entry_is_directory(directory_path,
                                                            entry->d_name);
        if (shell_completion_make_candidate(display_prefix,
                                             entry->d_name,
                                             is_directory,
                                             candidate,
                                             sizeof(candidate)) == 0) {
            (void)shell_completion_add_match(result, token_prefix, candidate);
        }
    }
    closedir(directory);
}

static void complete_shell_commands(const char *prefix,
                                    shell_completion_result_t *result)
{
    static const char *const builtins[] = {
        ".", "[", "alias", "bg", "bind", "cd", "command", "echo",
        "eval", "exec", "exit", "export", "false", "fg", "help",
        "history", "jobs", "kill", "printf", "pwd", "quit", "read",
        "set", "source", "test", "true", "type", "unalias", "umask",
        "unset", "wait"
    };
    const char *path;
    const char *path_start;

    size_t index;

    for (index = 0U; index < sizeof(builtins) / sizeof(builtins[0]); ++index) {
        (void)shell_completion_add_match(result, prefix, builtins[index]);
    }
    path = getenv("PATH");
    if (path == NULL) {
        return;
    }
    path_start = path;
    while (*path_start != '\0') {
        const char *path_end = strchr(path_start, ':');
        char directory_path[BVSTK_SHELL_PATH_MAX];
        DIR *directory;
        struct dirent *entry;
        size_t directory_length = path_end != NULL
                                       ? (size_t)(path_end - path_start)
                                       : strlen(path_start);

        if (directory_length == 0U) {
            strcpy(directory_path, ".");
        } else if (shell_completion_copy_fragment(directory_path,
                                                   sizeof(directory_path),
                                                   path_start,
                                                   directory_length) != 0) {
            goto next_path;
        }
        directory = opendir(directory_path);
        if (directory == NULL) {
            goto next_path;
        }
        while ((entry = readdir(directory)) != NULL) {
            char executable_path[BVSTK_SHELL_PATH_MAX];
            struct stat status;

            if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0 ||
                shell_completion_join_path(directory_path,
                                           entry->d_name,
                                           executable_path,
                                           sizeof(executable_path)) != 0 ||
                stat(executable_path, &status) != 0 ||
                !S_ISREG(status.st_mode) || access(executable_path, X_OK) != 0) {
                continue;
            }
            (void)shell_completion_add_match(result, prefix, entry->d_name);
        }
        closedir(directory);
next_path:
        if (path_end == NULL) {
            break;
        }
        path_start = path_end + 1;
    }
}

static int complete_shell_line(const shell_state_t *shell,
                               const char *line,
                               size_t line_length,
                               size_t cursor,
                               shell_completion_result_t *result)
{
    size_t token_start;
    size_t token_prefix_length;
    size_t token_count;
    char prefix[BVSTK_SHELL_COMPLETION_WORD_MAX];

    if (shell == NULL || line == NULL || result == NULL) {
        return 0;
    }
    memset(result, 0, sizeof(*result));
    if (cursor > line_length) {
        cursor = line_length;
    }
    token_start = cursor;
    while (token_start > 0U && line[token_start - 1U] != ' ' &&
           line[token_start - 1U] != '\t') {
        --token_start;
    }
    token_prefix_length = cursor - token_start;
    if (shell_completion_copy_fragment(prefix,
                                       sizeof(prefix),
                                       line + token_start,
                                       token_prefix_length) != 0) {
        return 0;
    }
    result->token_start = token_start;
    result->token_prefix_length = token_prefix_length;
    token_count = shell_completion_token_count(line, token_start);
    if (token_count == 0U) {
        complete_shell_commands(prefix, result);
    } else {
        complete_shell_files(shell, prefix, result);
    }
    return result->match_count != 0U;
}

static int shell_editor_write(void *context,
                              const void *data,
                              size_t length)
{
    (void)context;
    return write_all(STDOUT_FILENO, data, length);
}

static void shell_editor_prompt(void *context)
{
    print_prompt((shell_state_t *)context);
}

static int shell_editor_submit(void *context,
                               const char *line,
                               size_t length)
{
    shell_state_t *shell = (shell_state_t *)context;

    if (shell == NULL || shell->child_input < 0 || line == NULL ||
        write_all(shell->child_input, line, length) != 0 ||
        write_text(shell->child_input, "\n") != 0 ||
        write_all(shell->child_input,
                  prompt_command,
                  sizeof(prompt_command) - 1U) != 0) {
        return BVSTK_LINE_EDITOR_SUBMIT_STOP;
    }
    shell->waiting_for_prompt = 1;
    return BVSTK_LINE_EDITOR_SUBMIT_DEFER_PROMPT;
}

static int shell_editor_complete(void *context,
                                 const char *line,
                                 size_t length,
                                 size_t cursor,
                                 bvstk_line_editor_completion_t *result)
{
    shell_state_t *shell = (shell_state_t *)context;

    if (shell == NULL || line == NULL || result == NULL) {
        return 0;
    }
    if (line_starts_with_i2c(line, length, cursor)) {
        bvstk_i2c_completion_device_t devices[I2C_CFG_MAX_DEVICES];
        bvstk_i2c_completion_result_t i2c_result;
        size_t device_count = 0U;
        size_t index;

        memset(devices, 0, sizeof(devices));
        (void)bvstk_i2c_client_list_devices(devices,
                                            I2C_CFG_MAX_DEVICES,
                                            &device_count);
        if (!bvstk_i2c_complete_line(line,
                                     length,
                                     cursor,
                                     devices,
                                     device_count,
                                     &i2c_result)) {
            return 0;
        }
        memset(result, 0, sizeof(*result));
        result->token_start = i2c_result.token_start;
        result->token_prefix_length = i2c_result.token_prefix_length;
        result->match_count = i2c_result.match_count;
        if (result->match_count > BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES) {
            result->match_count = BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES;
        }
        for (index = 0U; index < result->match_count; ++index) {
            strncpy(result->candidates[index],
                    i2c_result.candidates[index],
                    BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U);
            result->candidates[index]
                    [BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX - 1U] = '\0';
        }
        return result->match_count != 0U;
    }
    return complete_shell_line(shell, line, length, cursor, result);
}

static void signal_child_interrupt(shell_state_t *shell)
{
    if (shell->child_pid > 0) {
        (void)kill(-shell->child_pid, SIGINT);
    }
    (void)write_text(STDOUT_FILENO, "^C\r\n");
}

static void handle_waiting_byte(shell_state_t *shell, unsigned char character)
{
    if (character == 0x03U) {
        signal_child_interrupt(shell);
    } else if (character == 0x04U) {
        if (shell->child_input >= 0) {
            close(shell->child_input);
            shell->child_input = -1;
        }
    } else if (shell->child_input >= 0) {
        (void)write_all(shell->child_input, &character, 1U);
    }
}

static void handle_input_byte(shell_state_t *shell, unsigned char character)
{
    if (shell->ignore_next_lf) {
        shell->ignore_next_lf = 0;
        if (character == '\n') {
            return;
        }
    }
    if (shell->waiting_for_prompt) {
        handle_waiting_byte(shell, character);
        return;
    }
    if (character == '\r') {
        shell->ignore_next_lf = character == '\r';
    }
    if (bvstk_line_editor_handle_byte(&shell->editor, character) != 0) {
        shell->stop_requested = 1;
    }
}

static int consume_marker_byte(shell_state_t *shell, unsigned char character)
{
    size_t marker_size = sizeof(prompt_marker) - 1U;

    if (shell->output_state == OUTPUT_TEXT) {
        if (character == (unsigned char)prompt_marker[0]) {
            shell->output_state = OUTPUT_MARKER;
            shell->marker_length = 1U;
            shell->marker[0] = (char)character;
        } else {
            (void)write_all(STDOUT_FILENO, &character, 1U);
        }
        return 0;
    }
    if (shell->output_state == OUTPUT_MARKER) {
        if (shell->marker_length < marker_size &&
            character == (unsigned char)prompt_marker[shell->marker_length]) {
            shell->marker[shell->marker_length++] = (char)character;
            if (shell->marker_length == marker_size) {
                shell->output_state = OUTPUT_PATH_END;
                shell->marker_path_length = 0U;
            }
        } else {
            (void)write_all(STDOUT_FILENO,
                            shell->marker,
                            shell->marker_length);
            shell->output_state = OUTPUT_TEXT;
            shell->marker_length = 0U;
            (void)consume_marker_byte(shell, character);
        }
        return 0;
    }
    if (character == 0x1EU) {
        shell->marker_path[shell->marker_path_length] = '\0';
        strncpy(shell->prompt_path,
                shell->marker_path,
                sizeof(shell->prompt_path) - 1U);
        shell->prompt_path[sizeof(shell->prompt_path) - 1U] = '\0';
        shell->output_state = OUTPUT_TEXT;
        shell->marker_length = 0U;
        shell->waiting_for_prompt = 0;
        print_prompt(shell);
        return 1;
    }
    if (shell->marker_path_length + 1U < sizeof(shell->marker_path)) {
        shell->marker_path[shell->marker_path_length++] = (char)character;
        return 0;
    }
    (void)write_all(STDOUT_FILENO,
                    shell->marker,
                    shell->marker_length);
    (void)write_all(STDOUT_FILENO,
                    shell->marker_path,
                    shell->marker_path_length);
    (void)write_text(STDOUT_FILENO, "\036");
    shell->output_state = OUTPUT_TEXT;
    shell->marker_length = 0U;
    (void)consume_marker_byte(shell, character);
    return 0;
}

static void consume_child_output(shell_state_t *shell,
                                 const unsigned char *data,
                                 size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        (void)consume_marker_byte(shell, data[index]);
    }
}

static int enter_raw_terminal(struct termios *saved)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, saved) != 0) {
        return -1;
    }
    raw = *saved;
    (void)cfmakeraw(&raw);
    raw.c_oflag = saved->c_oflag;
    return tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void restore_terminal(const struct termios *saved)
{
    if (saved != NULL) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, saved);
    }
}

static int start_child(shell_state_t *shell)
{
    int input_pipe[2];
    int output_pipe[2];
    pid_t child;

    if (pipe(input_pipe) != 0) {
        return -1;
    }
    if (pipe(output_pipe) != 0) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        return -1;
    }
    child = fork();
    if (child < 0) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -1;
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        (void)dup2(input_pipe[0], STDIN_FILENO);
        (void)dup2(output_pipe[1], STDOUT_FILENO);
        (void)dup2(output_pipe[1], STDERR_FILENO);
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        execl(BVSTK_SHELL_KSH_PATH,
              "ksh",
              "-s",
              (char *)NULL);
        _exit(127);
    }
    (void)setpgid(child, child);
    close(input_pipe[0]);
    close(output_pipe[1]);
    shell->child_input = input_pipe[1];
    shell->child_output = output_pipe[0];
    shell->child_pid = child;
    return 0;
}

static int stop_child(shell_state_t *shell)
{
    int status = 0;
    pid_t waited;

    if (shell->child_input >= 0) {
        close(shell->child_input);
        shell->child_input = -1;
    }
    if (shell->child_output >= 0) {
        close(shell->child_output);
        shell->child_output = -1;
    }
    if (shell->child_pid > 0) {
        waited = waitpid(shell->child_pid, &status, WNOHANG);
        if (waited == 0) {
            (void)kill(-shell->child_pid, SIGTERM);
            (void)waitpid(shell->child_pid, &status, 0);
        }
        shell->child_pid = -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
}

static int run_interactive(void)
{
    shell_state_t shell;
    bvstk_line_editor_config_t editor_config;
    struct termios saved_terminal;
    unsigned char buffer[BVSTK_SHELL_IO_MAX];
    int result = 0;

    memset(&shell, 0, sizeof(shell));
    shell.child_input = -1;
    shell.child_output = -1;
    shell.child_pid = -1;
    shell.output_state = OUTPUT_TEXT;
    (void)snprintf(shell.prompt_path, sizeof(shell.prompt_path), "/");
    memset(&editor_config, 0, sizeof(editor_config));
    editor_config.context = &shell;
    editor_config.write = shell_editor_write;
    editor_config.prompt = shell_editor_prompt;
    editor_config.submit = shell_editor_submit;
    editor_config.complete = shell_editor_complete;
    editor_config.eof_on_empty = 1;
    bvstk_line_editor_init(&shell.editor, &editor_config);
    if (start_child(&shell) != 0 || enter_raw_terminal(&saved_terminal) != 0) {
        (void)stop_child(&shell);
        return 1;
    }
    if (write_text(shell.child_input, ". /etc/profile\n") != 0 ||
        write_all(shell.child_input,
                  prompt_command,
                  sizeof(prompt_command) - 1U) != 0) {
        shell.stop_requested = 1;
    }
    shell.waiting_for_prompt = 1;

    while (!shell.stop_requested) {
        fd_set read_set;
        int maximum_fd = -1;
        int select_result;

        FD_ZERO(&read_set);
        if (shell.child_output >= 0) {
            FD_SET(shell.child_output, &read_set);
            maximum_fd = shell.child_output;
        }
        FD_SET(STDIN_FILENO, &read_set);
        if (STDIN_FILENO > maximum_fd) {
            maximum_fd = STDIN_FILENO;
        }
        select_result = select(maximum_fd + 1, &read_set, NULL, NULL, NULL);
        if (select_result < 0 && errno == EINTR) {
            continue;
        }
        if (select_result < 0) {
            result = 1;
            break;
        }
        if (shell.child_output >= 0 &&
            FD_ISSET(shell.child_output, &read_set)) {
            ssize_t length = read(shell.child_output, buffer, sizeof(buffer));

            if (length > 0) {
                consume_child_output(&shell, buffer, (size_t)length);
            } else if (length == 0 || errno != EINTR) {
                close(shell.child_output);
                shell.child_output = -1;
                shell.stop_requested = 1;
            }
        }
        if (!shell.stop_requested && FD_ISSET(STDIN_FILENO, &read_set)) {
            ssize_t length = read(STDIN_FILENO, buffer, sizeof(buffer));

            if (length <= 0) {
                shell.stop_requested = 1;
            } else {
                size_t index;

                for (index = 0U; index < (size_t)length; ++index) {
                    handle_input_byte(&shell, buffer[index]);
                    if (shell.stop_requested) {
                        break;
                    }
                }
            }
        }
    }
    restore_terminal(&saved_terminal);
    result = stop_child(&shell);
    return result;
}

static int exec_real_shell(int argc, char **argv)
{
    char **child_argv;
    int index;

    child_argv = (char **)calloc((size_t)argc + 1U, sizeof(*child_argv));
    if (child_argv == NULL) {
        return 1;
    }
    child_argv[0] = (char *)"sh";
    for (index = 1; index < argc; ++index) {
        child_argv[index] = argv[index];
    }
    child_argv[argc] = NULL;
    execv(BVSTK_SHELL_KSH_PATH, child_argv);
    free(child_argv);
    return 127;
}

int main(int argc, char **argv)
{
    if (argc != 1 || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return exec_real_shell(argc, argv);
    }
    return run_interactive();
}
