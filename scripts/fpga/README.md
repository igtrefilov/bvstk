# FPGA Build Scripts

These scripts build Vivado project **Burevestnik_21** and produce:

- `design.bit`
- `design.xsa`

Output defaults to `../bvstk_hw/tmp` relative to the repository root.

## Files

- `build_fpga.sh` — main wrapper (recommended entry point)
- `build_hw.tcl` — Vivado batch implementation/export script
- `build_fpga.conf` — local machine config (editable)
- `build_fpga.conf.example` — template for new machines

## Portable setup (new machine)

After cloning repository, do this once:

1. Open config:

```bash
cd <path-to-repo>/scripts/fpga
cp -n build_fpga.conf.example build_fpga.conf
```

2. Edit `build_fpga.conf`:

- `VIVADO_BIN` — absolute path to Vivado executable
- `FPGA_DIR` — absolute path to `hw_platform/fpga` (where `Burevestnik_21.tcl` is)
- `OUTPUT_DIR` — absolute path for generated `design.bit` and `design.xsa`
- optional: `JOBS`, `PROJ_NAME`, `CLEAN`

3. Validate tool path:

```bash
"$VIVADO_BIN" -version
```

4. Run build:

```bash
./build_fpga.sh
```

5. Verify artifacts exist in `OUTPUT_DIR`:

- `design.bit`
- `design.xsa`

## If directory layout differs

Default values assume sibling repos (`../hw_platform` and `../bvstk_hw` near this repo).
If your layout is different, set `FPGA_DIR` and `OUTPUT_DIR` explicitly in `build_fpga.conf`.

## CLI overrides (optional)

CLI options override config values:

```bash
./build_fpga.sh --jobs 12
./build_fpga.sh --config /path/to/custom.conf
./build_fpga.sh --vivado /path/to/vivado
./build_fpga.sh --fpga-dir /path/to/hw_platform/fpga
./build_fpga.sh --output-dir /path/to/output
./build_fpga.sh --clean
```

## Legacy path compatibility

Old path `~/Zynq/scripts/fpga/build_fpga.sh` is kept as a wrapper and forwards to this script.
For new setups, use scripts from repository directly.

## Troubleshooting

- `Vivado executable '...' not found`:
  - fix `VIVADO_BIN` in config.
- `project ... not found`:
  - fix `FPGA_DIR` (must contain `Burevestnik_21.tcl` and/or `vivado_project`).
- output files not generated:
  - check Vivado log for `ERROR`, then rerun with `--clean`.
