#!/usr/bin/env bash
set -euo pipefail

# Edit this:
DEVICE_IP="192.168.0.10"

# Optional overrides (usually keep defaults):
HTTP_PORT="8000"
CONSOLE_PORT="8888"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/assets"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  echo "Usage: ./upload_flash_www.sh [device-ip]"
  echo "Default device-ip is set in this script: DEVICE_IP=${DEVICE_IP}"
  exit 0
fi

if [[ -n "${1:-}" ]]; then
  DEVICE_IP="$1"
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 not found" >&2
  exit 1
fi

PY="$SCRIPT_DIR/upload_flash_www.py"
if [[ ! -f "$PY" ]]; then
  echo "ERROR: missing $PY" >&2
  exit 1
fi

if [[ ! -d "$SRC_DIR" ]]; then
  echo "ERROR: missing source directory: $SRC_DIR" >&2
  echo "Create it and put web assets there (only assets will be uploaded to flash:/www)." >&2
  exit 2
fi

exec python3 "$PY" "$DEVICE_IP" --http-port "$HTTP_PORT" --console-port "$CONSOLE_PORT" --src "$SRC_DIR"
