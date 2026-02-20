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

- `build_vitis.conf`: set `XSA` and/or `CLEAN_DEFAULT`
- `run_jtag.conf`: set `BITSTREAM_FILE`, `ELF_FILE`, `PS7_INIT_TCL`

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
