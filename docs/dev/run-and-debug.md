# Запуск и отладка

Этот документ описывает текущие JTAG-flow проекта `bvstk`. Штатная точка входа — корневой `./run.sh`, который выбирает FreeRTOS или Neutrino. Низкоуровневые скрипты находятся в `scripts/vitis/`, `scripts/neutrino/` и `scripts/vscode/`.

В FreeRTOS-цикле разработчик программирует PL, выполняет `ps7_init`, загружает `app_bvstk.elf` в `Cortex-A9 #0` и запускает приложение. Neutrino-flow вместо ELF загружает IFS в DDR и выставляет PC на его стартовый адрес. Пошаговый GDB/VSCode flow в этом документе относится к FreeRTOS.

## Нормальный путь запуска

Для обычного запуска достаточно активировать Xilinx-окружение, перейти в корень репозитория и вызвать:

```sh
source <Vitis-install>/settings64.sh
cd <repo-root>
./build.sh freertos
./run.sh freertos jtag
```

Смысл этой последовательности в том, что FreeRTOS build должен заранее подготовить и `vitis_ws/app_bvstk/Debug/app_bvstk.elf`, и platform export с `ps7_init.tcl`. Без этого JTAG-скрипт не сможет ни загрузить приложение, ни корректно инициализировать PS7.

Для Neutrino последовательность другая:

```sh
cd <repo-root>
./build.sh neutrino-image
./run.sh neutrino jtag
```

Корневой `run.sh` сначала вызывает `scripts/neutrino/run_jtag.sh`, который пишет UART в `build/neutrino/neutrino-uart.log` и проверяет сигнатуру старта. Затем запускается `scripts/neutrino/verify_ssh.sh`, выполняющий `bvstkctl version` и `bvstkctl pl list` на устройстве.

## На какие артефакты опирается запуск

Текущий запуск использует фиксированный набор входных файлов. В нормальной структуре проекта они уже лежат там, где их ожидают скрипты.

| Артефакт | Обычный путь |
|---|---|
| ELF приложения | `vitis_ws/app_bvstk/Debug/app_bvstk.elf` |
| bitstream | `artifacts/fpga/design.bit` |
| hardware export | `artifacts/fpga/design.xsa` |
| `ps7_init.tcl` | `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl` |
| `xsct` и `hw_server` | из активированного Xilinx/Vitis environment |

В проекте эти файлы действительно существуют в ожидаемых местах, поэтому документация должна опираться именно на них, а не на устаревшие каталоги вроде отдельного `bvstk_hw`.

## Как `run_jtag.sh` выбирает файлы

Скрипт `scripts/vitis/run_jtag.sh` умеет читать `scripts/vitis/run_jtag.conf`, принимать прямой путь к bitstream первым позиционным аргументом и уважать переменные окружения `BITSTREAM_FILE`, `ELF_FILE` и `PS7_INIT_TCL`. Но для повседневной работы полезнее держать в голове не механизм настройки как таковой, а реальный порядок приоритетов.

| Что выбирается | Приоритет |
|---|---|
| bitstream | позиционный аргумент `run_jtag.sh` -> `BITSTREAM_FILE` -> `artifacts/fpga/design.bit` -> `../bvstk_hw/tmp/design.bit` -> `vitis_ws/plat_bvstk/export/plat_bvstk/hw/Burevestnik_top.bit` |
| ELF | `ELF_FILE` -> `vitis_ws/app_bvstk/Debug/app_bvstk.elf` |
| `ps7_init.tcl` | `PS7_INIT_TCL` -> `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl` |

Из этого следует простой практический вывод: если вы не задаёте override вручную, проект ожидает, что актуальный bitstream лежит в `artifacts/fpga/design.bit`. Если туда случайно попал артефакт от другого hardware export, JTAG-запуск формально состоится, но поведение рантайма дальше будет трудно интерпретировать.

## Что делает обычный JTAG-запуск

`scripts/vitis/run_jtag.tcl` реализует ровно тот сценарий, который нужен для разработки: он подключается к `hw_server`, выбирает APU-таргет, делает `rst -system`, останавливает CPU, программирует PL, исполняет `ps7_init` и `ps7_post_config`, затем переключается на `Cortex-A9 #0`, загружает ELF командой `dow` и передаёт выполнение через `con`.

Это важно понимать архитектурно. Скрипт не повторяет автономный boot через `BOOT.BIN`, не запускает FSBL и не имитирует полную boot-цепочку SD/QSPI. Это именно development flow: подготовить железо, загрузить ELF в память и немедленно стартовать приложение.

Если нужно временно запустить проект на нестандартном bitstream, его можно передать прямо в командной строке:

```sh
./scripts/vitis/run_jtag.sh /abs/path/to/design.bit
```

Тот же override можно сделать через environment:

```sh
BITSTREAM_FILE=/abs/path/to/design.bit ./scripts/vitis/run_jtag.sh
```

Аналогично можно переопределить `ELF_FILE` и `PS7_INIT_TCL`, но в обычной разработке это требуется редко.

## Загрузка Neutrino по JTAG

Neutrino-скрипт использует по умолчанию:

| Артефакт или параметр | Значение |
|---|---|
| IFS | `build/neutrino/ifs-zynq7000-ax7020-bvstk.raw` |
| bitstream | `artifacts/fpga/design.bit` |
| `ps7_init.tcl` | `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl` |
| UART | `/dev/ttyUSB1`, `115200 8N1` |
| UART-лог | `build/neutrino/neutrino-uart.log` |
| SSH-проверка | `root@192.168.0.10` с ключом из `SSH_IDENTITY` |

Пути переопределяются через `IFS_FILE`, `BITSTREAM_FILE`, `PS7_INIT_TCL`, `UART_DEVICE`, `UART_BAUD`, `LOG_FILE`, `DEVICE_IP`, `SSH_USER` и `SSH_IDENTITY`. IFS загружается в DDR по адресу `0x00100000`; это временный JTAG-запуск, а не запись автономного образа на SD или QSPI.

## Debug-режим

Для debug-attach скрипт переключается в другой режим:

```sh
./scripts/vitis/run_jtag.sh --debug
```

В этом случае используется не `run_jtag.tcl`, а `scripts/vscode/jtag_prepare_debug.tcl`. Этот prepare-скрипт также подключается к `hw_server`, делает reset, программирует PL и выполняет `ps7_init`, но после этого оставляет `Cortex-A9 #0` остановленным. Загрузка ELF и дальнейшее управление переходят к GDB.

`run_jtag.sh --debug` отдельно проверяет, что `hw_server` доступен не только на TCF-порту `3121`, но и с включённым GDB-портом `3000`. Если `3121` уже занят старым экземпляром `hw_server`, запущенным без `-p 3000`, скрипт завершится с ошибкой и попросит перезапустить сервер в правильном режиме.

## Отладка из VSCode

Текущая VSCode-конфигурация уже реализует полный debug-cycle и не требует предварительно вручную запускать `./scripts/vitis/run_jtag.sh --debug`. В `.vscode/tasks.json` описана фоновая задача, которая поднимает `hw_server -s tcp::3121 -p 3000 -L-`, а затем вызывает `xsct ${workspaceFolder}/scripts/vscode/jtag_prepare_debug.tcl`. После этого `.vscode/launch.json` подключает `arm-none-eabi-gdb` к `localhost:3000`, выполняет `load`, ставит временный breakpoint на `main` и продолжает выполнение.

| Компонент | Роль |
|---|---|
| `.vscode/tasks.json` | запуск `hw_server` и hardware prepare через XSCT |
| `.vscode/launch.json` | attach GDB к `localhost:3000` и загрузка ELF |
| `scripts/vscode/jtag_prepare_debug.tcl` | reset, PL programming, `ps7_init`, остановка core0 |
| `scripts/vscode/arm-none-eabi-gdb.sh` | wrapper для запуска корректного `arm-none-eabi-gdb` |

У конфигурации в `launch.json` осталось историческое имя `Attach: Zynq-7000 (core0, after ./run_jtag.sh --debug)`, но фактически preLaunch task уже делает тот же prepare-step сама. На практике это обычный one-click attach из IDE.

## Ручной step-debug

Без VSCode отладка выглядит почти так же, только команды выполняются явно:

```sh
./scripts/vitis/run_jtag.sh --debug
arm-none-eabi-gdb vitis_ws/app_bvstk/Debug/app_bvstk.elf \
  -ex "target remote :3000" \
  -ex "load" \
  -ex "tbreak main" \
  -ex "continue"
```

Здесь важно не путать роли частей цепочки. `--debug` подготавливает железо и оставляет CPU остановленным, а `gdb` уже загружает ELF, расставляет breakpoints и управляет исполнением.

## Что должно стартовать после загрузки

После `con` в обычном FreeRTOS-режиме или после `load` и `continue` в GDB приложение переходит к своей штатной последовательности из `src/main.c`. В текущем состоянии проекта запускаются QSPI/SD/FS-инициализация, `config_store`, сеть, TCP shell, опциональный SSH, HTTP, DCP2, I2C и SPI. Подсистема SMI в кодовой базе есть, но `start_smi()` в `main.c` по-прежнему закомментирован и в стандартный startup path не входит.

Снаружи это обычно проявляется через следующие сервисы:

| Сервис | Порт | Назначение |
|---|---:|---|
| TCP shell | `8888` | интерактивная консоль |
| SSH shell | `22` | тот же диспетчер команд; только при `BVSTK_SSH_ENABLE=1` |
| HTTP | `80` | web/API surface |
| DCP2 | `8889` | бинарный TCP-интерфейс |

Если после успешного JTAG-запуска не наблюдается `SMI`-поведения, это не обязательно ошибка attach или загрузки. В текущем проектном состоянии подсистема просто не стартует автоматически.

## Первичная проверка после запуска

После загрузки полезно проверить не только сам факт успешного `dow`, но и то, что рантайм реально дошёл до сетевых сервисов. Самая быстрая проверка обычно делается через TCP shell:

```sh
telnet <device-ip> 8888
```

Если `telnet` не установлен, подойдёт:

```sh
nc <device-ip> 8888
```

Если FreeRTOS был собран с SSH, доступен привычный интерактивный вход:

```sh
ssh root@<device-ip>
```

Дальше обычно достаточно нескольких базовых команд:

```text
help
ip addr show
fs pwd
fs ls
```

Этого хватает, чтобы быстро проверить shell, сетевой стек и доступность файловых устройств. Для отдельной проверки DCP2 в репозитории уже есть готовый инструмент:

```sh
./scripts/dcp2/monitor_notify.py <device-ip> --port 8889
```

Если нужен только мониторинг I2C-событий, фильтр можно сузить:

```sh
./scripts/dcp2/monitor_notify.py <device-ip> --port 8889 --buses i2c --sources telnet,host
```

## Типичные проблемы

Большинство сбоев в этом flow удобно держать в одной таблице.

| Симптом | Что проверять первым |
|---|---|
| `xsct not found in PATH` | активировано ли Xilinx-окружение, либо задан ли `XILINX_SETTINGS` |
| `ELF file not found` | собран ли `vitis_ws/app_bvstk/Debug/app_bvstk.elf` |
| `Bitstream not found` | существует ли `artifacts/fpga/design.bit`, либо корректно ли задан `BITSTREAM_FILE` |
| `PS7 init script not found` | собран ли platform export и появился ли `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl` |
| `hw_server is running on port 3121, but GDB port 3000 is not open` | не остался ли старый `hw_server`, запущенный без `-p 3000` |
| breakpoints не срабатывают | был ли выполнен debug-prepare и тот ли ELF загружен в target |
| сервисы после запуска не появляются | совпадают ли firmware и bitstream, поднялась ли сеть, не ушла ли система в fallback-конфиг |

Последний пункт особенно важен для проекта, сильно завязанного на `xsa` и кастомные PL-ядра. Успешный `dow` сам по себе ещё не доказывает, что запущена правильная hardware/software комбинация.

## Смежные документы

`run-and-debug.md` описывает сам attach и запуск, но не заменяет остальные dev-документы. За контекстом сборки лучше идти в `build.md`, за настройками окружения и артефактов в `development-environment.md`, за ожидаемым поведением рантайма в `architecture.md`, а за составом и происхождением hardware platform в `hardware-platform.md`.
