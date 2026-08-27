#include <stdio.h>
#include <string.h>

#include "shared/cli/bvstk_line_editor.h"

typedef struct {
    char output[8192];
    size_t output_length;
    char submitted[8][BVSTK_LINE_EDITOR_LINE_MAX];
    size_t submitted_count;
} test_context_t;

static int check(int condition, const char *expression, int line)
{
    if (!condition) {
        fprintf(stderr, "line editor check failed at %d: %s\n", line, expression);
        return 0;
    }
    return 1;
}

#define CHECK(condition) \
    do { \
        if (!check((condition), #condition, __LINE__)) return 1; \
    } while (0)

static int test_write(void *context, const void *data, size_t length)
{
    test_context_t *test = (test_context_t *)context;

    if (test == NULL || data == NULL ||
        test->output_length + length >= sizeof(test->output)) {
        return -1;
    }
    memcpy(test->output + test->output_length, data, length);
    test->output_length += length;
    test->output[test->output_length] = '\0';
    return 0;
}

static void test_prompt(void *context)
{
    static const char prompt[] = "> ";

    (void)test_write(context, prompt, sizeof(prompt) - 1U);
}

static int test_submit(void *context, const char *line, size_t length)
{
    test_context_t *test = (test_context_t *)context;

    if (test == NULL || line == NULL ||
        test->submitted_count >= sizeof(test->submitted) /
            sizeof(test->submitted[0]) ||
        length >= sizeof(test->submitted[0])) {
        return BVSTK_LINE_EDITOR_SUBMIT_STOP;
    }
    memcpy(test->submitted[test->submitted_count], line, length);
    test->submitted[test->submitted_count][length] = '\0';
    ++test->submitted_count;
    return BVSTK_LINE_EDITOR_SUBMIT_PROMPT;
}

static int test_complete(void *context,
                         const char *line,
                         size_t length,
                         size_t cursor,
                         bvstk_line_editor_completion_t *result)
{
    (void)context;
    if (line == NULL || result == NULL || cursor != length ||
        length != 2U || strncmp(line, "fo", length) != 0) {
        return 0;
    }
    memset(result, 0, sizeof(*result));
    result->token_start = 0U;
    result->token_prefix_length = length;
    result->match_count = 1U;
    strcpy(result->candidates[0], "foo");
    return 1;
}

static void feed(bvstk_line_editor_t *editor,
                 const unsigned char *data,
                 size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        (void)bvstk_line_editor_handle_byte(editor, data[index]);
    }
}

static void feed_text(bvstk_line_editor_t *editor, const char *text)
{
    feed(editor, (const unsigned char *)text, strlen(text));
}

static void init_editor(bvstk_line_editor_t *editor, test_context_t *context)
{
    bvstk_line_editor_config_t config;

    memset(context, 0, sizeof(*context));
    memset(&config, 0, sizeof(config));
    config.context = context;
    config.write = test_write;
    config.prompt = test_prompt;
    config.submit = test_submit;
    config.complete = test_complete;
    bvstk_line_editor_init(editor, &config);
}

int main(void)
{
    bvstk_line_editor_t editor;
    test_context_t context;
    static const unsigned char ctrl_left[] = "\033[1;5D";
    static const unsigned char ctrl_right[] = "\033[1;5C";
    static const unsigned char delete_key[] = "\033[3~";

    init_editor(&editor, &context);
    feed_text(&editor, "echo alpha beta");
    feed(&editor, ctrl_left, sizeof(ctrl_left) - 1U);
    CHECK(bvstk_line_editor_cursor(&editor) <
          bvstk_line_editor_line_length(&editor));
    feed(&editor, ctrl_right, sizeof(ctrl_right) - 1U);
    CHECK(bvstk_line_editor_cursor(&editor) ==
          bvstk_line_editor_line_length(&editor));
    feed(&editor, ctrl_left, sizeof(ctrl_left) - 1U);
    feed(&editor, (const unsigned char *)"\013", 1U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "echo alpha ") == 0);
    feed(&editor, (const unsigned char *)"\027", 1U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "echo ") == 0);
    feed(&editor, (const unsigned char *)"\031", 1U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "echo alpha ") == 0);

    bvstk_line_editor_reset(&editor);
    feed_text(&editor, "echo ");
    feed(&editor, (const unsigned char *)"\033b", 2U);
    CHECK(bvstk_line_editor_cursor(&editor) == 0U);
    feed(&editor, (const unsigned char *)"\033f", 2U);
    CHECK(bvstk_line_editor_cursor(&editor) == 5U);

    bvstk_line_editor_reset(&editor);
    feed_text(&editor, "abc");
    feed(&editor, (const unsigned char *)"\001", 1U);
    feed(&editor, delete_key, sizeof(delete_key) - 1U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "bc") == 0);

    bvstk_line_editor_reset(&editor);
    feed_text(&editor, "first\nsecond\n");
    CHECK(context.submitted_count == 2U);
    feed(&editor, (const unsigned char *)"\033[A", 3U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "second") == 0);
    feed(&editor, (const unsigned char *)"\033[A", 3U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "first") == 0);
    feed(&editor, (const unsigned char *)"\033[B", 3U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "second") == 0);

    bvstk_line_editor_reset(&editor);
    feed_text(&editor, "partial");
    feed(&editor, (const unsigned char *)"\033[A", 3U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "second") == 0);
    feed(&editor, (const unsigned char *)"\033[B", 3U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "partial") == 0);

    bvstk_line_editor_reset(&editor);
    feed_text(&editor, "fo");
    feed(&editor, (const unsigned char *)"\t", 1U);
    CHECK(strcmp(bvstk_line_editor_line(&editor), "foo ") == 0);

    return 0;
}
