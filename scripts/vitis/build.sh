#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONFIG_FILE="${BUILD_VITIS_CONFIG:-$SCRIPT_DIR/build_vitis.conf}"

# Portable defaults (can be overridden in config/env)
DEFAULT_XSA="$REPO_ROOT/artifacts/fpga/design.xsa"
CLEAN_DEFAULT="1"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi

if [[ -n "${XILINX_SETTINGS:-}" ]]; then
  if [[ ! -f "$XILINX_SETTINGS" ]]; then
    echo "Xilinx settings script '$XILINX_SETTINGS' not found" >&2
    exit 1
  fi
  # shellcheck disable=SC1090
  source "$XILINX_SETTINGS"
fi

if [[ -z "${XSA:-}" ]]; then
  XSA="$DEFAULT_XSA"
fi
: "${CLEAN:=${CLEAN_DEFAULT}}"

if [[ "$CLEAN" != "0" && -d "$REPO_ROOT/vitis_ws" ]]; then
  echo "Removing existing workspace at $REPO_ROOT/vitis_ws"
  rm -rf "$REPO_ROOT/vitis_ws"
fi

if ! command -v xsct >/dev/null 2>&1; then
  echo "ERROR: xsct not found in PATH" >&2
  echo "Please source the Xilinx Vitis/Xilinx SDK environment before running this script." >&2
  exit 1
fi

export XSA CLEAN

echo "Starting XSCT build with XSA=$XSA (CLEAN=$CLEAN)"
xsct "$SCRIPT_DIR/build.tcl"
