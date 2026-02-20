#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONFIG_FILE="${BUILD_VITIS_CONFIG:-$SCRIPT_DIR/build_vitis.conf}"

# Portable defaults (can be overridden in config/env)
DEFAULT_XSA="$REPO_ROOT/../bvstk_hw/tmp/design.xsa"
DEFAULT_XSA_FALLBACK="$REPO_ROOT/../bvstk_hw/Burevestnik_top.xsa"
CLEAN_DEFAULT="1"

if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi

if [[ -z "${XSA:-}" ]]; then
  if [[ -f "$DEFAULT_XSA" ]]; then
    XSA="$DEFAULT_XSA"
  else
    XSA="$DEFAULT_XSA_FALLBACK"
  fi
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
