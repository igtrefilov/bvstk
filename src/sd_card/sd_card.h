#ifndef SD_CARD_H
#define SD_CARD_H

#include "xsdps.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct {
	u32 block_size;
	u64 block_count;
	u64 capacity_bytes;
	int mounted;
} sd_card_info_t;

int start_sd_card(void);              // spawn background task that mounts SD and FatFs
int sd_card_get_info(sd_card_info_t *info);
int sd_card_ls(int fd);               // list root directory over TCP console
int sd_card_cat(const char *path, int fd);
int sd_card_write_text(const char *path, const char *text, int append);

#endif // SD_CARD_H
