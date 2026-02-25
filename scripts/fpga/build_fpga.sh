#!/usr/bin/env bash
set -euo pipefail

# Wrapper that recreates the Vivado project and produces design.bit/design.xsa
# Outputs are placed in ../../artifacts/fpga relative to this script by default.

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# Default config location. Can be overridden via --config or BUILD_FPGA_CONFIG.
CONFIG_FILE=${BUILD_FPGA_CONFIG:-"$SCRIPT_DIR/build_fpga.conf"}

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --config FILE    Path to config file (default: \$BUILD_FPGA_CONFIG or $SCRIPT_DIR/build_fpga.conf)
  --jobs N         Number of parallel jobs for Vivado implementation
  --vivado PATH    Path to Vivado executable
  --proj NAME      Vivado project name
  --fpga-dir DIR   Path to FPGA project directory with Burevestnik_21.tcl
  --output-dir DIR Output directory for design.bit/design.xsa
  --clean          Remove existing vivado_project before build
  --help           Show this help

Precedence:
  built-in defaults < config file < CLI flags
EOF
}

# Built-in defaults (safety net; normal workflow is config-driven).
FPGA_DIR="$SCRIPT_DIR/../../../hw_platform/fpga"
PROJ_NAME="Burevestnik_21"
JOBS="8"
VIVADO_BIN="vivado"
OUTPUT_DIR="$SCRIPT_DIR/../../artifacts/fpga"
CLEAN="0"

# First pass: allow selecting config file before loading settings.
args=("$@")
idx=0
while [[ $idx -lt ${#args[@]} ]]; do
  case "${args[$idx]}" in
    --config)
      if [[ $((idx + 1)) -ge ${#args[@]} ]]; then
        echo "Missing value for --config" >&2
        exit 1
      fi
      CONFIG_FILE="${args[$((idx + 1))]}"
      idx=$((idx + 2))
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      idx=$((idx + 1))
      ;;
  esac
done

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "Config file '$CONFIG_FILE' not found." >&2
  echo "Create it (for example from build_fpga.conf.example) and set required paths." >&2
  exit 1
fi
# shellcheck disable=SC1090
source "$CONFIG_FILE"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config) shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --vivado) VIVADO_BIN="$2"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    --proj) PROJ_NAME="$2"; shift 2 ;;
    --fpga-dir) FPGA_DIR="$2"; shift 2 ;;
    --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

if [[ -n "${XILINX_SETTINGS:-}" ]]; then
  if [[ ! -f "$XILINX_SETTINGS" ]]; then
    echo "Xilinx settings script '$XILINX_SETTINGS' not found." >&2
    exit 1
  fi
  # shellcheck disable=SC1090
  source "$XILINX_SETTINGS"
fi

require_nonempty() {
  local name="$1"
  local value="$2"
  if [[ -z "$value" ]]; then
    echo "Required config value '$name' is empty (config: $CONFIG_FILE)." >&2
    exit 1
  fi
}

require_nonempty "VIVADO_BIN" "$VIVADO_BIN"
require_nonempty "FPGA_DIR" "$FPGA_DIR"
require_nonempty "PROJ_NAME" "$PROJ_NAME"
require_nonempty "JOBS" "$JOBS"
require_nonempty "OUTPUT_DIR" "$OUTPUT_DIR"

if ! command -v "$VIVADO_BIN" >/dev/null 2>&1; then
  echo "Vivado executable '$VIVADO_BIN' not found" >&2
  exit 1
fi

FPGA_DIR=$(cd "$FPGA_DIR" && pwd)
PROJ_DIR="$FPGA_DIR/vivado_project"
mkdir -p "$OUTPUT_DIR"

if [[ $CLEAN -eq 1 ]]; then
  echo "Removing existing project directory $PROJ_DIR"
  rm -rf "$PROJ_DIR"
fi

echo "Creating/refreshing Vivado project in $FPGA_DIR..."
"$VIVADO_BIN" -mode batch -source "$FPGA_DIR/Burevestnik_21.tcl" -tclargs --origin_dir "$FPGA_DIR" --project_name "$PROJ_NAME"

echo "Running implementation and exporting hardware..."
"$VIVADO_BIN" -mode batch -source "$SCRIPT_DIR/build_hw.tcl" -tclargs --fpga_dir "$FPGA_DIR" --project_name "$PROJ_NAME" --jobs "$JOBS" --output_dir "$OUTPUT_DIR"

echo "Done. Outputs in $OUTPUT_DIR:"; ls -1 "$OUTPUT_DIR" | sed 's/^/  /'
