#ifndef BVSTK_LINE_EDITOR_H
#define BVSTK_LINE_EDITOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BVSTK_LINE_EDITOR_LINE_MAX 512U
#define BVSTK_LINE_EDITOR_HISTORY_MAX 16U
#define BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES 16U
#define BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX 256U

typedef struct bvstk_line_editor bvstk_line_editor_t;
struct bvstk_line_editor_completion;

typedef int (*bvstk_line_editor_write_fn)(
    void *context,
    const void *data,
    size_t length);

typedef void (*bvstk_line_editor_prompt_fn)(void *context);

/* Return one of BVSTK_LINE_EDITOR_SUBMIT_* values below. */
typedef int (*bvstk_line_editor_submit_fn)(
    void *context,
    const char *line,
    size_t length);

typedef int (*bvstk_line_editor_complete_fn)(
    void *context,
    const char *line,
    size_t length,
    size_t cursor,
    struct bvstk_line_editor_completion *result);

/* Optional transport-specific completion hook. It may update and redraw the
 * editor itself, which is useful when a completion backend has special path
 * replacement rules. */
typedef int (*bvstk_line_editor_tab_fn)(
    void *context,
    bvstk_line_editor_t *editor);

typedef struct bvstk_line_editor_completion {
    size_t token_start;
    size_t token_prefix_length;
    size_t match_count;
    char candidates[BVSTK_LINE_EDITOR_COMPLETION_MAX_MATCHES]
                   [BVSTK_LINE_EDITOR_COMPLETION_WORD_MAX];
} bvstk_line_editor_completion_t;

enum {
    BVSTK_LINE_EDITOR_SUBMIT_PROMPT = 0,
    BVSTK_LINE_EDITOR_SUBMIT_DEFER_PROMPT = 1,
    BVSTK_LINE_EDITOR_SUBMIT_STOP = 2
};

typedef struct {
    void *context;
    bvstk_line_editor_write_fn write;
    bvstk_line_editor_prompt_fn prompt;
    bvstk_line_editor_submit_fn submit;
    bvstk_line_editor_complete_fn complete;
    bvstk_line_editor_tab_fn tab;
    int eof_on_empty;
} bvstk_line_editor_config_t;

struct bvstk_line_editor {
    bvstk_line_editor_config_t config;
    char line[BVSTK_LINE_EDITOR_LINE_MAX];
    size_t line_length;
    size_t cursor;
    char history[BVSTK_LINE_EDITOR_HISTORY_MAX]
                [BVSTK_LINE_EDITOR_LINE_MAX];
    size_t history_count;
    int history_position;
    char history_scratch[BVSTK_LINE_EDITOR_LINE_MAX];
    size_t history_scratch_length;
    char cut_buffer[BVSTK_LINE_EDITOR_LINE_MAX];
    size_t cut_length;
    unsigned int escape_state;
    char escape_parameters[16];
    size_t escape_parameter_length;
};

void bvstk_line_editor_init(
    bvstk_line_editor_t *editor,
    const bvstk_line_editor_config_t *config);

void bvstk_line_editor_reset(bvstk_line_editor_t *editor);

int bvstk_line_editor_handle_byte(
    bvstk_line_editor_t *editor,
    unsigned char character);

const char *bvstk_line_editor_line(const bvstk_line_editor_t *editor);
size_t bvstk_line_editor_line_length(const bvstk_line_editor_t *editor);
size_t bvstk_line_editor_cursor(const bvstk_line_editor_t *editor);

int bvstk_line_editor_set_line(
    bvstk_line_editor_t *editor,
    const char *line,
    size_t length);

int bvstk_line_editor_set_state(
    bvstk_line_editor_t *editor,
    const char *line,
    size_t length,
    size_t cursor);

void bvstk_line_editor_mark_edited(bvstk_line_editor_t *editor);

int bvstk_line_editor_replace(
    bvstk_line_editor_t *editor,
    size_t token_start,
    size_t prefix_length,
    const char *replacement,
    size_t replacement_length,
    int append_space);

void bvstk_line_editor_redraw(bvstk_line_editor_t *editor);
void bvstk_line_editor_beep(bvstk_line_editor_t *editor);

int bvstk_line_editor_write(
    bvstk_line_editor_t *editor,
    const void *data,
    size_t length);

int bvstk_line_editor_write_text(
    bvstk_line_editor_t *editor,
    const char *text);

#ifdef __cplusplus
}
#endif

#endif
