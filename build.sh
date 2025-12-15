#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_XSA="/home/ilya/Zynq/bvstk_hw/Burevestnik_top.xsa"

: "${XSA:=${DEFAULT_XSA}}"
: "${CLEAN:=1}"

if [[ "$CLEAN" != "0" && -d "$SCRIPT_DIR/vitis_ws" ]]; then
  echo "Removing existing workspace at $SCRIPT_DIR/vitis_ws"
  rm -rf "$SCRIPT_DIR/vitis_ws"
fi

if ! command -v xsct >/dev/null 2>&1; then
  echo "ERROR: xsct not found in PATH" >&2
  echo "Please source the Xilinx Vitis/Xilinx SDK environment before running this script." >&2
  exit 1
fi

export XSA CLEAN

echo "Starting XSCT build with XSA=$XSA (CLEAN=$CLEAN)"
xsct "$SCRIPT_DIR/build.tcl"
