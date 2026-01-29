# bvstk (Zynq-7000 / Vitis / FreeRTOS)

This repository is primarily built and flashed via Xilinx Vitis (XSCT).

## Build

```bash
source <Vitis>/settings64.sh
./build.sh
```

## Run via JTAG

```bash
source <Vitis>/settings64.sh
./run_jtag.sh
```

## VSCode: code navigation (BSP/platform)

See `docs/VSCODE_XILINX.md`.

Quick start:

```bash
sudo apt install -y bear
./scripts/gen_compile_commands.sh
```

Then open the repo root in VSCode and use `F12` / IntelliSense.

## VSCode: step debug via JTAG (AX7020 / Zynq-7000 + FreeRTOS)

See `docs/VSCODE_DEBUG_ZYNQ7000_JTAG.md`.

Prepare target for debug (program PL + PS7 init + halt core0):

```bash
source <Vitis>/settings64.sh
./run_jtag.sh --debug
```

Then in VSCode:
- `Run and Debug` → `Attach: Zynq-7000 (core0, after ./run_jtag.sh --debug)` → `F5`.

## New Machine Setup

- Install Xilinx Vitis and ensure `xsct`/`hw_server` are available (typically via `source <Vitis>/settings64.sh`).
- Install VSCode + `ms-vscode.cpptools`.
- Install `bear` (for navigation): `sudo apt install -y bear` and run `./scripts/gen_compile_commands.sh`.
- For JTAG step debug: follow `docs/VSCODE_DEBUG_ZYNQ7000_JTAG.md`.
