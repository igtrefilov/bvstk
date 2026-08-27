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

#ifndef BVSTK_SHELL_KSH_PATH
#define BVSTK_SHELL_KSH_PATH "/proc/boot/ksh"
#endif

enum {
    BVSTK_SHELL_LINE_MAX = 512,
    BVSTK_SHELL_HISTORY_MAX = 16,
    BVSTK_SHELL_PATH_MAX = 256,
    BVSTK_SHELL_IO_MAX = 512
};

static const char prompt_marker[] = "\036BVSTK_PROMPT\036";
static const char prompt_command[] =
    "printf '\\036BVSTK_PROMPT\\036%s\\036' \"$PWD\"\n";

typedef enum {
    ESCAPE_NONE = 0,
    ESCAPE_STARTED,
    ESCAPE_CSI,
    ESCAPE_CSI_DELETE
} escape_state_t;

typedef enum {
    OUTPUT_TEXT = 0,
    OUTPUT_MARKER,
    OUTPUT_PATH_END
} output_state_t;

typedef struct {
    int child_input;
    int child_output;
    pid_t child_pid;
    int waiting_for_prompt;
    int stop_requested;
    int ignore_next_lf;

    char line[BVSTK_SHELL_LINE_MAX];
    size_t line_length;
    size_t cursor;
    char history[BVSTK_SHELL_HISTORY_MAX][BVSTK_SHELL_LINE_MAX];
    size_t history_count;
    int history_position;

    char prompt_path[BVSTK_SHELL_PATH_MAX];
    escape_state_t escape_state;

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

typedef struct {
    size_t token_start;
    size_t token_prefix_length;
    size_t match_count;
    char candidates[BVSTK_SHELL_COMPLETION_MAX_MATCHES]
                   [BVSTK_SHELL_COMPLETION_WORD_MAX];
} shell_completion_result_t;

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

static void beep(void)
{
    (void)write_text(STDOUT_FILENO, "\a");
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

static void redraw_line(shell_state_t *shell)
{
    size_t index;

    (void)write_text(STDOUT_FILENO, "\r\033[2K");
    print_prompt(shell);
    if (shell->line_length != 0U) {
        (void)write_all(STDOUT_FILENO, shell->line, shell->line_length);
    }
    for (index = shell->cursor; index < shell->line_length; ++index) {
        (void)write_text(STDOUT_FILENO, "\033[D");
    }
}

static void reset_line(shell_state_t *shell)
{
    shell->line_length = 0U;
    shell->cursor = 0U;
    shell->line[0] = '\0';
    shell->history_position = -1;
}

static void add_history(shell_state_t *shell)
{
    if (shell->line_length == 0U ||
        (shell->history_count != 0U &&
         strcmp(shell->history[shell->history_count - 1U], shell->line) == 0)) {
        return;
    }
    if (shell->history_count == BVSTK_SHELL_HISTORY_MAX) {
        size_t index;

        for (index = 1U; index < BVSTK_SHELL_HISTORY_MAX; ++index) {
            memcpy(shell->history[index - 1U],
                   shell->history[index],
                   sizeof(shell->history[index - 1U]));
        }
        shell->history_count--;
    }
    memcpy(shell->history[shell->history_count],
           shell->line,
           shell->line_length + 1U);
    ++shell->history_count;
}

static void set_line(shell_state_t *shell, const char *line)
{
    size_t length = line != NULL ? strlen(line) : 0U;

    if (length >= sizeof(shell->line)) {
        length = sizeof(shell->line) - 1U;
    }
    if (length != 0U) {
        memcpy(shell->line, line, length);
    }
    shell->line[length] = '\0';
    shell->line_length = length;
    shell->cursor = length;
}

static void history_up(shell_state_t *shell)
{
    if (shell->history_count == 0U) {
        beep();
        return;
    }
    if (shell->history_position < 0) {
        shell->history_position = (int)shell->history_count - 1;
    } else if (shell->history_position > 0) {
        --shell->history_position;
    } else {
        beep();
        return;
    }
    set_line(shell, shell->history[shell->history_position]);
    redraw_line(shell);
}

static void history_down(shell_state_t *shell)
{
    if (shell->history_position < 0) {
        beep();
        return;
    }
    if ((size_t)(shell->history_position + 1) < shell->history_count) {
        ++shell->history_position;
        set_line(shell, shell->history[shell->history_position]);
    } else {
        reset_line(shell);
    }
    redraw_line(shell);
}

static void insert_character(shell_state_t *shell, char character)
{
    size_t tail;

    if (shell->line_length + 1U >= sizeof(shell->line)) {
        beep();
        return;
    }
    tail = shell->line_length - shell->cursor;
    memmove(shell->line + shell->cursor + 1U,
            shell->line + shell->cursor,
            tail);
    shell->line[shell->cursor] = character;
    ++shell->line_length;
    ++shell->cursor;
    (void)write_all(STDOUT_FILENO, &character, 1U);
    if (tail != 0U) {
        (void)write_all(STDOUT_FILENO,
                        shell->line + shell->cursor,
                        tail);
        while (tail-- != 0U) {
            (void)write_text(STDOUT_FILENO, "\033[D");
        }
    }
}

static void backspace_character(shell_state_t *shell)
{
    size_t tail;

    if (shell->cursor == 0U) {
        beep();
        return;
    }
    --shell->cursor;
    tail = shell->line_length - shell->cursor - 1U;
    memmove(shell->line + shell->cursor,
            shell->line + shell->cursor + 1U,
            tail);
    --shell->line_length;
    shell->line[shell->line_length] = '\0';
    redraw_line(shell);
}

static void delete_character(shell_state_t *shell)
{
    size_t tail;

    if (shell->cursor >= shell->line_length) {
        beep();
        return;
    }
    tail = shell->line_length - shell->cursor - 1U;
    memmove(shell->line + shell->cursor,
            shell->line + shell->cursor + 1U,
            tail);
    --shell->line_length;
    shell->line[shell->line_length] = '\0';
    redraw_line(shell);
}

static void delete_previous_word(shell_state_t *shell)
{
    size_t start = shell->cursor;

    while (start > 0U && isspace((unsigned char)shell->line[start - 1U])) {
        --start;
    }
    while (start > 0U && !isspace((unsigned char)shell->line[start - 1U])) {
        --start;
    }
    if (start == shell->cursor) {
        beep();
        return;
    }
    memmove(shell->line + start,
            shell->line + shell->cursor,
            shell->line_length - shell->cursor);
    shell->line_length -= shell->cursor - start;
    shell->cursor = start;
    shell->line[shell->line_length] = '\0';
    redraw_line(shell);
}

static void replace_completion_prefix(shell_state_t *shell,
                                      size_t token_start,
                                      size_t prefix_length,
                                      const char *replacement,
                                      size_t replacement_length,
                                      int append_space)
{
    size_t tail;
    size_t new_length;

    if (token_start > shell->cursor ||
        prefix_length != shell->cursor - token_start ||
        replacement == NULL) {
        return;
    }
    tail = shell->line_length - shell->cursor;
    new_length = token_start + replacement_length + tail;
    if (append_space && replacement_length != 0U &&
        replacement[replacement_length - 1U] != '/') {
        ++new_length;
    }
    if (new_length >= sizeof(shell->line)) {
        beep();
        return;
    }
    memmove(shell->line + token_start + replacement_length,
            shell->line + shell->cursor,
            tail);
    memcpy(shell->line + token_start, replacement, replacement_length);
    shell->line_length = token_start + replacement_length + tail;
    shell->cursor = token_start + replacement_length;
    if (append_space && replacement_length != 0U &&
        replacement[replacement_length - 1U] != '/') {
        memmove(shell->line + shell->cursor + 1U,
                shell->line + shell->cursor,
                tail);
        shell->line[shell->cursor] = ' ';
        ++shell->line_length;
        ++shell->cursor;
    }
    shell->line[shell->line_length] = '\0';
    redraw_line(shell);
}

static int line_starts_with_i2c(const shell_state_t *shell)
{
    size_t start = 0U;
    size_t i2c_length = strlen("i2c");

    while (start < shell->cursor && isspace((unsigned char)shell->line[start])) {
        ++start;
    }
    return shell->cursor - start >= i2c_length &&
           strncasecmp(shell->line + start, "i2c", i2c_length) == 0 &&
           (shell->cursor - start == i2c_length ||
            isspace((unsigned char)shell->line[start + i2c_length]));
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

static size_t shell_completion_common_prefix_len(
    const char candidates[][BVSTK_SHELL_COMPLETION_WORD_MAX],
    size_t candidate_count)
{
    size_t length;
    size_t index;

    if (candidates == NULL || candidate_count == 0U) {
        return 0U;
    }
    length = strlen(candidates[0]);
    for (index = 1U; index < candidate_count; ++index) {
        size_t current = 0U;

        while (current < length &&
               candidates[index][current] != '\0' &&
               tolower((unsigned char)candidates[0][current]) ==
                   tolower((unsigned char)candidates[index][current])) {
            ++current;
        }
        length = current;
        if (length == 0U) {
            break;
        }
    }
    return length;
}

static int complete_shell_line(const shell_state_t *shell,
                               shell_completion_result_t *result)
{
    size_t cursor;
    size_t token_start;
    size_t token_prefix_length;
    size_t token_count;
    char prefix[BVSTK_SHELL_COMPLETION_WORD_MAX];

    if (shell == NULL || result == NULL) {
        return 0;
    }
    memset(result, 0, sizeof(*result));
    cursor = shell->cursor <= shell->line_length ? shell->cursor
                                                  : shell->line_length;
    token_start = cursor;
    while (token_start > 0U && shell->line[token_start - 1U] != ' ' &&
           shell->line[token_start - 1U] != '\t') {
        --token_start;
    }
    token_prefix_length = cursor - token_start;
    if (shell_completion_copy_fragment(prefix,
                                       sizeof(prefix),
                                       shell->line + token_start,
                                       token_prefix_length) != 0) {
        return 0;
    }
    result->token_start = token_start;
    result->token_prefix_length = token_prefix_length;
    token_count = shell_completion_token_count(shell->line, token_start);
    if (token_count == 0U) {
        complete_shell_commands(prefix, result);
    } else {
        complete_shell_files(shell, prefix, result);
    }
    return result->match_count != 0U;
}

static void apply_shell_completion(shell_state_t *shell,
                                   const shell_completion_result_t *result)
{
    size_t prefix_length;
    size_t common_length;
    size_t index;

    if (shell == NULL || result == NULL || result->match_count == 0U) {
        return;
    }
    prefix_length = result->token_prefix_length;
    if (result->match_count == 1U) {
        replace_completion_prefix(shell,
                                  result->token_start,
                                  prefix_length,
                                  result->candidates[0],
                                  strlen(result->candidates[0]),
                                  1);
        return;
    }
    common_length = shell_completion_common_prefix_len(result->candidates,
                                                       result->match_count);
    if (common_length > prefix_length) {
        replace_completion_prefix(shell,
                                  result->token_start,
                                  prefix_length,
                                  result->candidates[0],
                                  common_length,
                                  0);
    }
    (void)write_text(STDOUT_FILENO, "\r\n");
    for (index = 0U; index < result->match_count; ++index) {
        (void)write_text(STDOUT_FILENO, result->candidates[index]);
        (void)write_text(STDOUT_FILENO, "  ");
    }
    (void)write_text(STDOUT_FILENO, "\r\n");
    redraw_line(shell);
}

static void complete_i2c(shell_state_t *shell)
{
    bvstk_i2c_completion_device_t devices[I2C_CFG_MAX_DEVICES];
    bvstk_i2c_completion_result_t result;
    size_t device_count = 0U;

    if (!line_starts_with_i2c(shell)) {
        beep();
        return;
    }
    memset(devices, 0, sizeof(devices));
    (void)bvstk_i2c_client_list_devices(devices,
                                        I2C_CFG_MAX_DEVICES,
                                        &device_count);
    if (!bvstk_i2c_complete_line(shell->line,
                                 shell->line_length,
                                 shell->cursor,
                                 devices,
                                 device_count,
                                 &result)) {
        beep();
        return;
    }
    if (result.match_count == 1U) {
        replace_completion_prefix(shell,
                                  result.token_start,
                                  result.token_prefix_length,
                                  result.candidates[0],
                                  strlen(result.candidates[0]),
                                  1);
        return;
    }
    if (result.match_count > 1U) {
        size_t common_length =
            bvstk_i2c_completion_common_prefix_len(result.candidates,
                                                   result.match_count);
        size_t index;

        if (common_length > result.token_prefix_length) {
            replace_completion_prefix(shell,
                                      result.token_start,
                                      result.token_prefix_length,
                                      result.candidates[0],
                                      common_length,
                                      0);
        }
        (void)write_text(STDOUT_FILENO, "\r\n");
        for (index = 0U; index < result.match_count; ++index) {
            (void)write_text(STDOUT_FILENO, result.candidates[index]);
            (void)write_text(STDOUT_FILENO, "  ");
        }
        (void)write_text(STDOUT_FILENO, "\r\n");
        redraw_line(shell);
    }
}

static int submit_line(shell_state_t *shell)
{
    if (write_text(STDOUT_FILENO, "\r\n") != 0 ||
        write_all(shell->child_input, shell->line, shell->line_length) != 0 ||
        write_text(shell->child_input, "\n") != 0 ||
        write_all(shell->child_input,
                  prompt_command,
                  sizeof(prompt_command) - 1U) != 0) {
        return -1;
    }
    add_history(shell);
    reset_line(shell);
    shell->waiting_for_prompt = 1;
    return 0;
}

static void signal_child_interrupt(shell_state_t *shell)
{
    if (shell->child_pid > 0) {
        (void)kill(-shell->child_pid, SIGINT);
    }
    (void)write_text(STDOUT_FILENO, "^C\r\n");
}

static void handle_escape_byte(shell_state_t *shell, unsigned char character)
{
    if (shell->escape_state == ESCAPE_STARTED) {
        if (character == '[') {
            shell->escape_state = ESCAPE_CSI;
        } else {
            shell->escape_state = ESCAPE_NONE;
            beep();
        }
        return;
    }
    if (shell->escape_state == ESCAPE_CSI) {
        if (character == 'A') {
            shell->escape_state = ESCAPE_NONE;
            history_up(shell);
        } else if (character == 'B') {
            shell->escape_state = ESCAPE_NONE;
            history_down(shell);
        } else if (character == 'C') {
            shell->escape_state = ESCAPE_NONE;
            if (shell->cursor < shell->line_length) {
                ++shell->cursor;
                (void)write_text(STDOUT_FILENO, "\033[C");
            }
        } else if (character == 'D') {
            shell->escape_state = ESCAPE_NONE;
            if (shell->cursor > 0U) {
                --shell->cursor;
                (void)write_text(STDOUT_FILENO, "\033[D");
            }
        } else if (character == 'H') {
            shell->escape_state = ESCAPE_NONE;
            shell->cursor = 0U;
            redraw_line(shell);
        } else if (character == 'F') {
            shell->escape_state = ESCAPE_NONE;
            shell->cursor = shell->line_length;
            redraw_line(shell);
        } else if (character == '3') {
            shell->escape_state = ESCAPE_CSI_DELETE;
        } else if (character != ';' && !isdigit(character)) {
            shell->escape_state = ESCAPE_NONE;
            beep();
        }
        return;
    }
    if (shell->escape_state == ESCAPE_CSI_DELETE) {
        if (character == '~') {
            shell->escape_state = ESCAPE_NONE;
            delete_character(shell);
        } else if (isdigit(character)) {
            return;
        } else {
            shell->escape_state = ESCAPE_NONE;
            beep();
        }
    }
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
    if (shell->escape_state != ESCAPE_NONE) {
        handle_escape_byte(shell, character);
        return;
    }
    if (character == 0x1BU) {
        shell->escape_state = ESCAPE_STARTED;
    } else if (character == '\r' || character == '\n') {
        shell->ignore_next_lf = character == '\r';
        if (submit_line(shell) != 0) {
            shell->stop_requested = 1;
        }
    } else if (character == 0x03U) {
        reset_line(shell);
        (void)write_text(STDOUT_FILENO, "^C\r\n");
        redraw_line(shell);
    } else if (character == 0x04U) {
        if (shell->line_length == 0U) {
            shell->stop_requested = 1;
        } else {
            delete_character(shell);
        }
    } else if (character == 0x08U || character == 0x7FU) {
        backspace_character(shell);
    } else if (character == '\t') {
        if (line_starts_with_i2c(shell)) {
            complete_i2c(shell);
        } else {
            shell_completion_result_t completion;

            if (complete_shell_line(shell, &completion)) {
                apply_shell_completion(shell, &completion);
            } else {
                beep();
            }
        }
    } else if (character == 0x01U) {
        shell->cursor = 0U;
        redraw_line(shell);
    } else if (character == 0x05U) {
        shell->cursor = shell->line_length;
        redraw_line(shell);
    } else if (character == 0x15U) {
        reset_line(shell);
        redraw_line(shell);
    } else if (character == 0x17U) {
        delete_previous_word(shell);
    } else if (character == 0x0CU) {
        redraw_line(shell);
    } else if (isprint(character)) {
        insert_character(shell, (char)character);
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
    struct termios saved_terminal;
    unsigned char buffer[BVSTK_SHELL_IO_MAX];
    int result = 0;

    memset(&shell, 0, sizeof(shell));
    shell.child_input = -1;
    shell.child_output = -1;
    shell.child_pid = -1;
    shell.history_position = -1;
    shell.output_state = OUTPUT_TEXT;
    (void)snprintf(shell.prompt_path, sizeof(shell.prompt_path), "/");
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
