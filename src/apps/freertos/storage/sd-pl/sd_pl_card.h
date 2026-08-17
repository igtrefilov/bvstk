#ifndef SD_PL_CARD_H
#define SD_PL_CARD_H

#include "apps/freertos/storage/fs/fs_shared.h"

#define SD_PL_ROOT "2:/"

int start_sd_pl_card(void);
int sd_pl_card_is_ready(void);
fs_shared_ctx_t *sd_pl_card_get_context(void);

#endif /* SD_PL_CARD_H */
