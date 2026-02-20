#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${ROOT_DIR}/compile_commands.json"

# Override if you use a different Vitis version or installed path.
TOOLCHAIN_BIN="${VITIS_ARM_GCC_BIN:-}"
if [[ -z "${TOOLCHAIN_BIN}" ]]; then
  if GCC_PATH="$(command -v arm-none-eabi-gcc 2>/dev/null)"; then
    TOOLCHAIN_BIN="$(dirname "${GCC_PATH}")"
  else
    TOOLCHAIN_BIN="$(
      ls -1d \
        "${HOME}/Xilinx/Vitis"/*/gnu/aarch32/lin/gcc-arm-none-eabi/bin \
        /opt/Xilinx/Vitis/*/gnu/aarch32/lin/gcc-arm-none-eabi/bin \
        2>/dev/null | sort -V | tail -n 1 || true
    )"
  fi
fi

if [[ -z "${TOOLCHAIN_BIN}" || ! -x "${TOOLCHAIN_BIN}/arm-none-eabi-gcc" ]]; then
  echo "ERROR: arm-none-eabi-gcc not found. Add it to PATH or set VITIS_ARM_GCC_BIN to the toolchain bin dir." >&2
  exit 1
fi
export PATH="${TOOLCHAIN_BIN}:${PATH}"

BEAR_LIB="${BEAR_LIBRARY_PATH:-}"
if [[ -z "${BEAR_LIB}" ]]; then
  if [[ -f /usr/lib/x86_64-linux-gnu/bear/libexec.so ]]; then
    BEAR_LIB="/usr/lib/x86_64-linux-gnu/bear/libexec.so"
  elif [[ -f /usr/lib64/bear/libexec.so ]]; then
    BEAR_LIB="/usr/lib64/bear/libexec.so"
  elif [[ -f /usr/lib/bear/libexec.so ]]; then
    BEAR_LIB="/usr/lib/bear/libexec.so"
  else
    echo "ERROR: Bear preload library not found. Check bear installation." >&2
    exit 1
  fi
fi

cd "${ROOT_DIR}"
rm -f "${OUT}"

BEAR=(bear --library "${BEAR_LIB}" --force-preload --output "${OUT}" --append)

"${BEAR[@]}" -- make -C vitis_ws/app_bvstk/Debug -B -j"$(nproc)"
"${BEAR[@]}" -- make -C vitis_ws/plat_bvstk/zynq_fsbl -B -j"$(nproc)"
"${BEAR[@]}" -- make -C vitis_ws/plat_bvstk/zynq_fsbl/zynq_fsbl_bsp -B -j"$(nproc)"
"${BEAR[@]}" -- make -C vitis_ws/plat_bvstk/ps7_cortexa9_0/freertos10_xilinx_domain/bsp -B -j"$(nproc)"

echo "Wrote ${OUT}"
