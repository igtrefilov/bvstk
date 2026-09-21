#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CONFIG_FILE="${BUILD_VITIS_CONFIG:-$SCRIPT_DIR/build_vitis.conf}"

# Portable defaults (can be overridden in config/env)
DEFAULT_XSA="$REPO_ROOT/artifacts/fpga/design.xsa"
CLEAN_DEFAULT="1"

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

if [[ -z "${XSA:-}" ]]; then
  XSA="$DEFAULT_XSA"
fi
: "${CLEAN:=${CLEAN_DEFAULT}}"

WORKSPACE="$REPO_ROOT/vitis_ws"
APP_BUILD_DIR="$WORKSPACE/app_bvstk/Debug"
APP_ELF="$APP_BUILD_DIR/app_bvstk.elf"
BSP_DIR="$WORKSPACE/plat_bvstk/ps7_cortexa9_0/freertos10_xilinx_domain/bsp"
BSP_EXPORT_DIR="$WORKSPACE/plat_bvstk/export/plat_bvstk/sw/plat_bvstk/freertos10_xilinx_domain/bsplib"
BUILD_STATE="$WORKSPACE/.bvstk-build-state"

if [[ -f "$XSA" ]]; then
  XSA="$(cd "$(dirname "$XSA")" && pwd)/$(basename "$XSA")"
fi

state_value() {
  local key="$1"
  local value

  value="$(awk -F= -v wanted_key="$key" '$1 == wanted_key {print substr($0, index($0, "=") + 1); exit}' "$BUILD_STATE" 2>/dev/null || true)"
  printf '%s' "$value"
}

write_build_state() {
  local xsa_sha256

  if [[ ! -s "$XSA" ]]; then
    echo "ERROR: XSA '$XSA' is missing or empty; cannot record Vitis build state" >&2
    return 1
  fi
  xsa_sha256="$(sha256sum "$XSA" | awk '{print $1}')"
  mkdir -p "$WORKSPACE"
  {
    printf 'format=1\n'
    printf 'xsa_sha256=%s\n' "$xsa_sha256"
    printf 'ssh_enable=%s\n' "${BVSTK_SSH_ENABLE:-0}"
    printf 'pl_spi_diagnostic=%s\n' "${BVSTK_PL_SPI_DIAGNOSTIC:-0}"
    printf 'lwip_lib=%s\n' "${LWIP_LIB:-}"
    printf 'wolfssl_root=%s\n' "${BVSTK_WOLFSSL_ROOT:-}"
    printf 'wolfssh_root=%s\n' "${BVSTK_WOLFSSH_ROOT:-}"
  } > "$BUILD_STATE"
}

check_build_state() {
  local key expected actual
  local xsa_sha256

  if [[ ! -s "$BUILD_STATE" ]]; then
    echo "ERROR: existing Vitis workspace has no BVSTK build-state marker:" >&2
    echo "       $BUILD_STATE" >&2
    echo "       Run './build.sh freertos' once with the default CLEAN=1." >&2
    return 1
  fi
  if [[ "$(state_value format)" != "1" ]]; then
    echo "ERROR: unsupported BVSTK Vitis workspace state format; run CLEAN=1" >&2
    return 1
  fi
  if [[ ! -s "$XSA" ]]; then
    echo "ERROR: XSA '$XSA' is missing or empty" >&2
    return 1
  fi
  xsa_sha256="$(sha256sum "$XSA" | awk '{print $1}')"
  if [[ "$(state_value xsa_sha256)" != "$xsa_sha256" ]]; then
    echo "ERROR: XSA changed since the workspace was generated; run CLEAN=1" >&2
    return 1
  fi

  for key in ssh_enable pl_spi_diagnostic lwip_lib wolfssl_root wolfssh_root; do
    case "$key" in
      ssh_enable) expected="${BVSTK_SSH_ENABLE:-0}" ;;
      pl_spi_diagnostic) expected="${BVSTK_PL_SPI_DIAGNOSTIC:-0}" ;;
      lwip_lib) expected="${LWIP_LIB:-}" ;;
      wolfssl_root) expected="${BVSTK_WOLFSSL_ROOT:-}" ;;
      wolfssh_root) expected="${BVSTK_WOLFSSH_ROOT:-}" ;;
    esac
    actual="$(state_value "$key")"
    if [[ "$actual" != "$expected" ]]; then
      echo "ERROR: Vitis workspace profile differs for '$key'; run CLEAN=1" >&2
      echo "       workspace='$actual' requested='$expected'" >&2
      return 1
    fi
  done
}

check_reusable_workspace() {
  local required
  local -a required_paths=(
    "$APP_BUILD_DIR/makefile"
    "$APP_BUILD_DIR/../src/lscript.ld"
    "$APP_BUILD_DIR/../src/Xilinx.spec"
    "$WORKSPACE/plat_bvstk/hw/design.xsa"
    "$WORKSPACE/plat_bvstk/export/plat_bvstk/plat_bvstk.xpfm"
    "$BSP_DIR/Makefile"
    "$BSP_EXPORT_DIR/lib/libxil.a"
  )

  for required in "${required_paths[@]}"; do
    if [[ ! -e "$required" ]]; then
      echo "ERROR: reusable Vitis workspace is incomplete (missing '$required')." >&2
      echo "       Run './build.sh freertos' with the default CLEAN=1." >&2
      return 1
    fi
  done
  if ! cmp -s "$XSA" "$WORKSPACE/plat_bvstk/hw/design.xsa"; then
    echo "ERROR: workspace hardware handoff differs from '$XSA'; run CLEAN=1" >&2
    return 1
  fi

  # Vitis generates this source view once.  GNU make can rebuild changed files,
  # but it cannot discover a source file added after the Vitis project was made.
  # Refuse that case instead of silently producing an incomplete ELF.
  local source_root source_file relative_file
  local -a source_roots=(
    "apps/freertos"
    "drivers"
    "hardware"
    "ports/freertos-xilinx/board"
    "ports/freertos-xilinx/fs-fatfs"
    "ports/freertos-xilinx/os"
    "ports/freertos-xilinx/storage"
    "protocols"
    "services"
    "shared"
    "vendor/lwip"
  )
  for source_root in "${source_roots[@]}"; do
    if [[ ! -d "$REPO_ROOT/src/$source_root" || ! -d "$WORKSPACE/app_bvstk/src/$source_root" ]]; then
      echo "ERROR: Vitis source view is incomplete at '$source_root'; run CLEAN=1" >&2
      return 1
    fi
    while IFS= read -r -d '' source_file; do
      relative_file="${source_file#"$REPO_ROOT/src/$source_root/"}"
      if [[ ! -e "$WORKSPACE/app_bvstk/src/$source_root/$relative_file" ]]; then
        echo "ERROR: source file '$source_root/$relative_file' is not in the Vitis source view; run CLEAN=1" >&2
        return 1
      fi
    done < <(find "$REPO_ROOT/src/$source_root" -type f -print0)
    while IFS= read -r -d '' source_file; do
      relative_file="${source_file#"$WORKSPACE/app_bvstk/src/$source_root/"}"
      if [[ ! -e "$REPO_ROOT/src/$source_root/$relative_file" ]]; then
        echo "ERROR: stale source '$source_root/$relative_file' remains in the Vitis source view; run CLEAN=1" >&2
        return 1
      fi
    done < <(find -L "$WORKSPACE/app_bvstk/src/$source_root" -type f -print0)
  done
}

prepend_vitis_arm_toolchain() {
  local vitis_root toolchain_bin xsct_path

  if command -v arm-none-eabi-gcc >/dev/null 2>&1 &&
     command -v arm-none-eabi-ar >/dev/null 2>&1 &&
     command -v arm-none-eabi-size >/dev/null 2>&1; then
    return 0
  fi

  vitis_root="${XILINX_VITIS:-}"
  if [[ -z "$vitis_root" ]] && xsct_path="$(command -v xsct 2>/dev/null)"; then
    vitis_root="$(cd "$(dirname "$xsct_path")/.." && pwd)"
  fi
  toolchain_bin="$vitis_root/gnu/aarch32/lin/gcc-arm-none-eabi/bin"
  if [[ -x "$toolchain_bin/arm-none-eabi-gcc" ]]; then
    PATH="$toolchain_bin:$PATH"
    export PATH
  fi

  for tool in arm-none-eabi-gcc arm-none-eabi-ar arm-none-eabi-size; do
    if ! command -v "$tool" >/dev/null 2>&1; then
      echo "ERROR: $tool not found; source the Vitis environment before CLEAN=0" >&2
      return 1
    fi
  done
}

run_incremental_build() {
  local generated_config="$REPO_ROOT/src/apps/freertos/config/default_configs.h"
  local gen_script="$REPO_ROOT/tools/codegen/gen_default_configs.py"

  prepend_vitis_arm_toolchain
  command -v make >/dev/null 2>&1 || {
    echo "ERROR: make not found; cannot run CLEAN=0 incremental build" >&2
    return 1
  }
  command -v python3 >/dev/null 2>&1 || {
    echo "ERROR: python3 not found; cannot run CLEAN=0 incremental build" >&2
    return 1
  }

  echo "Reusing Vitis workspace at $WORKSPACE (CLEAN=0; GNU make incremental build)"
  if [[ ! -s "$generated_config" || "$gen_script" -nt "$generated_config" ||
        -n "$(find "$REPO_ROOT/configs" -type f -newer "$generated_config" -print -quit)" ]]; then
    python3 "$gen_script" --repo-root "$REPO_ROOT" --out "$generated_config"
  fi

  # Do not reopen the Eclipse workspace or regenerate the Vitis BSP here.
  # CLEAN=0 is intentionally limited to the generated application's GNU make
  # graph; XSA/BSP/profile changes are rejected by check_build_state above and
  # require one normal CLEAN=1 generation.
  make -C "$APP_BUILD_DIR" all
  if [[ ! -s "$APP_ELF" ]]; then
    echo "ERROR: incremental build did not produce '$APP_ELF'" >&2
    return 1
  fi
  echo "Incremental build completed."
  echo "Application ELF: $APP_ELF"
}

if [[ "$CLEAN" == "0" && -d "$WORKSPACE" ]]; then
  check_build_state
  check_reusable_workspace
  run_incremental_build
  exit 0
fi

if [[ "$CLEAN" != "0" && -d "$WORKSPACE" ]]; then
  echo "Removing existing workspace at $WORKSPACE"
  rm -rf "$WORKSPACE"
fi

if ! command -v xsct >/dev/null 2>&1; then
  echo "ERROR: xsct not found in PATH" >&2
  echo "Please source the Xilinx Vitis/Xilinx SDK environment before running this script." >&2
  exit 1
fi

export XSA CLEAN

echo "Starting XSCT build with XSA=$XSA (CLEAN=$CLEAN)"
xsct "$SCRIPT_DIR/build.tcl"
write_build_state
