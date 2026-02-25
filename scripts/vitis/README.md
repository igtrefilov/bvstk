# Vitis Build / JTAG Scripts

This folder contains project-level Vitis/JTAG scripts:

- `build.sh` + `build.tcl` — create/update `vitis_ws`, generate platform/BSP, build ELF
- `run_jtag.sh` + `run_jtag.tcl` — JTAG run (program PL, init PS7, load ELF, run)

## New machine setup

1. (Optional) copy config templates:

```bash
cd <repo>/scripts/vitis
cp -n build_vitis.conf.example build_vitis.conf
cp -n run_jtag.conf.example run_jtag.conf
```

2. If your layout differs, edit configs:

- `build_vitis.conf`: set `XSA`, optional `XILINX_SETTINGS`, and/or `CLEAN_DEFAULT`
- `run_jtag.conf`: set optional `XILINX_SETTINGS`, plus `BITSTREAM_FILE`, `ELF_FILE`, `PS7_INIT_TCL` if needed

Default XSA path:
- `<repo>/artifacts/fpga/design.xsa`
Override it in `build_vitis.conf` if your artifacts are elsewhere.

Default JTAG bitstream lookup order:
1. `<repo>/artifacts/fpga/design.bit`
2. `<repo>/../bvstk_hw/tmp/design.bit` (legacy)
3. `<repo>/vitis_ws/plat_bvstk/export/plat_bvstk/hw/Burevestnik_top.bit`

3. Build firmware:

```bash
./build.sh
```

4. Run over JTAG:

```bash
./run_jtag.sh
# or debug-prepare mode:
./run_jtag.sh --debug
```

## Compatibility

Repository root keeps wrappers (`./build.sh`, `./build.tcl`, `./run_jtag.sh`, `./run_jtag.tcl`) forwarding to this folder.
