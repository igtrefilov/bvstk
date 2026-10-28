#!/usr/bin/env bash
set -euo pipefail
INTERFACE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
INTERFACE_ROOT=$(cd "$INTERFACE_DIR/../.." && pwd)
INTERFACE_OUTPUT=${1:?Pass an existing absolute temporary output directory}
cd "$INTERFACE_ROOT"
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic
  -ffunction-sections -fdata-sections -Isrc -pthread)
if [[ ${SANITIZE:-0} == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
fi
fs=(src/services/fs/bvstk_fs_reader.c src/services/fs/bvstk_fs_storage.c
  src/ports/neutrino-zynq7000/storage/bvstk_fs_posix.c)
cc "${flags[@]}" tests/host/test_storage_cli.c \
  src/services/cli/bvstk_cli.c src/services/cli/bvstk_storage_cli.c \
  src/services/cli/bvstk_tar_cli.c src/shared/base/bvstk_parse.c \
  "${fs[@]}" -Wl,--gc-sections -o "$INTERFACE_OUTPUT/test_storage_cli"
PYTHONDONTWRITEBYTECODE=1 python3 tests/host/tar_cli_fixtures.py "$INTERFACE_OUTPUT/tar-fixtures"
"$INTERFACE_OUTPUT/test_storage_cli" "$INTERFACE_OUTPUT/storage-cli" "$INTERFACE_OUTPUT/tar-fixtures"
cc "${flags[@]}" tests/host/test_dcp2_session.c \
  src/protocols/dcp2/bvstk_dcp2_session.c \
  src/protocols/dcp2/bvstk_dcp2_dispatch.c \
  src/protocols/dcp2/bvstk_dcp2_codec.c src/protocols/dcp2/bvstk_dcp2_control.c \
  src/protocols/dcp2/bvstk_dcp2_cache.c src/protocols/dcp2/bvstk_dcp2_policy.c \
  src/protocols/dcp2/bvstk_dcp2_fs.c src/services/stream/bvstk_stream_sim.c \
  src/services/control/bvstk_control_api.c src/services/cache/bvstk_cache.c \
  src/services/observation/bvstk_observation.c src/shared/events/bvstk_journal.c \
  src/shared/base/bvstk_status.c src/shared/config/bvstk_i2c_config_codec.c \
  src/shared/config/bvstk_mdio_config_codec.c src/shared/config/bvstk_device_profile.c \
  src/services/i2c/bvstk_i2c_master_service.c src/services/i2c/bvstk_i2c_devices.c \
  src/services/i2c/bvstk_i2c_policy.c src/services/i2c/bvstk_i2c_cache.c \
  src/services/mdio/bvstk_mdio_service.c src/drivers/pl/i2c/bvstk_i2c_master.c \
  src/drivers/pl/mdio/bvstk_mdio_core.c src/drivers/pl/spi/bvstk_spi_core.c \
  src/shared/pl/access/bvstk_pl_service.c src/hardware/boards/ax7020/bvstk_pl_regions.c \
  "${fs[@]}" -Wl,--gc-sections -o "$INTERFACE_OUTPUT/test_dcp2_session"
"$INTERFACE_OUTPUT/test_dcp2_session" "$INTERFACE_OUTPUT/dcp2-session"
