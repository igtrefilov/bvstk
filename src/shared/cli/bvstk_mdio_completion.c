#include "shared/cli/bvstk_mdio_completion.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static void add(const char *prefix, const char *word, bvstk_mdio_completion_result_t *r)
{
    if (r->match_count == BVSTK_MDIO_COMPLETION_MAX_MATCHES ||
        strncasecmp(prefix, word, strlen(prefix))) return;
    snprintf(r->candidates[r->match_count++], BVSTK_MDIO_COMPLETION_WORD_MAX, "%s", word);
}
static void words(const char *prefix, const char *list, bvstk_mdio_completion_result_t *r)
{
    while (*list) {
        char word[BVSTK_MDIO_COMPLETION_WORD_MAX];
        size_t n = 0;
        while (*list == ' ') ++list;
        while (*list && *list != ' ') { if (n + 1U < sizeof(word)) word[n++] = *list; ++list; }
        word[n] = '\0';
        if (n) add(prefix, word, r);
    }
}
int bvstk_mdio_complete_line(const char *line, size_t length, size_t cursor,
    const bvstk_mdio_completion_device_t *devices, size_t count,
    bvstk_mdio_completion_result_t *r)
{
    char tokens[6][64], prefix[64];
    size_t start, pos = 0, n = 0, len;
    const char *list = "";
    if (!r) return 0;
    memset(r, 0, sizeof(*r));
    if (!line || cursor > length) return 0;
    start = cursor;
    while (start && line[start - 1U] != ' ' && line[start - 1U] != '\t') --start;
    len = cursor - start;
    if (len >= sizeof(prefix)) return 0;
    memcpy(prefix, line + start, len); prefix[len] = '\0';
    while (pos < start) {
        size_t begin;
        while (pos < start && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        if (pos == start) break;
        begin = pos;
        while (pos < start && line[pos] != ' ' && line[pos] != '\t') ++pos;
        len = pos - begin;
        if (n == 6U || len >= sizeof(tokens[0])) return 0;
        memcpy(tokens[n], line + begin, len); tokens[n++][len] = '\0';
    }
    if (!n && !strcasecmp(prefix, "mdio")) {
        strcpy(tokens[n++], "mdio"); start = cursor; prefix[0] = '\0';
    }
    if (!n || strcasecmp(tokens[0], "mdio")) return 0;
    r->token_start = start; r->token_prefix_length = cursor - start;
    if (n == 1) {
        words(prefix, "list status -h --help", r);
        for (size_t i = 0; devices && i < count; ++i) {
            char phy[16];
            snprintf(phy, sizeof(phy), "@%u", devices[i].phy_addr);
            add(prefix, prefix[0] == '@' ? phy : devices[i].name, r);
        }
    } else if (n == 2 && strcasecmp(tokens[1], "list") && strcasecmp(tokens[1], "status"))
        list = "info r w cache policy phy autopoll settings save";
    else if (n == 3) {
        if (!strcasecmp(tokens[2], "policy")) list = "show set whitelist blacklist";
        if (!strcasecmp(tokens[2], "cache")) list = "show refresh invalidate";
        if (!strcasecmp(tokens[2], "settings")) list = "show set del clear";
        if (!strcasecmp(tokens[2], "autopoll")) list = "show on off reg_delay cycle_delay regs";
    } else if (n == 4 && !strcasecmp(tokens[2], "policy")) {
        if (!strcasecmp(tokens[3], "show")) list = "rules whitelist blacklist";
        if (!strcasecmp(tokens[3], "set")) list = "whitelist blacklist";
        if (!strcasecmp(tokens[3], "whitelist") || !strcasecmp(tokens[3], "blacklist")) list = "add del delete clear";
    }
    words(prefix, list, r);
    return r->match_count != 0;
}
