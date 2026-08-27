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
  --out "$BUILD_DIR/default_configs.h" \
  --i2c-only

COMMON_SOURCES=(
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_master.c"
  "$REPO_ROOT/src/drivers/pl/i2c/bvstk_i2c_slave.c"
  "$REPO_ROOT/src/drivers/pl/smi/bvstk_smi_core.c"
  "$REPO_ROOT/src/drivers/pl/spi/bvstk_spi_core.c"
  "$REPO_ROOT/src/hardware/boards/ax7020/bvstk_pl_regions.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_platform_neutrino.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_sync_neutrino.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_codec.c"
  "$REPO_ROOT/src/protocols/dcp2/bvstk_dcp2_control.c"
  "$REPO_ROOT/src/shared/base/bvstk_status.c"
  "$REPO_ROOT/src/shared/config/bvstk_i2c_config_codec.c"
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c"
  "$REPO_ROOT/src/services/control/bvstk_control_api.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_slave_service.c"
  "$REPO_ROOT/src/services/smi/bvstk_smi_service.c"
)

NEUTRINO_I2C_RUNTIME_SOURCES=(
  "$REPO_ROOT/src/apps/neutrino/config/bvstk_i2c_config_store.c"
  "$REPO_ROOT/src/apps/neutrino/i2c/bvstk_i2c_resmgr.c"
  "$REPO_ROOT/src/apps/neutrino/runtime/bvstk_i2c_runtime.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c"
)

I2C_CLIENT_SOURCE="$REPO_ROOT/src/apps/neutrino/i2c/bvstk_i2c_client.c"
I2C_COMPLETION_SOURCE="$REPO_ROOT/src/shared/cli/bvstk_i2c_completion.c"
LINE_EDITOR_SOURCE="$REPO_ROOT/src/shared/cli/bvstk_line_editor.c"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -I"$BUILD_DIR" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/apps/neutrino/bvstkctl/main.c" \
  "$I2C_CLIENT_SOURCE" \
  "${COMMON_SOURCES[@]}" \
  -o "$BUILD_DIR/bvstkctl"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -I"$BUILD_DIR" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/apps/neutrino/bvstkd/main.c" \
  "${NEUTRINO_I2C_RUNTIME_SOURCES[@]}" \
  "${COMMON_SOURCES[@]}" \
  -lsocket \
  -o "$BUILD_DIR/bvstkd"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/apps/neutrino/i2c/main.c" \
  "$I2C_CLIENT_SOURCE" \
  -o "$BUILD_DIR/i2c"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/apps/neutrino/shell/main.c" \
  "$I2C_CLIENT_SOURCE" \
  "$I2C_COMPLETION_SOURCE" \
  "$LINE_EDITOR_SOURCE" \
  -o "$BUILD_DIR/bvstk-shell"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/storage/bvstk_qspi_raw.c" \
  -o "$BUILD_DIR/bvstk-qspi-fat"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/ports/neutrino-zynq7000/storage/bvstk_sd_raw.c" \
  -o "$BUILD_DIR/bvstk-sd-raw"

echo "Neutrino applications: $BUILD_DIR/i2c, $BUILD_DIR/bvstkctl, $BUILD_DIR/bvstkd, $BUILD_DIR/bvstk-shell, $BUILD_DIR/bvstk-qspi-fat and $BUILD_DIR/bvstk-sd-raw"
