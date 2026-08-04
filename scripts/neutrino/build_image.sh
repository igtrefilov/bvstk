#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NEUTRINO_BUILD_DIR:-$REPO_ROOT/build/neutrino}"
DEFAULT_BSP_DIR="/home/ilya/neutrino/kpda-bsp-xilinx-zynq7000-2024-bin-20260430-43ee921596/kpda-bsp-xilinx-zynq7000"
BSP_DIR="${NEUTRINO_BSP_DIR:-$DEFAULT_BSP_DIR}"
BASE_BUILD="${NEUTRINO_BASE_BUILD:-$BSP_DIR/images/zynq7000-ax7020-ssh.build}"
GENERATED_BUILD="$BUILD_DIR/zynq7000-ax7020-bvstk.build"
IFS_FILE="${NEUTRINO_IFS_FILE:-$BUILD_DIR/ifs-zynq7000-ax7020-bvstk.raw}"
BVSTKCTL="$BUILD_DIR/bvstkctl"

if ! command -v mkifs >/dev/null 2>&1 && [[ -r /etc/profile.d/kpda_env_2024.sh ]]; then
  # shellcheck disable=SC1091
  source /etc/profile.d/kpda_env_2024.sh
fi

if ! command -v mkifs >/dev/null 2>&1; then
  echo "mkifs not found; source the Neutrino 2024 development kit" >&2
  exit 1
fi
if [[ ! -f "$BASE_BUILD" ]]; then
  echo "Base Neutrino build file not found: $BASE_BUILD" >&2
  exit 1
fi
if [[ ! -d "$BSP_DIR/install" ]]; then
  echo "BSP install tree not found: $BSP_DIR/install" >&2
  exit 1
fi

"$SCRIPT_DIR/build.sh"
mkdir -p "$BUILD_DIR"
cp "$BASE_BUILD" "$GENERATED_BUILD"
printf '\n# Burevestnik multi-OS application\n[perms=0755] /usr/bin/bvstkctl = %s\n' \
  "$BVSTKCTL" >> "$GENERATED_BUILD"

mkifs -r "$BSP_DIR/install" "$GENERATED_BUILD" "$IFS_FILE"

echo "Neutrino IFS: $IFS_FILE"
echo "Included utility: /usr/bin/bvstkctl"
