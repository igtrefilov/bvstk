# VSCode Guide (Xilinx/Vitis)

This guide combines:

- IntelliSense/navigation setup for BSP/platform sources
- JTAG step-debug setup for Zynq-7000 + FreeRTOS

## 1) Requirements

- VSCode + `ms-vscode.cpptools`
- Xilinx tools in `PATH`: `xsct`, `hw_server`
- ARM toolchain: `arm-none-eabi-gcc`, `arm-none-eabi-gdb`
- `bear` (for `compile_commands.json` generation)

Quick checks:

```bash
xsct -h
hw_server -h
arm-none-eabi-gcc --version
arm-none-eabi-gdb --version
bear --version
```

If toolchain is not in `PATH`, set:

```bash
export VITIS_ARM_GCC_BIN=/path/to/.../gcc-arm-none-eabi/bin
```

## 2) IntelliSense / Navigation

Generate compilation database:

```bash
cd <repo-root>
./scripts/vscode/gen_compile_commands.sh
```

This creates `compile_commands.json` in repo root. It is local/machine-specific and should be regenerated per machine.

VSCode uses:

- `.vscode/c_cpp_properties.json`
- `.vscode/settings.json`

with `${workspaceFolder}/compile_commands.json`.

If navigation is stale:

1. `Developer: Reload Window`
2. `C/C++: Reset IntelliSense Database`

## 3) JTAG Step Debug (Zynq-7000)

Debug flow needs:

1. Program PL bitstream
2. Run `ps7_init`
3. Halt CPU before GDB attach

This is handled by `scripts/vscode/jtag_prepare_debug.tcl`.

VSCode setup in repo:

- `.vscode/tasks.json` starts `hw_server` and runs XSCT prepare script
- `.vscode/launch.json` attaches GDB (`target remote :3000`), loads ELF, sets breakpoint at `main`

Typical flow:

1. Build app (`vitis_ws/app_bvstk/Debug/app_bvstk.elf` exists)
2. Open repo root in VSCode
3. Start debug config `Attach: Zynq-7000 (hw_server GDB, core0)`

CLI alternative:

```bash
./scripts/vitis/run_jtag.sh --debug [path/to/design.bit]
```

Then attach with VSCode/GDB.

## 4) Bitstream / path overrides

Default debug bitstream path:

- `vitis_ws/plat_bvstk/export/plat_bvstk/hw/Burevestnik_top.bit`

Override if needed:

```bash
export BITSTREAM_FILE=/abs/path/to/design.bit
```

## 5) Troubleshooting

- `xsct`/`hw_server` not found:
  - source Xilinx environment (`settings64.sh`)
- `arm-none-eabi-*` not found:
  - export `VITIS_ARM_GCC_BIN` or extend `PATH`
- Breakpoints not hit:
  - build with debug flags (`-g`, preferably `-O0`/`-Og`)
  - ensure JTAG prepare was run before attach
- Port 3000 busy:
  - change GDB base port in `.vscode/tasks.json` and matching target in `.vscode/launch.json`
