#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONFIG_FILE="${RUN_JTAG_CONFIG:-$SCRIPT_DIR/run_jtag.conf}"

BITSTREAM_OVERRIDE=""
MODE="run"

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

if [[ -n "${BITSTREAM_FILE:-}" ]]; then export BITSTREAM_FILE; fi
if [[ -n "${ELF_FILE:-}" ]]; then export ELF_FILE; fi
if [[ -n "${PS7_INIT_TCL:-}" ]]; then export PS7_INIT_TCL; fi

usage() {
  cat >&2 <<'EOF'
Usage:
  run_jtag.sh [--debug] [bitstream-path]

Modes:
  (default)        Program PL + init PS7 + download ELF + run
  --debug          Program PL + init PS7 + halt CPU (for GDB/VSCode attach)

Notes:
  - Requires Xilinx environment in PATH (xsct/hw_server).
  - In --debug mode, attach GDB to hw_server GDB port (usually localhost:3000).
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug|--prepare-debug)
      MODE="debug"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
    *)
      if [[ -n "$BITSTREAM_OVERRIDE" ]]; then
        echo "Too many arguments." >&2
        usage
        exit 2
      fi
      BITSTREAM_OVERRIDE="$1"
      shift
      ;;
  esac
done

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

if [[ "$MODE" == "debug" ]]; then
  echo "Mode: debug (prepare JTAG and halt core0 for GDB attach)"
  if command -v hw_server >/dev/null 2>&1; then
    if command -v ss >/dev/null 2>&1; then
      if ! ss -ltn 2>/dev/null | grep -qE '[:.]3121\\b'; then
        echo "Starting hw_server with GDB ports enabled (-p 3000)..."
        (hw_server -s tcp::3121 -p 3000 -L- >/tmp/hw_server.log 2>&1 &) || true
        sleep 1
      elif ! ss -ltn 2>/dev/null | grep -qE '[:.]3000\\b'; then
        echo "ERROR: hw_server is running on port 3121, but GDB port 3000 is not open." >&2
        echo "Stop existing hw_server and restart with GDB ports enabled:" >&2
        echo "  pkill -f hw_server" >&2
        echo "  hw_server -s tcp::3121 -p 3000 -L-" >&2
        exit 1
      fi
    fi
  fi
  xsct "$REPO_ROOT/scripts/vscode/jtag_prepare_debug.tcl"
  echo ""
  echo "Next steps:"
  echo "  - Start VSCode debug config: Attach: Zynq-7000 (hw_server GDB, core0)"
  echo "  - Or CLI GDB:"
  echo "      arm-none-eabi-gdb \"$REPO_ROOT/vitis_ws/app_bvstk/Debug/app_bvstk.elf\" \\"
  echo "        -ex \"target remote :3000\" -ex \"load\" -ex \"tbreak main\" -ex \"continue\""
else
  xsct "$SCRIPT_DIR/run_jtag.tcl"
fi
