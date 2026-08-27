#include "shared/cli/bvstk_line_editor.h"

#include <ctype.h>
#include <string.h>

enum {
    ESCAPE_NONE = 0,
    ESCAPE_STARTED,
    ESCAPE_CSI,
    ESCAPE_SS3
};

static int editor_write_all(
    bvstk_line_editor_t *editor,
    const void *data,
    size_t length)
{
    if (editor == NULL || editor->config.write == NULL) {
        return -1;
    }
    return editor->config.write(editor->config.context, data, length);
}

int bvstk_line_editor_write(
    bvstk_line_editor_t *editor,
    const void *data,
    size_t length)
{
    return editor_write_all(editor, data, length);
}

int bvstk_line_editor_write_text(
    bvstk_line_editor_t *editor,
    const char *text)
{
    if (text == NULL) {
        return -1;
    }
    return editor_write_all(editor, text, strlen(text));
}

void bvstk_line_editor_beep(bvstk_line_editor_t *editor)
{
    (void)bvstk_line_editor_write_text(editor, "\a");
}

static void editor_reset_escape(bvstk_line_editor_t *editor)
{
    editor->escape_state = ESCAPE_NONE;
    editor->escape_parameter_length = 0U;
    editor->escape_parameters[0] = '\0';
}

void bvstk_line_editor_reset(bvstk_line_editor_t *editor)
{
    if (editor == NULL) {
        return;
    }
    editor->line_length = 0U;
    editor->cursor = 0U;
    editor->line[0] = '\0';
    editor->history_position = -1;
    editor_reset_escape(editor);
}

void bvstk_line_editor_init(
    bvstk_line_editor_t *editor,
    const bvstk_line_editor_config_t *config)
{
    if (editor == NULL) {
        return;
    }
    memset(editor, 0, sizeof(*editor));
    if (config != NULL) {
        editor->config = *config;
    }
    editor->history_position = -1;
    editor_reset_escape(editor);
}

const char *bvstk_line_editor_line(const bvstk_line_editor_t *editor)
{
    return editor != NULL ? editor->line : "";
}

size_t bvstk_line_editor_line_length(const bvstk_line_editor_t *editor)
{
    return editor != NULL ? editor->line_length : 0U;
}

size_t bvstk_line_editor_cursor(const bvstk_line_editor_t *editor)
{
    return editor != NULL ? editor->cursor : 0U;
}

int bvstk_line_editor_set_line(
    bvstk_line_editor_t *editor,
    const char *line,
    size_t length)
{
    return bvstk_line_editor_set_state(editor, line, length, length);
}

int bvstk_line_editor_set_state(
    bvstk_line_editor_t *editor,
    const char *line,
    size_t length,
    size_t cursor)
{
    if (editor == NULL || line == NULL ||
        length >= BVSTK_LINE_EDITOR_LINE_MAX || cursor > length) {
        return -1;
    }
    if (length != 0U) {
        memcpy(editor->line, line, length);
    }
    editor->line[length] = '\0';
    editor->line_length = length;
    editor->cursor = cursor;
    return 0;
}

void bvstk_line_editor_mark_edited(bvstk_line_editor_t *editor)
{
    if (editor != NULL) {
        editor->history_position = -1;
    }
}

static void editor_add_history(bvstk_line_editor_t *editor)
{
    size_t index;

    if (editor->line_length == 0U ||
        (editor->history_count != 0U &&
         strcmp(editor->history[editor->history_count - 1U],
                editor->line) == 0)) {
        return;
    }
    if (editor->history_count == BVSTK_LINE_EDITOR_HISTORY_MAX) {
        for (index = 1U; index < BVSTK_LINE_EDITOR_HISTORY_MAX; ++index) {
            memcpy(editor->history[index - 1U],
                   editor->history[index],
                   sizeof(editor->history[index - 1U]));
        }
        --editor->history_count;
    }
    memcpy(editor->history[editor->history_count],
           editor->line,
           editor->line_length + 1U);
    ++editor->history_count;
}

void bvstk_line_editor_redraw(bvstk_line_editor_t *editor)
{
    size_t index;

    if (editor == NULL) {
        return;
    }
    (void)bvstk_line_editor_write_text(editor, "\r\033[2K");
    if (editor->config.prompt != NULL) {
        editor->config.prompt(editor->config.context);
    }
    if (editor->line_length != 0U) {
        (void)bvstk_line_editor_write(editor,
                                      editor->line,
                                      editor->line_length);
    }
    for (index = editor->cursor; index < editor->line_length; ++index) {
        (void)bvstk_line_editor_write_text(editor, "\033[D");
    }
}

static void editor_move_left(bvstk_line_editor_t *editor, int word)
{
    size_t old_cursor;

    old_cursor = editor->cursor;
    if (editor->cursor == 0U) {
        bvstk_line_editor_beep(editor);
        return;
    }
    if (!word) {
        --editor->cursor;
    } else {
        while (editor->cursor > 0U &&
               isspace((unsigned char)editor->line[editor->cursor - 1U])) {
            --editor->cursor;
        }
        while (editor->cursor > 0U &&
               !isspace((unsigned char)editor->line[editor->cursor - 1U])) {
            --editor->cursor;
        }
    }
    while (old_cursor-- > editor->cursor) {
        (void)bvstk_line_editor_write_text(editor, "\033[D");
    }
}

static void editor_move_right(bvstk_line_editor_t *editor, int word)
{
    size_t old_cursor;

    old_cursor = editor->cursor;
    if (editor->cursor >= editor->line_length) {
        bvstk_line_editor_beep(editor);
        return;
    }
    if (!word) {
        ++editor->cursor;
    } else {
        while (editor->cursor < editor->line_length &&
               !isspace((unsigned char)editor->line[editor->cursor])) {
            ++editor->cursor;
        }
        while (editor->cursor < editor->line_length &&
               isspace((unsigned char)editor->line[editor->cursor])) {
            ++editor->cursor;
        }
    }
    while (old_cursor++ < editor->cursor) {
        (void)bvstk_line_editor_write_text(editor, "\033[C");
    }
}

static void editor_history_up(bvstk_line_editor_t *editor)
{
    if (editor->history_count == 0U) {
        bvstk_line_editor_beep(editor);
        return;
    }
    if (editor->history_position < 0) {
        memcpy(editor->history_scratch,
               editor->line,
               editor->line_length + 1U);
        editor->history_scratch_length = editor->line_length;
        editor->history_position = (int)editor->history_count - 1;
    } else if (editor->history_position > 0) {
        --editor->history_position;
    } else {
        bvstk_line_editor_beep(editor);
        return;
    }
    (void)bvstk_line_editor_set_line(
        editor,
        editor->history[editor->history_position],
        strlen(editor->history[editor->history_position]));
    bvstk_line_editor_redraw(editor);
}

static void editor_history_down(bvstk_line_editor_t *editor)
{
    if (editor->history_position < 0) {
        bvstk_line_editor_beep(editor);
        return;
    }
    if ((size_t)(editor->history_position + 1) < editor->history_count) {
        ++editor->history_position;
        (void)bvstk_line_editor_set_line(
            editor,
            editor->history[editor->history_position],
            strlen(editor->history[editor->history_position]));
    } else {
        editor->history_position = -1;
        (void)bvstk_line_editor_set_line(editor,
                                         editor->history_scratch,
                                         editor->history_scratch_length);
    }
    bvstk_line_editor_redraw(editor);
}

static void editor_insert(bvstk_line_editor_t *editor, unsigned char character)
{
    size_t tail;

    if (editor->line_length + 1U >= BVSTK_LINE_EDITOR_LINE_MAX) {
        bvstk_line_editor_beep(editor);
        return;
    }
    tail = editor->line_length - editor->cursor;
    memmove(editor->line + editor->cursor + 1U,
            editor->line + editor->cursor,
            tail);
    editor->line[editor->cursor] = (char)character;
    ++editor->line_length;
    ++editor->cursor;
    editor->line[editor->line_length] = '\0';
    bvstk_line_editor_mark_edited(editor);
    (void)bvstk_line_editor_write(editor, &character, 1U);
    if (tail != 0U) {
        (void)bvstk_line_editor_write(editor,
                                      editor->line + editor->cursor,
                                      tail);
        while (tail-- != 0U) {
            (void)bvstk_line_editor_write_text(editor, "\033[D");
        }
    }
}

static void editor_backspace(bvstk_line_editor_t *editor)
{
    size_t tail;

    if (editor->cursor == 0U) {
        bvstk_line_editor_beep(editor);
        return;
    }
    --editor->cursor;
    tail = editor->line_length - editor->cursor - 1U;
    memmove(editor->line + editor->cursor,
            editor->line + editor->cursor + 1U,
            tail);
    --editor->line_length;
    editor->line[editor->line_length] = '\0';
    bvstk_line_editor_mark_edited(editor);
    bvstk_line_editor_redraw(editor);
}

static void editor_delete(bvstk_line_editor_t *editor)
{
    size_t tail;

    if (editor->cursor >= editor->line_length) {
        bvstk_line_editor_beep(editor);
        return;
    }
    tail = editor->line_length - editor->cursor - 1U;
    memmove(editor->line + editor->cursor,
            editor->line + editor->cursor + 1U,
            tail);
    --editor->line_length;
    editor->line[editor->line_length] = '\0';
    bvstk_line_editor_mark_edited(editor);
    bvstk_line_editor_redraw(editor);
}

static void editor_cut_range(bvstk_line_editor_t *editor,
                              size_t start,
                              size_t end)
{
    if (editor == NULL || start >= end || end > editor->line_length) {
        return;
    }
    editor->cut_length = end - start;
    if (editor->cut_length >= BVSTK_LINE_EDITOR_LINE_MAX) {
        editor->cut_length = BVSTK_LINE_EDITOR_LINE_MAX - 1U;
    }
    memcpy(editor->cut_buffer,
           editor->line + start,
           editor->cut_length);
    editor->cut_buffer[editor->cut_length] = '\0';
    memmove(editor->line + start,
            editor->line + end,
            editor->line_length - end);
    editor->line_length -= end - start;
    editor->cursor = start;
    editor->line[editor->line_length] = '\0';
    bvstk_line_editor_mark_edited(editor);
    bvstk_line_editor_redraw(editor);
}

static void editor_delete_previous_word(bvstk_line_editor_t *editor)
{
    size_t start;

    if (editor == NULL || editor->cursor == 0U) {
        return;
    }
    start = editor->cursor;
    while (start > 0U && isspace((unsigned char)editor->line[start - 1U])) {
        --start;
    }
    while (start > 0U && !isspace((unsigned char)editor->line[start - 1U])) {
        --start;
    }
    editor_cut_range(editor, start, editor->cursor);
}

static void editor_delete_next_word(bvstk_line_editor_t *editor)
{
    size_t end;

    if (editor == NULL || editor->cursor >= editor->line_length) {
        return;
    }
    end = editor->cursor;
    while (end < editor->line_length &&
           isspace((unsigned char)editor->line[end])) {
        ++end;
    }
    while (end < editor->line_length &&
           !isspace((unsigned char)editor->line[end])) {
        ++end;
    }
    editor_cut_range(editor, editor->cursor, end);
}

static void editor_paste_cut_buffer(bvstk_line_editor_t *editor)
{
    size_t tail;

    if (editor == NULL || editor->cut_length == 0U ||
        editor->line_length + editor->cut_length >=
            BVSTK_LINE_EDITOR_LINE_MAX) {
        return;
    }
    tail = editor->line_length - editor->cursor;
    memmove(editor->line + editor->cursor + editor->cut_length,
            editor->line + editor->cursor,
            tail);
    memcpy(editor->line + editor->cursor,
           editor->cut_buffer,
           editor->cut_length);
    editor->line_length += editor->cut_length;
    editor->cursor += editor->cut_length;
    editor->line[editor->line_length] = '\0';
    bvstk_line_editor_mark_edited(editor);
    bvstk_line_editor_redraw(editor);
}

int bvstk_line_editor_replace(
    bvstk_line_editor_t *editor,
    size_t token_start,
    size_t prefix_length,
    const char *replacement,
    size_t replacement_length,
    int append_space)
{
    size_t tail;
    size_t new_length;

    if (editor == NULL || replacement == NULL ||
        token_start > editor->cursor ||
        prefix_length != editor->cursor - token_start) {
        return -1;
    }
    tail = editor->line_length - editor->cursor;
    new_length = token_start + replacement_length + tail;
    if (append_space && replacement_length != 0U &&
        replacement[replacement_length - 1U] != '/') {
        ++new_length;
    }
    if (new_length >= BVSTK_LINE_EDITOR_LINE_MAX) {
        bvstk_line_editor_beep(editor);
        return -1;
    }
    memmove(editor->line + token_start + replacement_length,
            editor->line + editor->cursor,
            tail);
    memcpy(editor->line + token_start, replacement, replacement_length);
    editor->line_length = token_start + replacement_length + tail;
    editor->cursor = token_start + replacement_length;
    if (append_space && replacement_length != 0U &&
        replacement[replacement_length - 1U] != '/') {
        memmove(editor->line + editor->cursor + 1U,
                editor->line + editor->cursor,
                tail);
        editor->line[editor->cursor] = ' ';
        ++editor->line_length;
        ++editor->cursor;
    }
    editor->line[editor->line_length] = '\0';
    bvstk_line_editor_mark_edited(editor);
    bvstk_line_editor_redraw(editor);
    return 0;
}

static size_t common_prefix_length(
    const bvstk_line_editor_completion_t *completion)
{
    size_t length;
    size_t index;

    if (completion == NULL || completion->match_count == 0U) {
        return 0U;
    }
    length = strlen(completion->candidates[0]);
    for (index = 1U; index < completion->match_count; ++index) {
        size_t current = 0U;

        while (current < length &&
               completion->candidates[index][current] != '\0' &&
               tolower((unsigned char)completion->candidates[0][current]) ==
                   tolower((unsigned char)completion->candidates[index][current])) {
            ++current;
        }
        length = current;
        if (length == 0U) {
            break;
        }
    }
    return length;
}

static void editor_complete(bvstk_line_editor_t *editor)
{
    bvstk_line_editor_completion_t completion;
    size_t index;

    if (editor->config.tab != NULL) {
        (void)editor->config.tab(editor->config.context, editor);
        return;
    }
    memset(&completion, 0, sizeof(completion));
    if (editor->config.complete == NULL ||
        editor->config.complete(editor->config.context,
                                editor->line,
                                editor->line_length,
                                editor->cursor,
                                &completion) == 0 ||
        completion.match_count == 0U ||
        completion.match_count > BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES) {
        bvstk_line_editor_beep(editor);
        return;
    }
    if (completion.token_start > editor->cursor ||
        completion.token_prefix_length !=
            editor->cursor - completion.token_start) {
        bvstk_line_editor_beep(editor);
        return;
    }
    if (completion.match_count == 1U) {
        (void)bvstk_line_editor_replace(
            editor,
            completion.token_start,
            completion.token_prefix_length,
            completion.candidates[0],
            strlen(completion.candidates[0]),
            1);
        return;
    }

    {
        size_t prefix_length = common_prefix_length(&completion);

        if (prefix_length > completion.token_prefix_length) {
            (void)bvstk_line_editor_replace(
                editor,
                completion.token_start,
                completion.token_prefix_length,
                completion.candidates[0],
                prefix_length,
                0);
        }
    }
    (void)bvstk_line_editor_write_text(editor, "\r\n");
    for (index = 0U; index < completion.match_count; ++index) {
        (void)bvstk_line_editor_write_text(editor,
                                            completion.candidates[index]);
        (void)bvstk_line_editor_write_text(editor, "  ");
    }
    (void)bvstk_line_editor_write_text(editor, "\r\n");
    bvstk_line_editor_redraw(editor);
}

static int parse_parameter(const char *parameters, size_t length, size_t index)
{
    size_t current = 0U;
    size_t parameter_index = 0U;
    int value = 0;
    int have_value = 0;

    while (current < length) {
        char character = parameters[current++];

        if (character == ';') {
            if (parameter_index == index) {
                return have_value ? value : 0;
            }
            ++parameter_index;
            value = 0;
            have_value = 0;
        } else if (character >= '0' && character <= '9') {
            if (value < 1000) {
                value = value * 10 + (character - '0');
            }
            have_value = 1;
        }
    }
    return parameter_index == index && have_value ? value : -1;
}

static void editor_handle_csi_final(
    bvstk_line_editor_t *editor,
    unsigned char final_character)
{
    int first = parse_parameter(editor->escape_parameters,
                                editor->escape_parameter_length,
                                0U);
    int modifier = parse_parameter(editor->escape_parameters,
                                   editor->escape_parameter_length,
                                   1U);
    int word = modifier == 3 || modifier == 5 || first == 5;

    if (final_character == 'A') {
        editor_history_up(editor);
    } else if (final_character == 'B') {
        editor_history_down(editor);
    } else if (final_character == 'C') {
        editor_move_right(editor, word);
    } else if (final_character == 'D') {
        editor_move_left(editor, word);
    } else if (final_character == 'H') {
        editor->cursor = 0U;
        bvstk_line_editor_redraw(editor);
    } else if (final_character == 'F') {
        editor->cursor = editor->line_length;
        bvstk_line_editor_redraw(editor);
    } else if (final_character == '~') {
        if (first == 1 || first == 7) {
            editor->cursor = 0U;
            bvstk_line_editor_redraw(editor);
        } else if (first == 4 || first == 8) {
            editor->cursor = editor->line_length;
            bvstk_line_editor_redraw(editor);
        } else if (first == 3) {
            editor_delete(editor);
        } else {
            bvstk_line_editor_beep(editor);
        }
    } else {
        bvstk_line_editor_beep(editor);
    }
}

static void editor_handle_escape_byte(
    bvstk_line_editor_t *editor,
    unsigned char character)
{
    if (editor->escape_state == ESCAPE_STARTED) {
        if (character == '[') {
            editor->escape_state = ESCAPE_CSI;
            editor->escape_parameter_length = 0U;
        } else if (character == 'O') {
            editor->escape_state = ESCAPE_SS3;
        } else if (character == 'b' || character == 'B') {
            editor_move_left(editor, 1);
            editor_reset_escape(editor);
        } else if (character == 'f' || character == 'F') {
            editor_move_right(editor, 1);
            editor_reset_escape(editor);
        } else if (character == 'd' || character == 'D') {
            editor_delete_next_word(editor);
            editor_reset_escape(editor);
        } else if (character == 0x7FU || character == 0x08U) {
            editor_delete_previous_word(editor);
            editor_reset_escape(editor);
        } else {
            editor_reset_escape(editor);
            bvstk_line_editor_beep(editor);
        }
        return;
    }
    if (editor->escape_state == ESCAPE_CSI) {
        if ((character >= '0' && character <= '9') ||
            character == ';' || character == '?') {
            if (editor->escape_parameter_length + 1U <
                sizeof(editor->escape_parameters)) {
                editor->escape_parameters[editor->escape_parameter_length++] =
                    (char)character;
                editor->escape_parameters[editor->escape_parameter_length] =
                    '\0';
            }
            return;
        }
        editor_handle_csi_final(editor, character);
        editor_reset_escape(editor);
        return;
    }
    if (editor->escape_state == ESCAPE_SS3) {
        if (character == 'A') {
            editor_history_up(editor);
        } else if (character == 'B') {
            editor_history_down(editor);
        } else if (character == 'C') {
            editor_move_right(editor, 0);
        } else if (character == 'D') {
            editor_move_left(editor, 0);
        } else if (character == 'H') {
            editor->cursor = 0U;
            bvstk_line_editor_redraw(editor);
        } else if (character == 'F') {
            editor->cursor = editor->line_length;
            bvstk_line_editor_redraw(editor);
        } else {
            bvstk_line_editor_beep(editor);
        }
        editor_reset_escape(editor);
    }
}

static int editor_submit(bvstk_line_editor_t *editor)
{
    int result = BVSTK_LINE_EDITOR_SUBMIT_PROMPT;

    (void)bvstk_line_editor_write_text(editor, "\r\n");
    editor_add_history(editor);
    if (editor->config.submit != NULL) {
        result = editor->config.submit(editor->config.context,
                                       editor->line,
                                       editor->line_length);
    }
    bvstk_line_editor_reset(editor);
    if (result == BVSTK_LINE_EDITOR_SUBMIT_PROMPT) {
        if (editor->config.prompt != NULL) {
            editor->config.prompt(editor->config.context);
        }
    }
    return result == BVSTK_LINE_EDITOR_SUBMIT_STOP || result < 0;
}

int bvstk_line_editor_handle_byte(
    bvstk_line_editor_t *editor,
    unsigned char character)
{
    if (editor == NULL) {
        return 1;
    }
    if (editor->escape_state != ESCAPE_NONE) {
        editor_handle_escape_byte(editor, character);
        return 0;
    }
    if (character == 0x1BU) {
        editor->escape_state = ESCAPE_STARTED;
        editor->escape_parameter_length = 0U;
    } else if (character == '\r' || character == '\n') {
        return editor_submit(editor);
    } else if (character == 0x01U) {
        editor->cursor = 0U;
        bvstk_line_editor_redraw(editor);
    } else if (character == 0x02U) {
        editor_move_left(editor, 0);
    } else if (character == 0x05U) {
        editor->cursor = editor->line_length;
        bvstk_line_editor_redraw(editor);
    } else if (character == 0x06U) {
        editor_move_right(editor, 0);
    } else if (character == 0x10U) {
        editor_history_up(editor);
    } else if (character == 0x0EU) {
        editor_history_down(editor);
    } else if (character == 0x0BU) {
        editor_cut_range(editor, editor->cursor, editor->line_length);
    } else if (character == 0x0CU) {
        bvstk_line_editor_redraw(editor);
    } else if (character == 0x15U) {
        editor_cut_range(editor, 0U, editor->cursor);
    } else if (character == 0x17U) {
        editor_delete_previous_word(editor);
    } else if (character == 0x19U) {
        editor_paste_cut_buffer(editor);
    } else if (character == 0x03U) {
        bvstk_line_editor_reset(editor);
        (void)bvstk_line_editor_write_text(editor, "^C\r\n");
        if (editor->config.prompt != NULL) {
            editor->config.prompt(editor->config.context);
        }
    } else if (character == 0x04U) {
        if (editor->line_length == 0U && editor->config.eof_on_empty) {
            return 1;
        }
        editor_delete(editor);
    } else if (character == 0x08U || character == 0x7FU) {
        editor_backspace(editor);
    } else if (character == '\t') {
        editor_complete(editor);
    } else if (isprint(character)) {
        editor_insert(editor, character);
    }
    return 0;
}
