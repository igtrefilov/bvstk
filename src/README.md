# bvstk firmware (Zynq‑7000, FreeRTOS + lwIP)

Firmware for Zynq‑7000 built on FreeRTOS 10 with the lwIP socket API and FatFs.

Startup sequence in `src/main.c`:
`qspi_flash_self_test()` → `start_sd_card()` → `start_qspi_fs()` → `fs_devices_init()` → `start_lan()` → `start_tcp_server()` → `start_smi()` → `start_i2c()` → `vTaskStartScheduler()`.

## Features
- Static IPv4: `192.168.0.10/24`, gateway `192.168.0.1`, MAC `00:0a:35:00:01:02` (`src/bvstk_lan/bvstk_lan.c`).
- TCP console on port `8888` (`src/bvstk_tcp_server/bvstk_tcp_server.c`) with a telnet-friendly line editor (history, arrows, tab completion).
  - Prompt shows active filesystem and current directory: `Zynq/<fs>[:path]>` (e.g. `Zynq/sd:logs> `, `Zynq/flash> `).
  - Text lines go to `process_console_line()`; binary frames go to `process_received_data()` (weak symbol you can override).
  - Built-ins: `help`, `quit`/`exit`.
  - Utilities: `fs` (help), `tar`, `ip`, `smi`, `mem`, `axp` (run `<name> -h` for usage).
- HTTP server on port `8000` (`src/http/*`). Filesystem endpoints are implemented as a separate route module (`src/http_fs/http_fs_routes.c`) so the HTTP server stays generic for a future web UI:
  - Single file:
    - `GET  /sd/<path>` / `PUT /sd/<path>` (maps to FatFs `0:/<path>`)
    - `GET  /flash/<path>` / `PUT /flash/<path>` (maps to FatFs `1:/<path>`)
  - Directory as tar stream (tar encoder/decoder lives in `src/tar/*`, reusable from console in the future):
    - `GET  /tar/sd/<dir>` / `PUT /tar/sd/<dir>`
    - `GET  /tar/flash/<dir>` / `PUT /tar/flash/<dir>`
- Filesystem commands: `pwd`, `ls [path]`, `cd <dir>`, `cd flash|cd sd`, `mkdir`, `touch`, `cat`, `rm`, `cp`, `cp -r`, `mv` (see `fs -h`).
- Prefix paths with `sd:/` or `flash:/` when you need to address the other device directly (e.g., `cp sd:/test/file flash:/backup/file`). The same prefixes work with `cp -r`, `cat`, `mv`, etc.
- I2C master/slave driver with event queues, whitelist/blacklist policy, and autopolling of registers (`src/bvstk_i2c/*`).
- SMI/MDIO support with PHY register polling and host→slave event filtering (`src/bvstk_smi/*`).
- SD card + FatFs on PS SDIO0: background task mounts `0:/`; if mount fails it runs `f_mkfs()` and retries (`src/sd_card/*`, `src/fs/fs_shared.c`).
- QSPI flash (Winbond W25Q family opcodes, 32 MiB) with a simple driver and self-test (`src/qspi_flash/*`).
- QSPI-backed FatFs is exposed as `1:/` and shares the same FS helpers; `cd flash` selects it in the console (`src/qspi_fs/*`).
- Stubs for MQTT, SNTP, and UART console (`src/mqtt_proc/*`, `src/sntp_proc/*`, `src/uart_console/*`).

## Layout
- Repo root:
  - `build.tcl` — XSCT/Vitis build script: creates `vitis_ws/`, platform `plat_bvstk` (from XSA), BSP with FreeRTOS + lwIP + xilffs, links `src/` into the app project, builds the ELF.
  - `build.sh` — wrapper around `xsct build.tcl` (supports `XSA`/`CLEAN` env vars).
  - `run_jtag.sh` / `run_jtag.tcl` — program bitstream, init PS7, download ELF, run over JTAG.
- `src/` — firmware sources (tracked in git; symlinked into `vitis_ws/app_bvstk/src/` by `build.tcl`).

## New machine: build + run (step-by-step)
This project is intended to be built and launched via the provided scripts (`build.sh`, `run_jtag.sh`). On a new machine you must point them at your hardware files (`*.xsa` and usually `*.bit`).

### Prerequisites
- Xilinx Vitis/XSCT installed (Linux/Windows both work if `xsct` is available).
- Xilinx cable drivers / access to JTAG (and `hw_server` available).
- `python3` (used by `build.tcl` to patch FatFs config).

On Linux you typically need to source the Xilinx environment in every new terminal before running scripts:
```
source <Vitis-install>/settings64.sh
```

Sanity check:
```
xsct -version
```

### Hardware inputs: where to get `*.xsa` and `*.bit`
- `XSA` (`*.xsa`) is the hardware export from Vivado for your design.
- `BIT` (`*.bit`) is the bitstream to program the PL over JTAG.

Recommended: export an XSA **with bitstream included** (Vivado “Export Hardware” with “Include bitstream”). In that case you often can use the bitstream that Vitis exports under the platform folder, and you don’t need to manage a separate `.bit` manually.

### Build (creates `vitis_ws/` and the ELF)
From the repo root:
```
XSA=/abs/path/to/your/design.xsa ./build.sh
```

Useful knobs:
- Reuse an existing workspace (faster iterations):
  ```
  XSA=/abs/path/to/your/design.xsa CLEAN=0 ./build.sh
  ```
- Force lwIP library selection (if your Vitis install differs):
  ```
  XSA=/abs/path/to/your/design.xsa LWIP_LIB=lwip220 ./build.sh
  ```

Build outputs:
- ELF: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`
- Platform export (incl. default bitstream and `ps7_init.tcl` when available): `vitis_ws/plat_bvstk/export/plat_bvstk/hw/`

If you need to change the default XSA path used by the scripts:
- `build.sh`: update `DEFAULT_XSA=".../Burevestnik_top.xsa"` (near the top), or always pass `XSA=...`.
- `build.tcl`: update the hardcoded default in the “Tooling / design inputs” block, or always pass `XSA=...`.

### Run over JTAG (program PL, init PS7, load ELF, run)
1. Make sure the board is connected over JTAG and the Xilinx environment is sourced.
2. Run:
   ```
   ./run_jtag.sh /abs/path/to/your/design.bit
   ```
   If you omit the argument, `run_jtag.sh` runs `xsct run_jtag.tcl` without overriding the bitstream, and `run_jtag.tcl` will try:
   1) `BITSTREAM_PATH_OVERRIDE` (hardcoded inside `run_jtag.tcl`)
   2) `BITSTREAM_FILE` env var
   3) `BITSTREAM_DEFAULT` from the Vitis platform export under `vitis_ws/plat_bvstk/export/.../hw/`

Important for a new machine:
- `run_jtag.tcl` contains a hardcoded path `set BITSTREAM_PATH_OVERRIDE "..."` near the top. You should either:
  - set it to `""` and pass the bitstream via `./run_jtag.sh /abs/path/to/design.bit` (or `BITSTREAM_FILE=...`), or
  - replace it with a valid path on your machine.

After successful run, connect to the TCP console:
```
telnet 192.168.0.10 8888
```

## TCP console: `ip` utility
The console includes a small Linux-like `ip` command for inspecting and changing the network settings.

Show current interface settings:
```
ip addr show
ip link show
ip route show
```

Set IPv4 address and default gateway (applies immediately and persists to QSPI config):
```
ip addr set 192.168.0.10/24
ip route set default via 192.168.0.1
ip link set address 00:0a:35:00:01:02
```

Persist current runtime settings into `flash:/configs/network.json`:
```
ip save
```

Note: changing IP can drop the current TCP session.

## HTTP file transfer (curl/wget)
Base URL: `http://192.168.0.10:8000`

Single file:
```
curl -T local.bin http://192.168.0.10:8000/flash/fw/local.bin
curl -o local.bin http://192.168.0.10:8000/sd/logs/local.bin
```

Directories (tar stream, no compression):
```
curl http://192.168.0.10:8000/tar/flash/cfg | tar -xf -
tar -cf - ./cfg | curl -T - http://192.168.0.10:8000/tar/flash/cfg
```

## Build (quick)
See **New machine: build + run (step-by-step)** for full setup details.

From the repo root:
```
XSA=/abs/path/to/your/design.xsa ./build.sh
```
Output ELF: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`.

## Configuration knobs
- IP/mask/gateway — `src/bvstk_lan/bvstk_lan.c`.
- TCP console port — `echo_port` in `src/bvstk_tcp_server/bvstk_tcp_server.c`.
- I2C policy (whitelist/blacklist, autopolling) — `src/bvstk_i2c/bvstk_i2c.h` and `i2cdev_autopoll_profile` in `src/bvstk_i2c/bvstk_i2c.c`.
- SMI/MDIO registers and IRQ masks — `src/bvstk_smi/bvstk_smi.h`.
- SD mount point and task params — `SD_ROOT`, `SD_TASK_STACK`, `SD_TASK_PRIO` in `src/sd_card/sd_card.h` and `src/sd_card/sd_card.c`.
- QSPI mount point and task params — `QSPI_ROOT`, `QSPI_TASK_STACK`, `QSPI_TASK_PRIO` in `src/qspi_fs/qspi_fs.h` and `src/qspi_fs/qspi_fs.c`.
- Shell session init (default FS + cwd) — `console_session_init()` in `src/bvstk_tcp_server/utils/console_common.c`.

## SD card/FatFs usage
- Startup: `main()` starts SD + QSPI FS tasks early; each task retries mount until its media becomes available.
- Auto-format: `fs_shared_mount()` falls back to `f_mkfs()` only when `f_mount()` returns `FR_NO_FILESYSTEM` (`src/fs/fs_shared.c`).
- Concurrency: each FS context has its own mutex; shared helpers lock internally.
- Cross-device copy/move: console commands `cp`, `cp -r`, `mv` support `sd:/` and `flash:/` prefixes.

## Filesystem commands (linux-like)
The filesystem commands are intentionally **linux-like** (simple `ls/cd/pwd/cp/mv/rm` semantics) but operate on FatFs paths.

### Paths and devices
- Active device is shown in the prompt: `Zynq/sd...` or `Zynq/flash...`.
- `cd sd` / `cd flash` switches the active filesystem (and resets `cwd` to its root).
- Absolute paths start with `/` and are resolved relative to the active device root (`0:/` for SD, `1:/` for QSPI flash).
- To address the other device without switching, use device prefixes:
  - `sd:/path/...` (SD card, `0:/...`)
  - `flash:/path/...` (QSPI flash FS, `1:/...`)

### Command reference
- `pwd` — print current working directory (FatFs path, e.g. `0:/logs`).
- `ls [path]` — list directory (`ls`, `ls /`, `ls sd:/`, `ls flash:/logs`).
- `cd <dir>` — change directory (`cd /`, `cd logs`, `cd flash`, `cd sd`).
- `mkdir <dir>` — create directory (idempotent if already exists).
- `touch <file>` — create empty file if missing.
- `cat <file>` — print file contents.
- `rm <file|dir>` — remove file or directory (directories are removed recursively).
- `cp <src> <dst>` — copy file (destination can be a directory).
- `cp -r <src> <dst>` — recursive directory copy.
- `mv <src> <dst>` — move/rename; when source and destination are on different devices it performs copy+remove.

### Examples
Create a directory and a file on SD:
```
mkdir logs
cd logs
touch note.txt
pwd
ls
```

Copy from SD to flash **without** switching devices:
```
cp sd:/logs/note.txt flash:/backup/note.txt
```

Switch to flash, inspect, then go back to SD:
```
cd flash
pwd
ls /
cd sd
pwd
```

Move a directory from SD to flash (cross-device `mv`):
```
mv sd:/logs flash:/logs_backup
```

## Tar utility (console)
`tar` is a small, linux-like console utility for creating/extracting `.tar` archives inside FatFs. It does **not** stream binary tar over the telnet session; instead it reads/writes tar files on SD/QSPI, which you can then transfer via HTTP.

Commands:
- `tar c <src_dir> <dst_tar>` — create tar file from a directory.
- `tar x <src_tar> <dst_dir>` — extract tar file into a directory.
- `tar t <src_tar>` — list entries in a tar file.

Examples:
```
tar c logs sd:/backup/logs.tar
tar t sd:/backup/logs.tar
tar x sd:/backup/logs.tar /restore
```

## Quick test
1. Connect: `telnet 192.168.0.10 8888` (recommended) or `nc 192.168.0.10 8888`.
2. Try:
   - `help`
   - `fs -h`
   - `ls`
   - `mkdir logs`
   - `cd logs`
   - `touch note.txt`
   - `cat note.txt`
   - `cp sd:/logs/note.txt flash:/backup_note.txt`

## Run over JTAG (quick)
See **New machine: build + run (step-by-step)** for bitstream path selection details.

From repo root:
```
./run_jtag.sh /abs/path/to/your/design.bit
```

## License
Not specified yet. Add a `LICENSE` file if distribution is required.
