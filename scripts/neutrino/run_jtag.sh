#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$REPO_ROOT/scripts/common/hw_server.sh"
BUILD_DIR="${NEUTRINO_BUILD_DIR:-$REPO_ROOT/build/neutrino}"

HW_SERVER_OPTION=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --hw-server)
      if [[ $# -lt 2 || -z "$2" || "$2" == -* ]]; then
        echo "ERROR: --hw-server requires HOST[:PORT]" >&2
        exit 2
      fi
      HW_SERVER_OPTION="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [--hw-server HOST[:PORT]]"
      exit 0
      ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done
if [[ -n "$HW_SERVER_OPTION" ]]; then
  bvstk_set_hw_server_url "$HW_SERVER_OPTION"
elif [[ -n "${HW_SERVER_URL:-}" ]]; then
  bvstk_set_hw_server_url "$HW_SERVER_URL"
fi

IFS_FILE="${IFS_FILE:-$BUILD_DIR/ifs-zynq7000-ax7020-bvstk.raw}"
BITSTREAM_FILE="${BITSTREAM_FILE:-$REPO_ROOT/artifacts/fpga/design.bit}"
PS7_INIT_TCL="${PS7_INIT_TCL:-$REPO_ROOT/vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl}"
UART_BAUD="${UART_BAUD:-115200}"
LOG_FILE="${LOG_FILE:-$BUILD_DIR/neutrino-uart.log}"

select_uart_device() {
  # UART_DEVICE is an explicit override and also accepts a stable
  # /dev/serial/by-id path. This keeps the script usable on machines where
  # Linux assigns a different ttyUSB number after reconnecting the board.
  if [[ -n "${UART_DEVICE:-}" ]]; then
    printf '%s\n' "$UART_DEVICE"
    return
  fi

  shopt -s nullglob
  local candidate
  local -a stable=()
  local -a preferred=()
  local -a devices=()

  for candidate in /dev/serial/by-id/*; do
    [[ -e "$candidate" ]] || continue
    stable+=("$candidate")
    case "${candidate,,}" in
      *cp210*|*silicon_labs*|*ax7020*) preferred+=("$candidate") ;;
    esac
  done

  if (( ${#preferred[@]} == 1 )); then
    printf '%s\n' "${preferred[0]}"
    return
  fi
  if (( ${#stable[@]} == 1 )); then
    printf '%s\n' "${stable[0]}"
    return
  fi

  for candidate in /dev/ttyUSB* /dev/ttyACM*; do
    [[ -e "$candidate" ]] && devices+=("$candidate")
  done
  if (( ${#devices[@]} == 1 )); then
    printf '%s\n' "${devices[0]}"
    return
  fi

  echo "UART device was not selected automatically." >&2
  echo "Set UART_DEVICE=/dev/serial/by-id/<adapter> or UART_DEVICE=/dev/ttyUSB<N>." >&2
  if (( ${#stable[@]} > 0 )); then
    echo "Stable serial devices:" >&2
    printf '  %s\n' "${stable[@]}" >&2
  else
    echo "Detected serial devices:" >&2
    printf '  %s\n' "${devices[@]}" >&2
  fi
  exit 1
}

UART_DEVICE="$(select_uart_device)"

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
  echo "Use UART_DEVICE=/dev/ttyUSB<N> or a path from /dev/serial/by-id." >&2
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

if ! grep -Eq "Welcome to KPDA Neutrino|Starting Network driver|Starting platform-control" "$LOG_FILE"; then
  echo "Neutrino startup signature not found in $LOG_FILE" >&2
  exit 2
fi

echo "Neutrino started; UART log: $LOG_FILE"
