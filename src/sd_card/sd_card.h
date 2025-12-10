#ifndef SD_CARD_H
#define SD_CARD_H

#include "xsdps.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "task.h"

#define SD_ROOT "0:/"
#define SD_NAME_MAX 64

typedef struct {
	u32 block_size;
	u64 block_count;
	u64 capacity_bytes;
	int mounted;
} sd_card_info_t;

int start_sd_card(void);              // spawn background task that mounts SD and FatFs
int sd_card_get_info(sd_card_info_t *info);
int sd_card_ls(int fd);               // legacy root-only list
int sd_card_cat(const char *path, int fd);
int sd_card_write_text(const char *path, const char *text, int append);

/* New generic FS helpers for console shell */
int sd_fs_ls(const char *path, int fd);
int sd_fs_cat(const char *path, int fd);
int sd_fs_touch(const char *path);
int sd_fs_mkdir(const char *path);
int sd_fs_rm(const char *path);
int sd_fs_is_dir(const char *path);
int sd_fs_complete(const char *dir, const char *prefix, char results[][SD_NAME_MAX], int max_results, int *out_count);

#endif // SD_CARD_H
