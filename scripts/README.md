# Scripts Layout

This directory is split by purpose:

- `scripts/fpga/` — FPGA build scripts (`build_fpga.sh`, `build_hw.tcl`, configs)
- `scripts/vitis/` — Vitis build/JTAG scripts (`build.sh`, `build.tcl`, `run_jtag.sh`, `run_jtag.tcl`, configs)
- `scripts/vscode/` — VSCode/debug helper scripts (`arm-none-eabi-gdb.sh`, `gen_compile_commands.sh`, `jtag_prepare_debug.tcl`)
- `scripts/compat/` — legacy wrappers for old script paths (kept for backward compatibility)

Compatibility wrappers are kept in `scripts/compat/` for old paths.

## New Machine Checklist

1. Configure FPGA build script in `scripts/fpga/build_fpga.conf`:
`VIVADO_BIN`, `FPGA_DIR`, `OUTPUT_DIR`.
2. Configure Vitis/JTAG scripts in `scripts/vitis/*.conf` if paths are non-default.
3. Use VSCode helpers from `scripts/vscode/`.
4. If needed, legacy wrappers are available in `scripts/compat/`.
