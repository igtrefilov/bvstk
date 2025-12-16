#include "tar.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xstatus.h"

#include "../fs/fs_shared.h"

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} tar_hdr_t;

enum { TAR_BLOCK = 512, TAR_NAME_MAX = 100 };

static void tar_zero(void *p, size_t n) { memset(p, 0, n); }

static unsigned tar_checksum(const tar_hdr_t *h)
{
    const unsigned char *p = (const unsigned char *)h;
    unsigned sum = 0;
    for (size_t i = 0; i < TAR_BLOCK; ++i) sum += p[i];
    return sum;
}

static void tar_set_octal(char *dst, size_t dst_sz, uint64_t v)
{
    if (!dst || dst_sz == 0) return;
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%llo", (unsigned long long)v);
    size_t len = strlen(tmp);
    tar_zero(dst, dst_sz);
    if (len + 1 > dst_sz) {
        for (size_t i = 0; i + 1 < dst_sz; ++i) dst[i] = '7';
        dst[dst_sz - 1] = '\0';
        return;
    }
    size_t pad = dst_sz - 1 - len;
    for (size_t i = 0; i < pad; ++i) dst[i] = '0';
    memcpy(dst + pad, tmp, len);
    dst[dst_sz - 1] = '\0';
}

static bool tar_parse_octal(const char *s, size_t n, uint64_t *out)
{
    if (!s || !out) return false;
    uint64_t v = 0;
    size_t i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\0')) i++;
    for (; i < n; ++i) {
        if (s[i] == '\0' || s[i] == ' ') break;
        if (s[i] < '0' || s[i] > '7') return false;
        v = (v << 3) + (uint64_t)(s[i] - '0');
    }
    *out = v;
    return true;
}

static int write_all(tar_write_cb cb, void *user, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t off = 0;
    while (off < len) {
        int w = cb(user, p + off, len - off);
        if (w <= 0) return XST_FAILURE;
        off += (size_t)w;
    }
    return XST_SUCCESS;
}

static int read_exact(tar_read_cb cb, void *user, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t off = 0;
    while (off < len) {
        int r = cb(user, p + off, len - off);
        if (r <= 0) return XST_FAILURE;
        off += (size_t)r;
    }
    return XST_SUCCESS;
}

static bool relpath_safe(const char *name)
{
    if (!name || name[0] == '\0') return false;
    if (name[0] == '/') return false;
    if (strchr(name, ':')) return false;
    if (strstr(name, "..")) return false;
    return true;
}

static bool join_under(const char *base, const char *rel, char *out, size_t out_sz)
{
    if (!base || !rel || !out || out_sz == 0) return false;
    size_t bl = strlen(base);
    size_t rl = strlen(rel);
    if (bl + 1 + rl + 1 > out_sz) return false;
    strncpy(out, base, out_sz - 1);
    out[out_sz - 1] = '\0';
    if (bl && out[bl - 1] != '/') strncat(out, "/", out_sz - strlen(out) - 1);
    strncat(out, rel, out_sz - strlen(out) - 1);
    return true;
}

static int mkdir_p(const char *path)
{
    if (!path) return XST_FAILURE;
    char tmp[FS_PATH_MAX * 2];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    size_t len = strlen(tmp);
    if (len == 0) return XST_SUCCESS;
    if (len == 3 && tmp[1] == ':' && tmp[2] == '/') return XST_SUCCESS;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    len = strlen(tmp);
    if (len == 2 && tmp[1] == ':') return XST_SUCCESS;

    char *p = tmp;
    if (p[0] && p[1] == ':' && p[2] == '/') p += 3;
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            if (strlen(tmp) > 0) {
                FRESULT r = f_mkdir(tmp);
                if (!(r == FR_OK || r == FR_EXIST)) return XST_FAILURE;
            }
            *p = '/';
        }
        p++;
    }
    FRESULT r = f_mkdir(tmp);
    if (!(r == FR_OK || r == FR_EXIST)) return XST_FAILURE;
    return XST_SUCCESS;
}

static int stream_file_as_tar(const char *fat_path, const char *tar_name, uint32_t mtime,
                              tar_write_cb write_cb, void *user)
{
    FILINFO info;
    if (f_stat(fat_path, &info) != FR_OK) return XST_FAILURE;
    if (info.fattrib & AM_DIR) return XST_FAILURE;

    tar_hdr_t h;
    tar_zero(&h, sizeof(h));
    if (strlen(tar_name) >= TAR_NAME_MAX) return XST_FAILURE;
    strncpy(h.name, tar_name, sizeof(h.name) - 1);
    strncpy(h.mode, "0000644", sizeof(h.mode) - 1);
    tar_set_octal(h.uid, sizeof(h.uid), 0);
    tar_set_octal(h.gid, sizeof(h.gid), 0);
    tar_set_octal(h.size, sizeof(h.size), (uint64_t)info.fsize);
    tar_set_octal(h.mtime, sizeof(h.mtime), (uint64_t)mtime);
    memset(h.chksum, ' ', sizeof(h.chksum));
    h.typeflag = '0';
    strncpy(h.magic, "ustar", 6);
    strncpy(h.version, "00", 2);

    unsigned sum = tar_checksum(&h);
    tar_set_octal(h.chksum, sizeof(h.chksum), sum);
    h.chksum[sizeof(h.chksum) - 1] = ' ';

    if (write_all(write_cb, user, &h, TAR_BLOCK) != XST_SUCCESS) return XST_FAILURE;

    FIL f;
    if (f_open(&f, fat_path, FA_READ) != FR_OK) return XST_FAILURE;
    uint8_t buf[1024];
    UINT br = 0;
    uint64_t remaining = info.fsize;
    while (remaining > 0) {
        UINT want = (remaining < sizeof(buf)) ? (UINT)remaining : (UINT)sizeof(buf);
        if (f_read(&f, buf, want, &br) != FR_OK) { f_close(&f); return XST_FAILURE; }
        if (br == 0) break;
        if (write_all(write_cb, user, buf, br) != XST_SUCCESS) { f_close(&f); return XST_FAILURE; }
        remaining -= br;
    }
    f_close(&f);

    size_t pad = (size_t)(TAR_BLOCK - (info.fsize % TAR_BLOCK)) % TAR_BLOCK;
    if (pad) {
        uint8_t zeros[TAR_BLOCK];
        tar_zero(zeros, sizeof(zeros));
        if (write_all(write_cb, user, zeros, pad) != XST_SUCCESS) return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int stream_dir_recursive(const char *fat_dir, const char *tar_prefix, tar_write_cb write_cb, void *user)
{
    DIR d;
    if (f_opendir(&d, fat_dir) != FR_OK) return XST_FAILURE;
    FILINFO fno;
    char name[FF_MAX_LFN + 1];

    while (1) {
        if (f_readdir(&d, &fno) != FR_OK) { f_closedir(&d); return XST_FAILURE; }
        if (fno.fname[0] == '\0') break;
#if FF_USE_LFN
        const char *entry = (fno.fname[0]) ? fno.fname : fno.altname;
#else
        const char *entry = fno.fname;
#endif
        if (!entry || entry[0] == '\0') continue;
        strncpy(name, entry, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;

        char child_fat[FS_PATH_MAX * 2];
        char child_tar[FS_PATH_MAX * 2];
        if (!join_under(fat_dir, name, child_fat, sizeof(child_fat))) { f_closedir(&d); return XST_FAILURE; }
        if (tar_prefix && tar_prefix[0]) {
            if (!join_under(tar_prefix, name, child_tar, sizeof(child_tar))) { f_closedir(&d); return XST_FAILURE; }
        } else {
            strncpy(child_tar, name, sizeof(child_tar) - 1);
            child_tar[sizeof(child_tar) - 1] = '\0';
        }

        if (fno.fattrib & AM_DIR) {
            char dir_name[FS_PATH_MAX * 2];
            snprintf(dir_name, sizeof(dir_name), "%s/", child_tar);
            if (strlen(dir_name) >= TAR_NAME_MAX) { f_closedir(&d); return XST_FAILURE; }

            tar_hdr_t h;
            tar_zero(&h, sizeof(h));
            strncpy(h.name, dir_name, sizeof(h.name) - 1);
            strncpy(h.mode, "0000755", sizeof(h.mode) - 1);
            tar_set_octal(h.uid, sizeof(h.uid), 0);
            tar_set_octal(h.gid, sizeof(h.gid), 0);
            tar_set_octal(h.size, sizeof(h.size), 0);
            tar_set_octal(h.mtime, sizeof(h.mtime), 0);
            memset(h.chksum, ' ', sizeof(h.chksum));
            h.typeflag = '5';
            strncpy(h.magic, "ustar", 6);
            strncpy(h.version, "00", 2);
            unsigned sum = tar_checksum(&h);
            tar_set_octal(h.chksum, sizeof(h.chksum), sum);
            h.chksum[sizeof(h.chksum) - 1] = ' ';
            if (write_all(write_cb, user, &h, TAR_BLOCK) != XST_SUCCESS) { f_closedir(&d); return XST_FAILURE; }

            if (stream_dir_recursive(child_fat, child_tar, write_cb, user) != XST_SUCCESS) { f_closedir(&d); return XST_FAILURE; }
        } else {
            if (stream_file_as_tar(child_fat, child_tar, 0, write_cb, user) != XST_SUCCESS) { f_closedir(&d); return XST_FAILURE; }
        }
    }
    f_closedir(&d);
    return XST_SUCCESS;
}

int tar_stream_dir_fatfs(const fs_shared_ctx_t *ctx, const char *dir_path, tar_write_cb write_cb, void *user)
{
    (void)ctx;
    if (!dir_path || !write_cb) return XST_FAILURE;
    if (stream_dir_recursive(dir_path, "", write_cb, user) != XST_SUCCESS) return XST_FAILURE;
    uint8_t zeros[TAR_BLOCK * 2];
    tar_zero(zeros, sizeof(zeros));
    return write_all(write_cb, user, zeros, sizeof(zeros));
}

int tar_extract_into_dir_fatfs(const fs_shared_ctx_t *ctx, const char *dst_dir_path,
                               tar_read_cb read_cb, void *user)
{
    (void)ctx;
    if (!dst_dir_path || !read_cb) return XST_FAILURE;
    if (mkdir_p(dst_dir_path) != XST_SUCCESS) return XST_FAILURE;

    uint8_t block[TAR_BLOCK];
    int zero_blocks = 0;
    while (1) {
        if (read_exact(read_cb, user, block, sizeof(block)) != XST_SUCCESS) return XST_FAILURE;

        bool all_zero = true;
        for (size_t i = 0; i < sizeof(block); ++i) {
            if (block[i] != 0) { all_zero = false; break; }
        }
        if (all_zero) {
            zero_blocks++;
            if (zero_blocks >= 2) return XST_SUCCESS;
            continue;
        }
        zero_blocks = 0;

        tar_hdr_t h;
        memcpy(&h, block, sizeof(h));
        h.name[sizeof(h.name) - 1] = '\0';
        h.prefix[sizeof(h.prefix) - 1] = '\0';

        char rel[256];
        if (h.prefix[0]) snprintf(rel, sizeof(rel), "%s/%s", h.prefix, h.name);
        else snprintf(rel, sizeof(rel), "%s", h.name);

        if (!relpath_safe(rel)) return XST_FAILURE;

        uint64_t fsize = 0;
        if (!tar_parse_octal(h.size, sizeof(h.size), &fsize)) return XST_FAILURE;
        char type = h.typeflag ? h.typeflag : '0';

        char out_path[FS_PATH_MAX * 2];
        if (!join_under(dst_dir_path, rel, out_path, sizeof(out_path))) return XST_FAILURE;

        if (type == '5') {
            if (mkdir_p(out_path) != XST_SUCCESS) return XST_FAILURE;
            continue;
        }

        char *last_slash = strrchr(out_path, '/');
        if (last_slash) {
            char parent[FS_PATH_MAX * 2];
            size_t plen = (size_t)(last_slash - out_path);
            if (plen >= sizeof(parent)) return XST_FAILURE;
            memcpy(parent, out_path, plen);
            parent[plen] = '\0';
            if (mkdir_p(parent) != XST_SUCCESS) return XST_FAILURE;
        }

        FIL f;
        if (f_open(&f, out_path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return XST_FAILURE;
        uint64_t remaining = fsize;
        uint8_t buf[1024];
        while (remaining > 0) {
            size_t want = (remaining < sizeof(buf)) ? (size_t)remaining : sizeof(buf);
            int rr = read_cb(user, buf, want);
            if (rr <= 0) { f_close(&f); return XST_FAILURE; }
            UINT bw = 0;
            if (f_write(&f, buf, (UINT)rr, &bw) != FR_OK || bw != (UINT)rr) { f_close(&f); return XST_FAILURE; }
            remaining -= (uint64_t)rr;
        }
        f_close(&f);

        size_t pad = (size_t)(TAR_BLOCK - (fsize % TAR_BLOCK)) % TAR_BLOCK;
        if (pad) {
            uint8_t skip[TAR_BLOCK];
            if (read_exact(read_cb, user, skip, pad) != XST_SUCCESS) return XST_FAILURE;
        }
    }
}

