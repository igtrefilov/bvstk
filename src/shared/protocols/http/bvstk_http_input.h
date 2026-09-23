#ifndef BVSTK_HTTP_INPUT_H
#define BVSTK_HTTP_INPUT_H
#include <stddef.h>
#include <stdint.h>
/* Query: 1 found, 0 missing, -1 malformed/duplicate/truncated. */
int bvstk_http_query(const char *url, const char *key, char *out, size_t cap);
int bvstk_http_uint(const char *text, uint32_t max, uint32_t *out);
/* Canonical absolute volume-relative path; rejects traversal and FAT aliases. */
int bvstk_http_path(const char *path);
/* Strict JSON grammar, duplicate keys rejected at every level; integer-only.
 * Root keys must be in allowed (max 32). Strings may use normal JSON escapes;
 * keys must be plain ASCII. Bounded depth and object field count. */
int bvstk_http_json_object(const char *json, const char *const *allowed,
    size_t count, uint32_t required);
uint32_t bvstk_http_revision(const char *canonical_json);
#endif
