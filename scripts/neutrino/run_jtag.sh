#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${NEUTRINO_BUILD_DIR:-$REPO_ROOT/build/neutrino}"

IFS_FILE="${IFS_FILE:-$BUILD_DIR/ifs-zynq7000-ax7020-bvstk.raw}"
BITSTREAM_FILE="${BITSTREAM_FILE:-$REPO_ROOT/artifacts/fpga/design.bit}"
PS7_INIT_TCL="${PS7_INIT_TCL:-$REPO_ROOT/vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl}"
UART_DEVICE="${UART_DEVICE:-/dev/ttyUSB1}"
UART_BAUD="${UART_BAUD:-115200}"
LOG_FILE="${LOG_FILE:-$BUILD_DIR/neutrino-uart.log}"

if ! command -v xsct >/dev/null 2>&1; then
  if [[ -r /home/ilya/Xilinx/Vitis/2021.2/settings64.sh ]]; then
    # shellcheck disable=SC1091
    source /home/ilya/Xilinx/Vitis/2021.2/settings64.sh
  fi
fi

for input in "$IFS_FILE" "$BITSTREAM_FILE" "$PS7_INIT_TCL"; do
  if [[ ! -f "$input" ]]; then
    echo "Required input not found: $input" >&2
    exit 1
  fi
done
if [[ ! -e "$UART_DEVICE" ]]; then
  echo "UART device not found: $UART_DEVICE" >&2
  exit 1
fi
if ! command -v xsct >/dev/null 2>&1; then
  echo "xsct not found" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"
stty -F "$UART_DEVICE" "$UART_BAUD" cs8 -cstopb -parenb raw -echo \
  -ixon -ixoff -crtscts clocal

tee "$LOG_FILE" < "$UART_DEVICE" &
UART_READER_PID=$!
cleanup() {
  kill "$UART_READER_PID" 2>/dev/null || true
  wait "$UART_READER_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

export IFS_FILE BITSTREAM_FILE PS7_INIT_TCL
xsct "$SCRIPT_DIR/run_jtag.tcl"
sleep 3

if ! rg -q "Welcome to KPDA Neutrino|Starting Network driver|Starting platform-control" "$LOG_FILE"; then
  echo "Neutrino startup signature not found in $LOG_FILE" >&2
  exit 2
fi

echo "Neutrino started; UART log: $LOG_FILE"
