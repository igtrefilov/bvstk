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
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c"
  "$REPO_ROOT/src/services/control/bvstk_control_api.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_cache.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_devices.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_master_service.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_policy.c"
  "$REPO_ROOT/src/services/i2c/bvstk_i2c_slave_service.c"
  "$REPO_ROOT/src/services/smi/bvstk_smi_service.c"
)

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/apps/neutrino/bvstkctl/main.c" \
  "${COMMON_SOURCES[@]}" \
  -o "$BUILD_DIR/bvstkctl"

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "$REPO_ROOT/src/apps/neutrino/bvstkd/main.c" \
  "${COMMON_SOURCES[@]}" \
  -lsocket \
  -o "$BUILD_DIR/bvstkd"

echo "Neutrino applications: $BUILD_DIR/bvstkctl and $BUILD_DIR/bvstkd"
