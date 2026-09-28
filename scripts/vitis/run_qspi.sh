#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${RUN_QSPI_CONFIG:-$SCRIPT_DIR/run_qspi.conf}"
if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi
if [[ -n "${XILINX_SETTINGS:-}" ]]; then
  # shellcheck disable=SC1090
  source "$XILINX_SETTINGS"
fi
for name in BOOT_OUTPUT_DIR HW_SERVER_URL JTAG_FREQUENCY; do
  if [[ -n "${!name:-}" ]]; then export "${name?}"; fi
done
exec python3 "$SCRIPT_DIR/run_qspi.py" "$@"
