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

SOURCES=(
  "$REPO_ROOT/src/apps/neutrino/bvstkctl/main.c"
  "$REPO_ROOT/src/hardware/boards/ax7020/bvstk_pl_regions.c"
  "$REPO_ROOT/src/ports/neutrino-zynq7000/os/bvstk_platform_neutrino.c"
  "$REPO_ROOT/src/shared/base/bvstk_status.c"
  "$REPO_ROOT/src/shared/pl/access/bvstk_pl_service.c"
)

qcc -V"$QCC_VARIANT" \
  -Wall -Wextra -Werror -O2 \
  -I"$REPO_ROOT/src" \
  -DBVSTK_PLATFORM_NEUTRINO=1 \
  "${SOURCES[@]}" \
  -o "$BUILD_DIR/bvstkctl"

echo "Neutrino application: $BUILD_DIR/bvstkctl"
