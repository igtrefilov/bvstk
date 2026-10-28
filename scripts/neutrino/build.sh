#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NEUTRINO_BUILD_DIR:-$REPO_ROOT/build/neutrino}"
QCC_VARIANT="${QCC_VARIANT:-8.3.0,gcc_ntoarmv7le}"

if ! command -v qcc >/dev/null 2>&1 && [[ -r /etc/profile.d/kpda_env_2024.sh ]]; then
  # shellcheck disable=SC1091
  source /etc/profile.d/kpda_env_2024.sh
fi

if ! command -v qcc >/dev/null 2>&1; then
  echo "qcc not found; install/source the Neutrino 2024 development kit" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"

python3 "$REPO_ROOT/tools/codegen/gen_default_configs.py" \
  --repo-root "$REPO_ROOT" \
  --out "$BUILD_DIR/default_configs.h"

COMMON_SOURCES=(
  "$REPO_ROOT/src/services/system/bvstk_system.c"
  "$REPO_ROOT/src/shared/protocols/http/bvstk_http_input.c"
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c"
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_slave.c"
  "$REPO_ROOT/src/drivers/pl/mdio/bvstk_mdio_core.c"
  "$REPO_ROOT/src/drivers/pl/spi/bvstk_spi_core.c"
  "$REPO_ROOT/src/hardware/boards/ax7020/bvstk_pl_regions.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_platform_neutrino.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_sync_neutrino.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_dispatch.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_session.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_cache.c"
  "$REPO_ROOT/src/services/cli/bvstk_cli.c"
  "$REPO_ROOT/src/services/cli/bvstk_i2c_cli.c"
  "$REPO_ROOT/src/services/cli/bvstk_mdio_cli.c"
  "$REPO_ROOT/src/services/cli/bvstk_stream_cli.c"
  "$REPO_ROOT/src/services/cli/bvstk_storage_cli.c"
  "$REPO_ROOT/src/services/cli/bvstk_tar_cli.c"
  "$REPO_ROOT/src/shared/base/bvstk_parse.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_fs.c"
  "$REPO_ROOT/src/services/stream/bvstk_stream_sim.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_control.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_policy.c"
  "$REPO_ROOT/src/shared/base/bvstk_status.c"
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c"
  "$REPO_ROOT/src/shared/config/bvstk_device_profile.c"
  "$REPO_ROOT/src/shared/config/bvstk_mdio_config_codec.c"
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c"
  "$REPO_ROOT/src/services/control/bvstk_control_api.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_slave_service.c"
  "$REPO_ROOT/src/services/mdio/bvstk_mdio_service.c"
  "$REPO_ROOT/src/services/cache/bvstk_cache.c"
  "$REPO_ROOT/src/services/cache/bvstk_cache_bridges.c"
  "$REPO_ROOT/src/services/observation/bvstk_observation.c"
  "$REPO_ROOT/src/services/fs/bvstk_fs_reader.c"
  "$REPO_ROOT/src/services/fs/bvstk_fs_storage.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/storage/bvstk_fs_posix.c"
  "$REPO_ROOT/src/shared/events/bvstk_journal.c"
)

NEUTRINO_I2C_RUNTIME_SOURCES=(
  "$REPO_ROOT/src/apps/neutrino/http/bvstk_http_neutrino.c"
  "$REPO_ROOT/src/apps/neutrino/http/bvstk_http_server.c"
  "$REPO_ROOT/src/protocols/http/bvstk_http_system.c"
  "$REPO_ROOT/src/protocols/http/bvstk_http_ui.c"
  "$REPO_ROOT/src/protocols/http/bvstk_http_bus.c"
  "$REPO_ROOT/src/protocols/http/bvstk_http_files.c"
  "$REPO_ROOT/src/protocols/http/bvstk_http_observe.c"
  "$REPO_ROOT/src/apps/neutrino/runtime/bvstk_system_neutrino.c"
  "$REPO_ROOT/src/apps/neutrino/commands/bvstk_command.c"
  "$REPO_ROOT/src/apps/neutrino/commands/bvstk_command_resmgr.c"
  "$REPO_ROOT/src/apps/neutrino/observe/bvstk_observe.c"
  "$REPO_ROOT/src/apps/neutrino/observe/bvstk_observe_resmgr.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_observation_neutrino.c"
  "$REPO_ROOT/src/apps/neutrino/bvstkd/bvstk_dcp2_runtime.c"
  "$REPO_ROOT/src/apps/neutrino/config/bvstk_profile_store.c"
  "$REPO_ROOT/src/apps/neutrino/config/bvstk_mdio_config_store.c"
  "$REPO_ROOT/src/apps/neutrino/runtime/bvstk_mdio_runtime.c"
  "$REPO_ROOT/src/services/acquisition/bvstk_acquisition.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_worker.c"
  "$REPO_ROOT/src/apps/neutrino/config/bvstk_i2c_config_store.c"
  "$REPO_ROOT/src/apps/neutrino/i2c/bvstk_i2c_resmgr.c"
  "$REPO_ROOT/src/apps/neutrino/runtime/bvstk_i2c_runtime.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c"
)

I2C_CLIENT_SOURCE="$REPO_ROOT/src/apps/neutrino/i2c/bvstk_i2c_client.c"
I2C_COMPLETION_SOURCE="$REPO_ROOT/src/shared/cli/bvstk_i2c_completion.c"
LINE_EDITOR_SOURCE="$REPO_ROOT/src/shared/cli/bvstk_line_editor.c"

declare -A compiled_sources=()
build_program() {
  local program="$1" source relative object first_object=""
  shift
  local object_dir="$BUILD_DIR/objects"
  local response="$BUILD_DIR/$program.objects"
  local -a libraries=()
  mkdir -p "$object_dir"
  : > "$response"
  for source in "$@"; do
    if [[ "$source" == -l* ]]; then libraries+=("$source"); continue; fi
    relative="${source#"$REPO_ROOT/"}"
    object="$object_dir/${relative//\//_}.o"
    if [[ -z "${compiled_sources[$source]:-}" ]]; then
      qcc -V"$QCC_VARIANT" -Wall -Wextra -Werror -O2 \
        -I"$REPO_ROOT/src" -I"$BUILD_DIR" -DBVSTK_PLATFORM_NEUTRINO=1 \
        -c "$source" -o "$object"
      compiled_sources[$source]=1
    fi
    if [[ -z "$first_object" ]]; then first_object="$object"
    else printf '"%s"\n' "$object" >> "$response"; fi
  done
  # qcc has a small argument limit. Let the GNU linker read the object list;
  # every source is still compiled with the same explicit target/diagnostics.
  qcc -V"$QCC_VARIANT" "$first_object" "-Wl,@$response" "${libraries[@]}" -o "$BUILD_DIR/$program"
}

build_program bvstkctl \
  "$REPO_ROOT/src/apps/neutrino/bvstkctl/main.c" \
  "$REPO_ROOT/src/apps/neutrino/observe/bvstk_observe_client.c" \
  "$I2C_CLIENT_SOURCE" \
  "$REPO_ROOT/src/apps/neutrino/commands/bvstk_command_client.c" \
  "${COMMON_SOURCES[@]}"

build_program bvstkd \
  "$REPO_ROOT/src/apps/neutrino/bvstkd/main.c" \
  "${NEUTRINO_I2C_RUNTIME_SOURCES[@]}" \
  "${COMMON_SOURCES[@]}" \
  -lsocket

build_program i2c \
  "$REPO_ROOT/src/apps/neutrino/i2c/main.c" \
  "$I2C_CLIENT_SOURCE" \
  "$REPO_ROOT/src/apps/neutrino/commands/bvstk_command_client.c"

build_program bvstk-shell \
  "$REPO_ROOT/src/apps/neutrino/shell/main.c" \
  "$I2C_CLIENT_SOURCE" \
  "$REPO_ROOT/src/apps/neutrino/commands/bvstk_command_client.c" \
  "$I2C_COMPLETION_SOURCE" \
  "$REPO_ROOT/src/shared/cli/bvstk_mdio_completion.c" \
  "$LINE_EDITOR_SOURCE"

build_program bvstk-qspi-fat \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/storage/bvstk_qspi_raw.c"

build_program bvstk-sd-raw \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/storage/bvstk_sd_raw.c"

# Application list follows all link steps.

build_program bvstk-sd-pl-fat \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/storage/bvstk_sd_pl_raw.c" \
  "$REPO_ROOT/src/drivers/pl/sd/bvstk_sd_master.c" \
  "$REPO_ROOT/src/services/sd/bvstk_sd_service.c" \
  "$REPO_ROOT/src/services/sd/bvstk_sd_volume.c" \
  "$REPO_ROOT/src/services/sd/bvstk_sd_disk.c" \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_platform_neutrino.c" \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_sync_neutrino.c" \
  "$REPO_ROOT/src/shared/base/bvstk_status.c" \
  "$REPO_ROOT/src/hardware/boards/ax7020/bvstk_pl_regions.c" \
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c"

echo "Neutrino applications include bvstk-sd-pl-fat (exclusive PL SD owner)"
