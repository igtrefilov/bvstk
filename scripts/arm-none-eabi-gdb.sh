#!/usr/bin/env bash
set -euo pipefail

# Wrapper for VSCode/cppdbg to reliably find ARM GDB without depending on VSCode's PATH.
#
# Resolution order:
#  1) $VITIS_ARM_GDB (full path)
#  2) $VITIS_ARM_GCC_BIN/arm-none-eabi-gdb
#  3) /home/ilya/Xilinx/Vitis/<latest>/gnu/aarch32/lin/gcc-arm-none-eabi/bin/arm-none-eabi-gdb
#  4) arm-none-eabi-gdb from PATH

# VSCode sometimes starts the MI debugger with an unexpected working directory.
# Ensure GDB runs from the repo root so relative paths (like `vitis_ws/...`) work.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -n "${GDB_CWD:-}" ]]; then
  cd "${GDB_CWD}"
else
  cd "${REPO_ROOT}"
fi

if [[ -n "${VITIS_ARM_GDB:-}" && -x "${VITIS_ARM_GDB}" ]]; then
  exec "${VITIS_ARM_GDB}" "$@"
fi

if [[ -n "${VITIS_ARM_GCC_BIN:-}" && -x "${VITIS_ARM_GCC_BIN}/arm-none-eabi-gdb" ]]; then
  exec "${VITIS_ARM_GCC_BIN}/arm-none-eabi-gdb" "$@"
fi

candidate="$(ls -1d /home/ilya/Xilinx/Vitis/*/gnu/aarch32/lin/gcc-arm-none-eabi/bin 2>/dev/null | sort -V | tail -n 1 || true)"
if [[ -n "${candidate}" && -x "${candidate}/arm-none-eabi-gdb" ]]; then
  exec "${candidate}/arm-none-eabi-gdb" "$@"
fi

if command -v arm-none-eabi-gdb >/dev/null 2>&1; then
  exec arm-none-eabi-gdb "$@"
fi

echo "ERROR: arm-none-eabi-gdb not found." >&2
echo "Set VITIS_ARM_GCC_BIN to the toolchain bin dir or VITIS_ARM_GDB to the full gdb path." >&2
exit 1
