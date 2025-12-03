# bvstk firmware (FreeRTOS + lwIP)

Firmware for Zynq‑7000 built on FreeRTOS 10 with the lwIP socket API. Main services start in `main()`:
`start_lan()` → `start_tcp_server()` → `start_smi()` → `start_i2c()` → `vTaskStartScheduler()`.

## Features
- Static IPv4: `192.168.0.10/24`, gateway `192.168.0.1`, MAC `00:0a:35:00:01:02` (`src/bvstk_lan/bvstk_lan.c`).
- TCP console on port `8888` with prompt `Zynq>`; text lines go to `process_console_line()`, binary frames to `process_received_data()` (`src/bvstk_tcp_server/bvstk_tcp_server.c`).
- I2C master/slave driver with event queues, whitelist/blacklist policy, and autopolling of registers (`src/bvstk_i2c/*`).
- SMI/MDIO support with PHY register polling and host→slave event filtering (`src/bvstk_smi/*`).
- Stubs for MQTT, SNTP, SD card, and UART console (`src/mqtt_proc/*`, `src/sntp_proc/*`, `src/sd_card/*`, `src/uart_console/*`).

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

## Configuration knobs
- IP/mask/gateway — `src/bvstk_lan/bvstk_lan.c`.
- TCP console port — `echo_port` in `src/bvstk_tcp_server/bvstk_tcp_server.c`.
- I2C policy (whitelist/blacklist, autopolling) — `src/bvstk_i2c/bvstk_i2c.h` and `i2cdev_autopoll_profile` in `bvstk_i2c.c`.
- SMI/MDIO registers and IRQ masks — `src/bvstk_smi/bvstk_smi.h`.

## Quick test
- Connect to console: `telnet 192.168.0.10 8888` or `nc 192.168.0.10 8888`.
- Change port/address as needed and rebuild.

## License
Not specified yet. Add a `LICENSE` file if distribution is required.
