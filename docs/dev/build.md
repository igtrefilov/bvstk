# Сборка

Подробная инструкция по сборке прошивки, переменным окружения и структуре Vitis workspace.


### Быстрый старт

Минимальный сценарий сборки (с нуля):

1) Активировать окружение Xilinx:
```sh
source <Vitis-install>/settings64.sh
```

2) Собрать прошивку, указав `*.xsa`:
```sh
XSA=/abs/path/to/design.xsa ./build.sh
```

Примечание:
- Дефолтный XSA выбирается автоматически в `scripts/vitis/build.sh` (`../bvstk_hw/tmp/design.xsa`, fallback `../bvstk_hw/Burevestnik_top.xsa`).
- Для переносимой настройки лучше использовать `scripts/vitis/build_vitis.conf` (или переменную `XSA`), а не редактировать скрипт.

Результат:
- ELF приложения: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`
- Экспорт платформы (в т.ч. `ps7_init.tcl`): `vitis_ws/plat_bvstk/export/plat_bvstk/hw/`

Если нужно пересобирать без удаления `vitis_ws/`:
```sh
XSA=/abs/path/to/design.xsa CLEAN=0 ./build.sh
```

### Переменные сборки

Скрипты сборки/запуска читают настройки из **переменных окружения** (environment variables).

Как задавать переменные (в bash):

1) **Только для одной команды**:
```sh
XSA=/abs/path/to/design.xsa CLEAN=0 ./build.sh
```

2) **Экспортировать в текущую сессию**:
```sh
export XSA=/abs/path/to/design.xsa
export CLEAN=0
./build.sh
```

3) Эквивалент через `env`:
```sh
env XSA=/abs/path/to/design.xsa CLEAN=0 ./build.sh
```

**`XSA`**
- Путь к Hardware Export (`*.xsa`), который используется для создания платформы Vitis.
- Если не задан, берётся авто-детект из `scripts/vitis/build.sh`/`scripts/vitis/build.tcl`.
- Пример:
```sh
XSA=/abs/path/to/design.xsa ./build.sh
```

**`CLEAN`**
- Управляет пересозданием `vitis_ws/`.
- `CLEAN=1` (по умолчанию) — удалить `vitis_ws/` перед сборкой.
- `CLEAN=0` — оставить существующий `vitis_ws/` и пересобрать внутри него.
- Пример:
```sh
XSA=/abs/path/to/design.xsa CLEAN=0 ./build.sh
```

**`LWIP_LIB`**
- Выбор имени lwIP‑библиотеки в BSP (зависит от установленной версии Vitis/BSP).
- Если не задан, скрипт пробует по очереди `lwip220`, затем `lwip211`.
- Пример:
```sh
XSA=/abs/path/to/design.xsa LWIP_LIB=lwip220 ./build.sh
```

**`BITSTREAM_FILE`** (JTAG‑запуск)
- Путь к `.bit`, который будет использован `run_jtag.tcl`.
- Учитывается, если не передан путь через аргумент `run_jtag.sh`.
- Пример:
```sh
BITSTREAM_FILE=/abs/path/to/design.bit ./run_jtag.sh
```

### Артефакты и структура vitis_ws

`vitis_ws/` — генерируемая рабочая область Vitis/XSCT. По умолчанию `build.sh` удаляет её и создаёт заново (см. `CLEAN`).

Типовая структура:

- `vitis_ws/app_bvstk/` — проект приложения.
  - `vitis_ws/app_bvstk/src` — **symlink** на `./src` репозитория (исходники не копируются).
  - `vitis_ws/app_bvstk/Debug/app_bvstk.elf` — собранный ELF приложения.
  - `vitis_ws/app_bvstk/_ide/` — служебные артефакты IDE (в т.ч. копии `.bit`/`ps7_init.tcl`).

- `vitis_ws/plat_bvstk/` — платформа (hardware + domains + BSP).
  - `vitis_ws/plat_bvstk/hw/` — локальная копия/снимок hardware (`*.xsa`, `*.bit`, `ps7_init.tcl`).
  - `vitis_ws/plat_bvstk/export/plat_bvstk/hw/` — export платформы, используемый `run_jtag.tcl`:
    - `ps7_init.tcl` — инициализация PS7 для JTAG‑старта
    - `*.bit` — bitstream (если присутствует)
    - `*.xsa` — hardware export
  - `vitis_ws/plat_bvstk/ps7_cortexa9_0/freertos10_xilinx_domain/bsp/` — BSP FreeRTOS‑домена (Makefile, `system.mss`, `libsrc/...`).

- `vitis_ws/plat_bvstk/zynq_fsbl/` — проект FSBL (может собираться/использоваться отдельно).
  - `vitis_ws/plat_bvstk/zynq_fsbl/fsbl.elf` — ELF FSBL.

Замечания:
- `src/config/default_configs.h` генерируется при сборке и входит в `app_bvstk` через symlink на `src/`.
- Скрипты сборки патчат файлы FatFs в BSP (LFN + FreeRTOS) внутри `.../bsp/.../libsrc/` — это нормально, но значит, что состояние `vitis_ws/` зависит от прогонов сборки (поэтому `CLEAN=1` полезен для “чистой” пересборки).
