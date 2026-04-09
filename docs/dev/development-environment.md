# Окружение разработки

Требования к окружению разработки, входным HW-артефактам и структуре репозитория.


### Требования

Для сборки и запуска через JTAG требуется окружение Xilinx и несколько утилит на хост‑ПК.

**Обязательное**
- **Xilinx Vitis / XSCT**: `xsct` должен быть доступен в `PATH` (обычно после `source <Vitis-install>/settings64.sh`).
- **Python 3**: используется вспомогательными скриптами сборки (генерация дефолтных конфигов и патчи FatFs).
- **Hardware Export (`*.xsa`)**: соответствует вашему HW‑design (адреса/IRQ/периферия должны совпадать с прошивкой).

**Для запуска по JTAG**
- **hw_server** и драйверы JTAG‑кабеля (Xilinx Cable Drivers).
- **Bitstream (`*.bit`)** для программирования PL.

**(Опционально) VSCode: навигация по BSP/платформе и step‑debug**
- **VSCode** + расширение **C/C++** (`ms-vscode.cpptools`).
- **bear** — для генерации `compile_commands.json` (IntelliSense/F12 по BSP):
  - Ubuntu/Debian: `sudo apt install -y bear`
  - затем: `./scripts/vscode/gen_compile_commands.sh`
- Инструкции:
  - единая инструкция: `scripts/vscode/README.md`

**Проверка окружения**
```sh
source <Vitis-install>/settings64.sh
xsct -version
python3 --version
```

### Входные HW‑артефакты

Прошивка собирается и запускается поверх конкретного HW‑design. Поэтому нужны артефакты аппаратной части:

**1) Hardware Export (`*.xsa`)**
- Используется при сборке платформы Vitis (создание `plat_bvstk` и BSP).
- Должен соответствовать вашему bitstream’у и адресной карте (PS‑периферия, IRQ, MMIO/BRAM для PL‑ядер).
- Как передать:
  - через переменную окружения для сборки: `XSA=/abs/path/design.xsa ./build.sh`
  - если `XSA` не задан, по умолчанию берётся авто-детект в `scripts/vitis/build.sh`/`scripts/vitis/build.tcl`.

**2) Bitstream (`*.bit`)**
- Нужен для программирования PL при запуске по JTAG.
- Как передать:
  - аргументом: `./run_jtag.sh /abs/path/design.bit`
  - либо через `BITSTREAM_FILE=/abs/path/design.bit` (учитывается в `run_jtag.sh`)
  - иначе используется дефолт из `run_jtag.tcl`: сначала `../bvstk_hw/tmp/design.bit`, затем `vitis_ws/plat_bvstk/export/.../hw/*.bit`.

**3) PS7 init (`ps7_init.tcl`)**
- Требуется для JTAG‑старта: инициализация PS перед загрузкой ELF.
- Скрипт берётся из export’а платформы: `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl`.
- Появляется после успешной сборки `./build.sh`.

**4) (Опционально) Device/board‑specific файлы**
- В зависимости от HW‑design могут потребоваться дополнительные файлы/настройки вне репозитория (например, проекты Vivado, constraints, генерация XSA/bit).

### Структура репозитория

Ключевые каталоги и файлы:

- `build.sh` — совместимая обёртка (корневой путь), реальная логика: `scripts/vitis/build.sh`.
- `build.tcl` — совместимая обёртка (корневой путь), реальная логика: `scripts/vitis/build.tcl`.
- `run_jtag.sh` — совместимая обёртка (корневой путь), реальная логика: `scripts/vitis/run_jtag.sh`.
  - обычный режим: программирует PL + `ps7_init` + `dow` ELF + `con`
  - режим `--debug`: программирует PL + `ps7_init` + **halt core0** (для attach из VSCode/GDB)
- `run_jtag.tcl` — совместимая обёртка (корневой путь), реальная логика: `scripts/vitis/run_jtag.tcl`.
- `scripts/` — вспомогательные скрипты:
  - `scripts/vitis/*` — сборка Vitis и JTAG запуск (`build*`, `run_jtag*`, конфиги)
  - `scripts/vscode/gen_compile_commands.sh` — генерация `compile_commands.json` через `bear` (для VSCode навигации по BSP)
  - `scripts/vscode/jtag_prepare_debug.tcl` — XSCT подготовка JTAG для отладки (PL + `ps7_init` + halt core0)
  - `scripts/vscode/arm-none-eabi-gdb.sh` — враппер для поиска `arm-none-eabi-gdb` (удобно для VSCode)
- `.vscode/` — настройки VSCode (IntelliSense + debug)
- `docs/` — дополнительная документация по проекту.
- `configs/` — шаблоны JSON, которые встраиваются в прошивку как дефолты (сеть, I2C, SMI).
- `src/` — исходники прошивки (линкуются в Vitis‑проект как `vitis_ws/app_bvstk/src -> ./src`).
  - `src/main.c` — порядок инициализации и запуск планировщика.
  - `src/config/` — `config_store` (загрузка/миграция/сохранение JSON) и `default_configs.h` (генерируется при сборке).
  - `src/fs/` — общий слой FatFs (`fs_shared`, `fs_devices`), `diskio.c` (SD + QSPI как тома), FreeRTOS glue.
  - `src/sd_card/` — SDIO + задача монтирования SD.
  - `src/qspi_flash/` — низкоуровневый доступ к QSPI NOR и self‑test.
  - `src/qspi_fs/` — задача монтирования QSPI‑тома и разметка окна в флеше.
  - `src/bvstk_lan/` — инициализация lwIP/netif (MAC/IP из config_store).
  - `src/bvstk_tcp_server/` — TCP‑консоль и утилиты (`fs/ip/i2c/smi/mem/tar`).
  - `src/http/` и `src/http_fs/` — HTTP‑сервер, API `/api/*`, файловый доступ `/sd|/flash|/tar`, раздача `flash:/www/`.
  - `src/bvstk_i2c/`, `src/bvstk_smi/` — подсистемы кастомных PL‑ядер I2C и SMI/MDIO.
  - `src/mqtt_proc/`, `src/sntp_proc/` — дополнительные сетевые обработчики (если включены/используются).
  - `src/tar/` — tar‑упаковка/распаковка (используется для `/tar/*`).
- `vitis_ws/` — генерируемая рабочая область Vitis (платформа/BSP/ELF). Может быть удалена и пересоздана сборкой.
- `web/` — утилиты загрузки web‑ресурсов в `flash:/www/` (скрипты + `web/assets/`).
- `dot/` — вспомогательные материалы/черновики (не участвует в сборке прошивки).
