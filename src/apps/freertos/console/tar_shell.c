#include "apps/freertos/console/tar_shell.h"
#include "apps/freertos/console/bvstk_cli_freertos.h"
#include "apps/freertos/console/bvstk_storage_freertos.h"
#include "apps/freertos/storage/fs/fs_dcp2_backend.h"
#include <string.h>
#include <strings.h>
bool tar_handle(char *token, char **save, int fd, console_session_t *session)
{
    if (!token || strcasecmp(token, "tar")) return false;
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    char *argv[8], cwd[BVSTK_FS_PATH_CAPACITY];
    int argc = bvstk_cli_tokens(*save ? *save : (char *)"", argv, 8);
    bvstk_storage_freertos_cwd(session, cwd);
    if (argc < 0) bvstk_cli_result(&cli, "tar", BVSTK_ERR_RANGE);
    else (void)bvstk_tar_cli(&cli, fs_storage_backend(), cwd, argc, argv);
    return true;
}
void tar_help(int fd)
{
    bvstk_cli_t cli; bvstk_cli_freertos_init(&cli, fd);
    (void)bvstk_tar_cli(&cli, fs_storage_backend(), "flash:/", 0, NULL);
}
