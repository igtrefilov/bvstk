#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<'EOF'
Usage: ./hw_server.sh {--start|--stop} [--port PORT] [--settings FILE]

Manage the Xilinx hardware server for remote USB-JTAG access.
  --start          Start in the background; keep running after logout.
  --stop           Stop the instance started by this script on this port.
  --port PORT      TCP listening port (default: 3121).
  --settings FILE  Source this Xilinx settings64.sh before starting.
  -h, --help       Show this help.

Uses hw_server from PATH. If unavailable, loads
~/Xilinx/Vitis/2021.2/settings64.sh. XILINX_SETTINGS overrides that choice.
PID and log files: build/hw_server/<port>.pid and <port>.log.
Requires Linux and Python 3. Stop does not require a Xilinx installation.
EOF
}

port=3121
action=""
settings="${XILINX_SETTINGS:-}"
while (( $# )); do
  case "$1" in
    --start|--stop)
      if [[ -n "$action" ]]; then
        echo "ERROR: Choose exactly one of --start and --stop" >&2
        exit 2
      fi
      action="${1#--}"; shift ;;
    --port|--settings)
      if (( $# < 2 )) || [[ -z "$2" || "$2" == -* ]]; then
        echo "ERROR: $1 requires a value" >&2
        exit 2
      fi
      if [[ "$1" == --port ]]; then port="$2"; else settings="$2"; fi
      shift 2
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done
if [[ -z "$action" ]]; then usage >&2; exit 2; fi

if [[ ! "$port" =~ ^[0-9]{1,5}$ ]] || (( 10#$port < 1 || 10#$port > 65535 )); then
  echo "ERROR: Port must be between 1 and 65535" >&2
  exit 2
fi
port=$((10#$port))

if [[ "$action" == start ]]; then
  if [[ -z "$settings" ]] && ! command -v hw_server >/dev/null 2>&1; then
    settings="$HOME/Xilinx/Vitis/2021.2/settings64.sh"
  fi
  if [[ -n "$settings" ]]; then
    if [[ ! -r "$settings" ]]; then
      echo "ERROR: Xilinx settings file not readable: $settings" >&2
      exit 1
    fi
    # Vendor setup scripts can reference unset variables.
    set +u
    # shellcheck disable=SC1090
    source "$settings"
    set -u
  fi
  if ! command -v hw_server >/dev/null 2>&1; then
    echo "ERROR: hw_server not found; use --settings /path/to/settings64.sh" >&2
    exit 1
  fi
fi

exec python3 "$REPO_ROOT/scripts/common/hw_server_control.py" "$action" "$port" "$REPO_ROOT/build/hw_server"
