# bvstk firmware (FreeRTOS + lwIP)

Firmware for Zynq‑7000 built on FreeRTOS 10 with the lwIP socket API. Main services start in `main()`:
`start_lan()` → `start_tcp_server()` → `start_smi()` → `start_i2c()` → `vTaskStartScheduler()`.

## Features
- Static IPv4: `192.168.0.10/24`, gateway `192.168.0.1`, MAC `00:0a:35:00:01:02` (`src/bvstk_lan/bvstk_lan.c`).
- TCP console on port `8888` with prompt `Zynq>`; text lines go to `process_console_line()`, binary frames to `process_received_data()` (`src/bvstk_tcp_server/bvstk_tcp_server.c`). Console now exposes a tiny shell for the SD card and QSPI flash—switch disks with `fs use sd` or `fs use qspi` (blank flash formats on demand) and run `pwd`, `ls [path]`, `cd`, `mkdir`, `touch`, `cat`, `rm`.
- I2C master/slave driver with event queues, whitelist/blacklist policy, and autopolling of registers (`src/bvstk_i2c/*`).
- SMI/MDIO support with PHY register polling and host→slave event filtering (`src/bvstk_smi/*`).
- SD card + FatFs on PS SDIO0: background task mounts `0:/`, auto-formats blank cards, and exposes helpers to fetch capacity, list root dir, read files, and append/overwrite text (`src/sd_card/*`).
- QSPI flash (Winbond W25Q series) is mirrored into the same FatFs stack at `1:/`; `fs use qspi` picks that disk in the shell and formats a fresh flash before exposing files (`src/qspi_fs/*`).
- Stubs for MQTT, SNTP, and UART console (`src/mqtt_proc/*`, `src/sntp_proc/*`, `src/uart_console/*`).

## Layout
- `build.tcl` — XSCT/Vitis build script: creates `vitis_ws/`, platform `plat_bvstk` (from XSA), BSP with lwIP, and app `app_bvstk`.
- `src/` — all firmware sources (tracked in git).

## Build with XSCT/Vitis
1. Set the XSA path in `build.tcl` to your hardware design, e.g.:
   ```
   set XSA {/path/to/your/Burevestnik_top.xsa}
   ```
   (Only this variable should need changing.)
2. From repo root run:
   ```
   xsct build.tcl
   ```
   This creates `vitis_ws/` and symlinks `src/` into the app workspace.
3. Build the app:
   ```
   xsct
   xsct% setws ./vitis_ws
   xsct% app build -name app_bvstk
   ```
   Output ELF: `vitis_ws/app_bvstk/Debug/`.

FatFs is pulled in automatically by `build.tcl` (`xilffs` library, SD interface, IRQ mode). No extra BSP tweaking is needed unless you change the SDIO instance.

## Configuration knobs
- IP/mask/gateway — `src/bvstk_lan/bvstk_lan.c`.
- TCP console port — `echo_port` in `src/bvstk_tcp_server/bvstk_tcp_server.c`.
- I2C policy (whitelist/blacklist, autopolling) — `src/bvstk_i2c/bvstk_i2c.h` and `i2cdev_autopoll_profile` in `bvstk_i2c.c`.
- SMI/MDIO registers and IRQ masks — `src/bvstk_smi/bvstk_smi.h`.
- SD mount point and task params — `SD_MOUNT_POINT`, `SD_TASK_STACK`, `SD_TASK_PRIO` in `src/sd_card/sd_card.c`.
- Shell working directory defaults to `0:/`; see `console_session_init()` in `src/bvstk_tcp_server/utils/utils.c`.

## SD card/FatFs usage
- Startup: `main()` calls `start_sd_card()` before networking; the task initializes SDIO, mounts FatFs at `0:/`, and formats if the card is blank.
- Info: `sd_card_get_info()` returns block size/count, total capacity, and a `mounted` flag.
- File helpers (all write to the TCP console socket you pass):
  - `sd_card_ls(fd)` — list root directory (legacy).
  - `sd_card_cat(path, fd)` — dump a file.
  - `sd_card_write_text(path, text, append)` — create/overwrite or append a text file.
  - `sd_fs_*()` — console-ready wrappers: `ls`, `cat`, `touch`, `mkdir`, `rm`, `is_dir`.
- `fs use <device>` — swap the shell between `sd` and `qspi`, reusing the same helpers for whichever disk is selected.
- Concurrency: a mutex is created inside `start_sd_card()`; use it (or your own) around multi-call sequences if you add more SD operations.

## Quick test
- Connect to console: `telnet 192.168.0.10 8888` or `nc 192.168.0.10 8888`, then run `pwd`, `ls`, `cd /logs`, `touch note.txt`, `cat note.txt`.
- Change port/address as needed and rebuild.

## License
Not specified yet. Add a `LICENSE` file if distribution is required.
