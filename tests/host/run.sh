#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_TMP="$(mktemp -d)"
trap 'rm -rf "$TEST_TMP"' EXIT

PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/test_boot_image.py"
cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$SCRIPT_DIR/qspi_stubs" -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_qspi_flash.c" \
  "$REPO_ROOT/src/ports/freertos-xilinx/storage/qspi-flash/qspi_flash.c" \
  -o "$TEST_TMP/test_qspi_flash"
"$TEST_TMP/test_qspi_flash"

bash "$SCRIPT_DIR/run_http.sh" "$TEST_TMP"

cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_journal.c" "$REPO_ROOT/src/shared/events/bvstk_journal.c" \
  -o "$TEST_TMP/test_journal"
"$TEST_TMP/test_journal"

cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_i2c_master.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c" -o "$TEST_TMP/test_i2c_master"
"$TEST_TMP/test_i2c_master"

cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_device_profiles.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" -o "$TEST_TMP/test_device_profiles"
"$TEST_TMP/test_device_profiles" "$REPO_ROOT/configs"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wno-overlength-strings -ffunction-sections -fdata-sections \
  -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" -I"$REPO_ROOT/src/apps/freertos/config" \
  "$SCRIPT_DIR/test_config_bundles.c" \
  "$REPO_ROOT/src/apps/neutrino/config/bvstk_i2c_config_store.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  -Wl,--gc-sections -o "$TEST_TMP/test_config_bundles"
"$TEST_TMP/test_config_bundles" "$TEST_TMP/flash"

cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_profile_v2.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" -o "$TEST_TMP/test_profile_v2"
"$TEST_TMP/test_profile_v2" "$REPO_ROOT/configs" "$SCRIPT_DIR/fixtures/device-profiles-v1"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_shared.c" \
  "$REPO_ROOT/src/services/cache/bvstk_cache.c" \
  "$REPO_ROOT/src/services/cache/bvstk_cache_bridges.c" \
  "$REPO_ROOT/src/hardware/boards/ax7020/bvstk_pl_regions.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_slave.c" \
  "$REPO_ROOT/src/drivers/pl/sd/bvstk_sd_controller.c" \
  "$REPO_ROOT/src/drivers/pl/mdio/bvstk_mdio_core.c" \
  "$REPO_ROOT/src/drivers/pl/spi/bvstk_spi_core.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_control.c" \
  "$REPO_ROOT/src/shared/base/bvstk_parse.c" \
  "$REPO_ROOT/src/shared/base/bvstk_status.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_slave_service.c" \
  "$REPO_ROOT/src/services/mdio/bvstk_mdio_service.c" \
  "$REPO_ROOT/src/services/control/bvstk_control_api.c" \
  -o "$TEST_TMP/test_shared"

"$TEST_TMP/test_shared"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_mdio.c" \
  "$REPO_ROOT/src/services/cache/bvstk_cache.c" \
  "$REPO_ROOT/src/services/cache/bvstk_cache_bridges.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/drivers/pl/mdio/bvstk_mdio_core.c" \
  "$REPO_ROOT/src/services/mdio/bvstk_mdio_service.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  "$REPO_ROOT/src/shared/cli/bvstk_mdio_completion.c" \
  -o "$TEST_TMP/test_mdio"

"$TEST_TMP/test_mdio"
PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/test_dcp2_mdio.py"

cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_bus_policy.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c" \
  "$REPO_ROOT/src/drivers/pl/mdio/bvstk_mdio_core.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/services/mdio/bvstk_mdio_service.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  -o "$TEST_TMP/test_bus_policy"
"$TEST_TMP/test_bus_policy"

cc -std=c11 -Wall -Wextra -Werror -pedantic -ffunction-sections -fdata-sections \
  -I"$SCRIPT_DIR/policy_stubs" -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_dcp2_policy_sync.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  -Wl,--gc-sections -pthread -o "$TEST_TMP/test_dcp2_policy_sync"
"$TEST_TMP/test_dcp2_policy_sync"

cc -std=c11 -Wall -Wextra -Werror -pedantic -ffunction-sections -fdata-sections \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/dcp2_policy_server.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_control.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/services/mdio/bvstk_mdio_service.c" \
  -Wl,--gc-sections -o "$TEST_TMP/dcp2_policy_server"
PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/test_dcp2_policy.py" "$TEST_TMP/dcp2_policy_server"


cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/dcp2_cache_server.c" \
  "$REPO_ROOT/src/services/cache/bvstk_cache.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_cache.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c" \
  -o "$TEST_TMP/dcp2_cache_server"
PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/test_dcp2_cache.py" "$TEST_TMP/dcp2_cache_server"
node "$REPO_ROOT/tests/web/cache-indicators.mjs"

cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic \
  -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_bus_shell.c" \
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c" \
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c" \
  "$REPO_ROOT/src/apps/freertos/console/bus_shell_common.c" \
  "$REPO_ROOT/src/apps/freertos/console/i2c_shell.c" \
  "$REPO_ROOT/src/apps/freertos/console/mdio_shell.c" \
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c" \
  "$REPO_ROOT/src/drivers/pl/mdio/bvstk_mdio_core.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c" \
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c" \
  "$REPO_ROOT/src/services/mdio/bvstk_mdio_service.c" \
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c" \
  "$REPO_ROOT/src/shared/cli/bvstk_i2c_completion.c" \
  "$REPO_ROOT/src/shared/cli/bvstk_mdio_completion.c" \
  "$REPO_ROOT/src/shared/base/bvstk_parse.c" \
  "$REPO_ROOT/src/shared/base/bvstk_status.c" \
  -o "$TEST_TMP/test_bus_shell"

"$TEST_TMP/test_bus_shell"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_sd_master.c" \
  "$REPO_ROOT/src/drivers/pl/sd/bvstk_sd_master.c" \
  "$REPO_ROOT/src/services/sd/bvstk_sd_service.c" \
  -o "$TEST_TMP/test_sd_master"

"$TEST_TMP/test_sd_master"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_sd_volume.c" \
  "$REPO_ROOT/src/services/sd/bvstk_sd_volume.c" \
  -o "$TEST_TMP/test_sd_volume"

"$TEST_TMP/test_sd_volume"

cc -std=c11 -Wall -Wextra -Werror -pedantic -ffunction-sections -fdata-sections \
  -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_fs_policy.c" \
  "$REPO_ROOT/src/apps/freertos/storage/fs/fs_shared.c" \
  -Wl,--gc-sections -o "$TEST_TMP/test_fs_policy"

"$TEST_TMP/test_fs_policy"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$SCRIPT_DIR/fs_stubs" -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_sd_disk.c" \
  "$REPO_ROOT/src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.c" \
  "$REPO_ROOT/src/services/sd/bvstk_sd_volume.c" \
  "$REPO_ROOT/src/shared/base/bvstk_status.c" \
  -o "$TEST_TMP/test_sd_disk"

for disk_case in init-error no-filesystem read-error write-error; do
  "$TEST_TMP/test_sd_disk" "$disk_case"
done

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_i2c_completion.c" \
  "$REPO_ROOT/src/shared/cli/bvstk_i2c_completion.c" \
  -o "$TEST_TMP/test_i2c_completion"

"$TEST_TMP/test_i2c_completion"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/test_line_editor.c" \
  "$REPO_ROOT/src/shared/cli/bvstk_line_editor.c" \
  -o "$TEST_TMP/test_line_editor"

"$TEST_TMP/test_line_editor"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$REPO_ROOT/src" \
  "$SCRIPT_DIR/dcp2_fs_server.c" \
  "$REPO_ROOT/src/services/fs/bvstk_fs_reader.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c" \
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_fs.c" \
  -o "$TEST_TMP/dcp2_fs_server"

PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/test_dcp2_fs.py" "$TEST_TMP/dcp2_fs_server"
