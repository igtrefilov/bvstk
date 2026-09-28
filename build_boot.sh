#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Use the same trusted environment configuration as the FreeRTOS build.
SCRIPT_DIR="$REPO_ROOT/scripts/vitis"
CONFIG_FILE="${BUILD_VITIS_CONFIG:-$SCRIPT_DIR/build_vitis.conf}"
if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi
if [[ -n "${XILINX_SETTINGS:-}" ]]; then
  # shellcheck disable=SC1090
  source "$XILINX_SETTINGS"
fi
if [[ -n "${XSA:-}" ]]; then export XSA; fi
exec python3 "$SCRIPT_DIR/build_boot.py" "$@"
