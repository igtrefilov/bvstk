#include "apps/freertos/console/console_common.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define PROMPT_MAX 80

void console_print_banner(int fd)
{
    static const char *const lines[] = {
        " _______   __     __   ______  ________  __    __       \r\n",
        "|       \\ |  \\   |  \\ /      \\|        \\|  \\  /  \\      \r\n",
        "| $$$$$$$\\| $$   | $$|  $$$$$$\\\\$$$$$$$$| $$ /  $$      \r\n",
        "| $$__/ $$| $$   | $$| $$___\\$$  | $$   | $$/  $$       \r\n",
        "| $$    $$ \\$$\\ /  $$ \\$$    \\   | $$   | $$  $$        \r\n",
        "| $$$$$$$\\  \\$$\\  $$  _\\$$$$$$\\  | $$   | $$$$$\\        \r\n",
        "| $$__/ $$   \\$$ $$  |  \\__| $$  | $$   | $$ \\$$\\       \r\n",
        "| $$    $$    \\$$$    \\$$    $$  | $$   | $$  \\$$\\      \r\n",
        " \\$$$$$$$      \\$      \\$$$$$$    \\$$    \\$$   \\$$      \r\n",
        "\r\n",
        "+----------------+-----------------------------+\r\n",
        "| Utility        | Description                 |\r\n",
        "+----------------+-----------------------------+\r\n",
        "| fs             | Filesystem commands         |\r\n",
        "| tar            | Tar archive utility         |\r\n",
        "| ip             | Network configuration       |\r\n",
        "| smi            | MDIO/SMI access             |\r\n",
        "| spi            | SPI PL master shell         |\r\n",
        "| mem            | Memory info/tools           |\r\n",
        "| i2c            | I2C device shell            |\r\n",
        "| help           | List available utilities    |\r\n",
        "| reboot         | Reboot via watchdog         |\r\n",
        "| quit / exit    | Close session               |\r\n",
        "+----------------+-----------------------------+\r\n",
        "\r\n",
        "Tip: use `<utility> -h` for help.\r\n",
        "\r\n",
        NULL
    };

    for (const char *const *p = lines; *p; ++p) {
        write_str(fd, *p);
    }
}

void console_session_init(console_session_t *s)
{
    if (!s) return;
    s->fs_ctx = NULL;
    s->fs_label = NULL;
    console_session_set_fs(s, fs_device_default()->ctx, fs_device_default()->label);
}

const fs_shared_ctx_t *console_session_get_fs(const console_session_t *s)
{
    return (s ? s->fs_ctx : NULL);
}

const char *console_session_get_root(const console_session_t *s)
{
    if (s && s->fs_ctx && s->fs_ctx->root) return s->fs_ctx->root;
    return "/";
}

const char *console_session_get_label(const console_session_t *s)
{
    return (s && s->fs_label) ? s->fs_label : "FS";
}

void console_session_set_fs(console_session_t *s, const fs_shared_ctx_t *ctx, const char *label)
{
    if (!s || !ctx) return;
    s->fs_ctx = ctx;
    s->fs_label = label ? label : "FS";
    const char *root = ctx->root ? ctx->root : "/";
    strncpy(s->cwd, root, CONSOLE_CWD_LEN - 1);
    s->cwd[CONSOLE_CWD_LEN - 1] = '\0';
}

void write_str(int fd, const char *s)
{
    (void)console_stream_write(fd, s, strlen(s));
}

void console_print_prompt(int fd, const console_session_t *session)
{
    char prompt[PROMPT_MAX];
    const char *root = console_session_get_root(session);
    const char *cwd = (session && session->cwd[0]) ? session->cwd : root;
    const size_t root_len = strlen(root);
    const char *rel = cwd;
    if (root_len && strncmp(cwd, root, root_len) == 0) {
        rel = cwd + root_len;
        if (*rel == '/') rel++;
    }
    const char *label = console_session_get_label(session);
    if (rel && *rel) {
        snprintf(prompt, sizeof(prompt), "Zynq/%s:%s> ", label, rel);
    } else {
        snprintf(prompt, sizeof(prompt), "Zynq/%s> ", label);
    }
    write_str(fd, prompt);
}
