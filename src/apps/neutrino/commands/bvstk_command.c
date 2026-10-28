#include "apps/neutrino/commands/bvstk_command.h"
#include <string.h>
#include <strings.h>

static bvstk_status_t mdio_changed(void *context, size_t id)
{ return bvstk_neutrino_mdio_runtime_sync_locked(context, id) ? BVSTK_OK : BVSTK_ERR_INTERNAL; }
static bvstk_status_t mdio_save(void *context, bool mdio, size_t id)
{ return !mdio ? BVSTK_ERR_UNSUPPORTED : bvstk_neutrino_mdio_runtime_save_locked(context, id) ? BVSTK_OK : BVSTK_ERR_IO; }
int bvstk_neutrino_command_execute(bvstk_neutrino_command_t *runtime,
    const char *cwd, const char *line, const bvstk_output_t *output)
{
    bvstk_cli_t cli = {0}; cli.output = *output;
    char command[2048], *argv[64];
    if (!line || strlen(line) >= sizeof(command)) {
        bvstk_cli_result(&cli, "command", BVSTK_ERR_RANGE); return cli.result;
    }
    strcpy(command, line);
    int argc = bvstk_cli_tokens(command, argv, 64);
    if (argc < 0) { bvstk_cli_result(&cli, "command", BVSTK_ERR_RANGE); return cli.result; }
    if (!argc || !strcasecmp(argv[0], "help")) {
        bvstk_cli_text(&cli, "BVSTK commands: i2c mdio fs tar mem pl spi uart ip system reboot\r\n"
            "Use <command> --help. Device edits apply in RAM; save persists the profile.\r\n");
        return cli.result;
    }
    if ((!strcmp(argv[0], "system") || !strcmp(argv[0], "ip") || !strcmp(argv[0], "reboot")) && runtime->system)
        return runtime->system(argc, argv, output);
    if (!strcasecmp(argv[0], "i2c")) {
        bvstk_neutrino_i2c_runtime_t *i2c = runtime->runtime->i2c;
        bvstk_neutrino_i2c_runtime_lock(i2c);
        bvstk_neutrino_i2c_cli_init(&cli, i2c, output); cli.streams = runtime->streams;
        (void)bvstk_i2c_cli(&cli, argc - 1, argv + 1);
        bvstk_neutrino_i2c_runtime_unlock(i2c);
    } else if (!strcasecmp(argv[0], "mdio")) {
        bvstk_neutrino_mdio_runtime_t *mdio = runtime->runtime->mdio;
        bvstk_neutrino_mdio_runtime_lock(mdio);
        cli.context = mdio; cli.mdio = bvstk_neutrino_mdio_runtime_service(mdio);
        cli.save = mdio_save; cli.mdio_changed = mdio_changed; cli.streams = runtime->streams;
        (void)bvstk_mdio_cli(&cli, argc - 1, argv + 1);
        bvstk_neutrino_mdio_runtime_unlock(mdio);
    } else if (!strcasecmp(argv[0], "fs"))
        (void)bvstk_storage_cli(&cli, runtime->storage, cwd, argc - 1, argv + 1);
    else if (!strcasecmp(argv[0], "tar"))
        (void)bvstk_tar_cli(&cli, runtime->storage, cwd, argc - 1, argv + 1);
    else if (!strcasecmp(argv[0], "mem"))
        (void)bvstk_memory_cli(&cli, runtime->control, argc - 1, argv + 1);
    else if (!strcasecmp(argv[0], "pl"))
        (void)bvstk_pl_cli(&cli, runtime->control, argc - 1, argv + 1);
    else if ((!strcasecmp(argv[0], "spi") || !strcasecmp(argv[0], "uart")) &&
        argc >= 2 && !strcasecmp(argv[1], "stream")) {
        cli.streams = runtime->streams;
        (void)bvstk_stream_cli(&cli, !strcasecmp(argv[0], "spi") ?
            DCP2_STREAM_SERVICE_SPI : DCP2_STREAM_SERVICE_UART, argc - 2, argv + 2);
    } else bvstk_cli_result(&cli, "command", BVSTK_ERR_UNSUPPORTED);
    return cli.result;
}
