# bvstk firmware (Zynq‑7000, FreeRTOS + lwIP + FatFs)

Прошивка для Zynq‑7000 на FreeRTOS 10 с lwIP (socket API) и FatFs.

Ключевая идея проекта: устройство поднимает сеть, предоставляет TCP‑консоль “как telnet” и HTTP‑эндпойнты для передачи файлов/директорий, а также имеет два тома FatFs:

- `sd` → `0:/` (SD карта)
- `flash` → `1:/` (QSPI flash, окно файловой системы начиная с `QSPI_FS_BASE_BYTES`)

## Table of contents
- [Features](#features)
- [Repository layout](#repository-layout)
- [Runtime: startup sequence](#runtime-startup-sequence)
- [Filesystems](#filesystems)
- [Configuration](#configuration)
  - [Network config file](#network-config-file)
  - [Bootstrap behavior](#bootstrap-behavior)
  - [Ways to edit config](#ways-to-edit-config)
- [Build & run](#build-run)
  - [Prerequisites](#prerequisites)
  - [Build](#build)
  - [Run over JTAG](#run-over-jtag)
  - [Connect to TCP console](#connect-to-tcp-console)
- [TCP console](#tcp-console)
  - [Built-ins](#built-ins)
  - [fs — filesystem](#fs-filesystem)
  - [tar — archive](#tar-archive)
  - [ip — network](#ip-network)
  - [smi — MDIO/SMI](#smi-mdiosmi)
  - [mem — memory](#mem-memory)
  - [axp — AXP15060 (I2C)](#axp-axp15060-i2c)
- [HTTP file transfer](#http-file-transfer)
- [Quick verification checklist](#quick-verification-checklist)
- [Troubleshooting](#troubleshooting)

## Features
- TCP console (telnet-friendly) on port `8888` with line editor (history, arrows, tab completion).
- HTTP server on port `8000` with file/dir transfer endpoints (see [HTTP file transfer](#http-file-transfer)).
- SD FatFs (`0:/`, alias `sd:/`) with auto-mount and auto-format on “no filesystem”.
- QSPI FatFs (`1:/`, alias `flash:/`) mapped away from boot area (`QSPI_FS_BASE_BYTES`).
- Network configuration stored as editable JSON on QSPI: `flash:/configs/network.json`.
- Console utilities:
  - `fs` (filesystem shell), `tar` (tar create/list/extract), `ip` (Linux-like network utility),
  - `smi` (MDIO read/write), `mem` (peek/poke), `axp` (AXP15060 status + access + policy/rules).

## Repository layout
- Repo root:
  - `build.sh` — wrapper around XSCT (`xsct build.tcl`).
  - `build.tcl` — creates `vitis_ws/`, generates platform/BSP (FreeRTOS + lwIP + xilffs), links `src/` into the app project, builds ELF.
  - `run_jtag.sh` / `run_jtag.tcl` — program bitstream, init PS7, download ELF, run via JTAG.
  - `configs/network.json` — default network configuration template (embedded into firmware at build time).
- `src/` — firmware sources (symlinked into `vitis_ws/app_bvstk/src/` by `build.tcl`).

## Runtime: startup sequence
Текущий порядок инициализации в `src/main.c`:

`qspi_flash_self_test()` → `start_sd_card()` → `start_qspi_fs()` → `fs_devices_init()` → `start_config_store()` → `start_lan()` → `start_tcp_server()` → `start_http_server()` → `start_smi()` → `start_i2c()` → `vTaskStartScheduler()`.

## Filesystems
- `sd` (SDIO0) монтируется как `0:/` (alias `sd:/` в консоли).
- `flash` (QSPI FatFs) монтируется как `1:/` (alias `flash:/` в консоли).

QSPI‑том работает не “поверх всего флеша”, а внутри окна:
- базовый адрес: `QSPI_FS_BASE_BYTES` (см. `src/qspi_fs/qspi_fs_layout.h`)
- размер: `QSPI_FS_SIZE_BYTES`

Это сделано, чтобы не трогать BOOT‑область в начале QSPI.

## Configuration

### Network config file
Основной конфиг сети: `flash:/configs/network.json`.

Текущий формат (IPv4 + MAC):
```json
{
  "ipv4": {
    "ip": "192.168.0.10",
    "netmask": "255.255.255.0",
    "gateway": "192.168.0.1"
  },
  "mac": "00:0a:35:00:01:02"
}
```

### Bootstrap behavior
При старте `config_store` делает следующее:
- убеждается, что QSPI‑том смонтирован;
- создаёт директорию `flash:/configs/`, если её нет;
- создаёт `flash:/configs/network.json`, **только если файла ещё нет**;
- загружает настройки в RAM (если QSPI недоступен — использует встроенный дефолт).

Дефолт берётся из `configs/network.json` (в репозитории) и вшивается в прошивку во время сборки.

### Ways to edit config
Есть несколько практичных способов управлять настройками:

1) Через TCP‑консоль утилитой `ip` (см. [ip — network](#ip-network)):
- меняет настройки и сохраняет обратно в `flash:/configs/network.json`;
- может оборвать текущую TCP‑сессию при смене IP.

2) Через HTTP PUT (см. [HTTP file transfer](#http-file-transfer)):
```
curl -T network.json http://<device-ip>:8000/flash/configs/network.json
```

3) Через консоль `fs` копированием между `sd:/` и `flash:/`:
```
cp sd:/configs/network.json flash:/configs/network.json
```

## Build & run

### Prerequisites
- Xilinx Vitis/XSCT установлен, `xsct` доступен в PATH.
- Есть hardware export `*.xsa` (из Vivado).
- Для запуска по JTAG: доступен `hw_server`, драйверы кабеля установлены.
- `python3` (используется build‑скриптами).

Обычно на Linux нужно “подсорсить” окружение Xilinx в каждом новом терминале:
```sh
source <Vitis-install>/settings64.sh
xsct -version
```

### Build
Из корня репозитория:
```sh
XSA=/abs/path/to/your/design.xsa ./build.sh
```

Полезные переменные:
```sh
# Не удалять vitis_ws (ускоряет итерации)
XSA=/abs/path/to/your/design.xsa CLEAN=0 ./build.sh

# Выбор lwIP библиотеки (если в вашей установке отличаются имена)
XSA=/abs/path/to/your/design.xsa LWIP_LIB=lwip220 ./build.sh
```

Результаты сборки:
- ELF: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`
- Экспорт платформы: `vitis_ws/plat_bvstk/export/plat_bvstk/hw/`

### Run over JTAG
Запуск (программирование PL + init PS7 + загрузка ELF):
```sh
./run_jtag.sh /abs/path/to/your/design.bit
```

Примечания:
- В `run_jtag.tcl` есть `BITSTREAM_PATH_OVERRIDE`. Если он не пустой — скрипт будет использовать его.
- Можно передать bitstream через аргумент `run_jtag.sh` (он выставит `BITSTREAM_FILE`).

### Connect to TCP console
После старта прошивки:
```sh
telnet 192.168.0.10 8888
```

## TCP console
Все команды вводятся в TCP‑консоли (порт `8888`), ответы имеют простой формат (`OK`, `ERR`, либо данные).

### Built-ins
- `help` / `-h` / `--help` — список доступных утилит
- `quit` / `exit` — закрыть сессию

### fs — filesystem
Назначение: навигация и операции с файлами на `sd` и `flash`.

Команды:
- `pwd`
- `ls [path]`
- `cd <dir>`
- `cd flash | cd sd` (переключение устройства)
- `mkdir <dir>`
- `touch <file>`
- `cat <file>`
- `rm <file|dir>`
- `cp <src> <dst>`
- `cp -r <src> <dst>`
- `mv <src> <dst>`

Пути:
- относительные (от текущей директории),
- абсолютные `/...` (от корня выбранного FS),
- явные `0:/...`, `1:/...`,
- алиасы `sd:/...`, `flash:/...`.

Примеры:
```
pwd
ls
cd flash
mkdir configs
cat flash:/configs/network.json
cp sd:/configs/network.json flash:/configs/network.json
```

### tar — archive
Назначение: tar‑архивация каталогов и распаковка.

Команды:
- `tar c <src_dir> <dst_tar>` — создать tar из директории
- `tar x <src_tar> <dst_dir>` — распаковать tar в директорию
- `tar t <src_tar>` — список содержимого

Примеры:
```
tar c logs sd:/backup/logs.tar
tar t sd:/backup/logs.tar
tar x sd:/backup/logs.tar /restore
```

### ip — network
Назначение: Linux‑like управление сетевыми параметрами.

Поддержано (ограниченный набор, интерфейс фиксирован как `eth0`, только IPv4):
- `ip addr show`
- `ip addr set <IPv4>/<prefix>`
- `ip link show`
- `ip link set address <mac>`
- `ip route show`
- `ip route set default via <gw>`
- `ip save` — сохранить текущие runtime‑настройки из `netif` в `flash:/configs/network.json`

Примеры:
```
ip addr show
ip addr set 192.168.0.10/24
ip route set default via 192.168.0.1
ip link set address 00:0a:35:00:01:02
ip save
```

Важно: смена IP может оборвать текущую TCP‑сессию.

### smi — MDIO/SMI
Назначение: доступ к PHY по MDIO.

Команды:
- `smi r <phy> <reg>` — прочитать регистр (0..31)
- `smi w <phy> <reg> <data>` — записать регистр

Примеры:
```
smi r 0 1
smi w 0 0 0x8000
```

### mem — memory
Назначение: “peek/poke” памяти (MMIO).

Команды:
- `mem r <addr>`
  - читает 32‑бит, если адрес выровнен по 4; иначе читает 8‑бит
- `mem w <addr> <value>`
  - пишет 32‑бит, если адрес выровнен; иначе разрешает 8‑бит при `value <= 0xFF`

Примеры:
```
mem r 0xE000A000
mem w 0xE000A000 0x00000001
mem r 0xE000A001
mem w 0xE000A001 0x7F
```

### axp — AXP15060 (I2C)
Назначение: диагностика и управление PMIC AXP15060 + правила доступа (whitelist/blacklist).

Команды:
- `axp status`
- `axp r <reg>`
- `axp w <reg> <val>`
- `axp rules`
- `axp policy <whitelist|blacklist>`
- `axp allow <reg> <val>`
- `axp deny <reg> <val>`
- `axp clear <reg> <val>`
- `axp reset`

Примеры:
```
axp status
axp r 0x10
axp w 0x10 0x01
axp policy whitelist
axp allow 0x10 0x01
axp rules
axp reset
```

## HTTP file transfer
Base URL: `http://<device-ip>:8000`

Single file:
- `GET  /sd/<path>` → `0:/<path>`
- `PUT  /sd/<path>` → `0:/<path>`
- `GET  /flash/<path>` → `1:/<path>`
- `PUT  /flash/<path>` → `1:/<path>`

Примеры:
```sh
curl -T local.bin http://192.168.0.10:8000/flash/fw/local.bin
curl -o local.bin http://192.168.0.10:8000/sd/logs/local.bin
```

Directories (tar stream, без сжатия):
- `GET  /tar/sd/<dir>` / `PUT /tar/sd/<dir>`
- `GET  /tar/flash/<dir>` / `PUT /tar/flash/<dir>`

Примеры:
```sh
curl http://192.168.0.10:8000/tar/flash/cfg | tar -xf -
tar -cf - ./cfg | curl -T - http://192.168.0.10:8000/tar/flash/cfg
```

## Quick verification checklist
1) Сборка: `XSA=... ./build.sh`
2) Запуск по JTAG: `./run_jtag.sh ...bit`
3) Консоль: `telnet <ip> 8888`
4) Проверить FS:
```
pwd
ls
cd flash
ls /configs
cat /configs/network.json
```
5) Проверить сеть/настройки:
```
ip addr show
ip route show
```
6) Проверить HTTP PUT/GET:
```sh
echo hello > hello.txt
curl -T hello.txt http://<ip>:8000/flash/tmp/hello.txt
curl -o hello2.txt http://<ip>:8000/flash/tmp/hello.txt
```

## Troubleshooting
- `xsct: not found` — не подсорсили окружение Vitis (`settings64.sh`) или не установлен Vitis/XSCT.
- `Bitstream not found` — проверьте `BITSTREAM_PATH_OVERRIDE` в `run_jtag.tcl` или передайте `.bit` через `run_jtag.sh`.
- QSPI “не монтируется” — проверьте, что `QSPI_FS_BASE_BYTES` не пересекается с boot‑образами, и что QSPI доступен (см. `qspi_flash_self_test()` лог).
- После `ip addr set ...` сессия отвалилась — это ожидаемо при смене IP; переподключитесь к новому адресу.
