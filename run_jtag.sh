#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BITSTREAM_OVERRIDE=""

if [ $# -gt 1 ]; then
  echo "Usage: $0 [bitstream-path]" >&2
  exit 1
elif [ $# -eq 1 ]; then
  BITSTREAM_OVERRIDE="$1"
fi

if ! command -v xsct >/dev/null 2>&1; then
  echo "ERROR: xsct not found in PATH" >&2
  echo "Please source the Xilinx Vitis/Xilinx SDK environment before running this script." >&2
  exit 1
fi

if [ -n "$BITSTREAM_OVERRIDE" ]; then
  export BITSTREAM_FILE="$BITSTREAM_OVERRIDE"
  echo "Using provided bitstream: $BITSTREAM_FILE"
else
  echo "Starting XSCT and running JTAG script"
fi

xsct "$SCRIPT_DIR/run_jtag.tcl"
