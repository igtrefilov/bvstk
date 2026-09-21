# Параметры сборки и инструментов

Пути по умолчанию указаны относительно корня BVSTK, если не оговорено
иначе. Вызов корневого диспетчера не передаёт произвольные дополнительные
аргументы вложенному сценарию: для его параметров командной строки
используйте прямой путь.

## Диспетчеры

`build.sh` принимает одну цель: `check`, `freertos`, `neutrino`,
`neutrino-image`, `all`. `all` последовательно собирает FreeRTOS и IFS,
не вызывает Vivado и не загружает устройство. `run.sh` принимает
`freertos jtag` или `neutrino jtag`; во втором случае после JTAG выполняет
SSH-проверку. Неверная цель завершается кодом 2.

## Аппаратная сборка

Сценарий: `scripts/fpga/build_fpga.sh`.
Приоритет: встроенные значения → `.conf` → параметры командной строки.
Сам `.conf` выполняется как код Bash и должен быть доверенным.

| Переменная файла | Параметр команды | Умолчание и смысл |
|---|---|---|
| `BUILD_FPGA_CONFIG` в окружении | `--config FILE` | Выбор файла, штатно `scripts/fpga/build_fpga.conf` |
| `FPGA_DIR` | `--fpga-dir DIR` | Соседний `../hw_platform/fpga`, содержит `Burevestnik_21.tcl` |
| `PROJ_NAME` | `--proj NAME` | `Burevestnik_21` |
| `VIVADO_BIN` | `--vivado PATH` | `vivado` |
| `JOBS` | `--jobs N` | `8`, параллельные задания реализации |
| `OUTPUT_DIR` | `--output-dir DIR` | `artifacts/fpga` |
| `VIVADO_LOG_DIR` | `--log-dir DIR` | `artifacts/vivado/logs` |
| `CLEAN` | `--clean` | `0`; флаг включает удаление `<FPGA_DIR>/vivado_project` |
| `XILINX_SETTINGS` | — | Необязательный путь к `settings64.sh` |

`--help` выводит справку. Отсутствующий файл конфигурации, инструмент или
входной проект — ошибка; существующий файл результата не заменяет
проверку кода завершения текущей сборки.

## FreeRTOS / Vitis

Сценарий: `scripts/vitis/build.sh`, состав: `build.tcl`.

| Переменная | Умолчание и смысл |
|---|---|
| `BUILD_VITIS_CONFIG` | `scripts/vitis/build_vitis.conf`, локальные параметры |
| `XILINX_SETTINGS` | Необязательное окружение Vitis |
| `XSA` | `artifacts/fpga/design.xsa` |
| `CLEAN_DEFAULT` | `1`, исходный режим очистки |
| `CLEAN` | Если не задан, берётся `CLEAN_DEFAULT`; ненулевое значение удаляет `vitis_ws` |
| `LWIP_LIB` | Если не задан, пробуются `lwip220`, затем `lwip211` |
| `BVSTK_PL_SPI_DIAGNOSTIC` | `0`: обычное приложение BVSTK с PL-SD службой; `1`: сохранённая SPI-диагностика |

Файл `.conf` загружается до назначения недостающих значений. Например,
обычное `XSA=...` внутри него заменит экспортированный `XSA`.
Результат — `vitis_ws/app_bvstk/Debug/app_bvstk.elf`; очистка рабочего
каталога выполняется до проверки доступности `xsct`.
Для обычного FreeRTOS-профиля BSP выделяет 512 КиБ heap (профиль с SSH —
1 МиБ), чтобы вместе запускались сетевые, файловые, UART и PL-SD задачи.

Отдельной сборки SD нет: драйвер, порт, служба и команды входят в обычный
`vitis_ws/app_bvstk/Debug/app_bvstk.elf`. В штатном режиме `main` запускает
сетевые службы, файловые службы и UART-консоль; SD PL автоматически
инициализируется и монтируется в `sd-pl:/` (существующая FAT12/16/32,
без форматирования). Подробнее — [PL SD](../operations/sd-pl-prototype.md).

### SSH FreeRTOS

| Переменная | Умолчание и смысл |
|---|---|
| `BVSTK_SSH_ENABLE` | `0`; значение `1` включает зависимости и сервер |
| `BVSTK_SSH_PASSWORD` | Обязателен при включении; секрет из окружения |
| `BVSTK_SSH_USER` | `root` |
| `BVSTK_SSH_PORT` | `22` |
| `BVSTK_SSH_HOST_KEY` | PEM-ключ устройства; без него генерируется временный |
| `BVSTK_WOLFSSL_ROOT` | Готовые зависимости, штатно `build/ssh-deps/wolfssl` |
| `BVSTK_WOLFSSH_ROOT` | Готовые зависимости, штатно `build/ssh-deps/wolfssh` |

Два пути библиотек переопределяют вместе. Исходные архивы —
`third_party/dist/`, производные файлы — `build/ssh-deps/` и
`src/apps/freertos/services/ssh/bvstk_ssh_generated.h`. Секретные материалы
не включаются в отчёт проверки. [Инструкция SSH-сборки](../build/freertos.md)
описывает передачу пароля без записи в историю команды.

## Нейтрино

`scripts/neutrino/build.sh` собирает программы;
`scripts/neutrino/build_image.sh` повторно вызывает эту сборку и создаёт IFS.

| Переменная | Умолчание и смысл |
|---|---|
| `NEUTRINO_BUILD_DIR` | `build/neutrino` |
| `QCC_VARIANT` | `8.3.0,gcc_ntoarmv7le` |
| `NEUTRINO_BSP_DIR` | `third_party/neutrino/bsp/ax7020` |
| `NEUTRINO_BASE_BUILD` | `images/zynq7000-ax7020-ssh.build` внутри выбранного BSP |
| `NEUTRINO_IFS_FILE` | `ifs-zynq7000-ax7020-bvstk.raw` внутри каталога сборки |
| `NEUTRINO_KEY_DIR` | `ssh` внутри каталога сборки |
| `SSH_IDENTITY` | `ax7020_ssh_client` внутри каталога сборки |
| `NEUTRINO_ROOT_SHADOW_FILE` | `root.shadow` внутри каталога сборки |

Программы: `bvstkctl`, `bvstkd`, `i2c`, `bvstk-shell`, `bvstk-qspi-fat`,
`bvstk-sd-raw`. Дополнительно создаются встроенный `default_configs.h`
и производное описание `zynq7000-ax7020-bvstk.build`. `build_image.sh`
создаёт отсутствующие ключи и парольную строку с заблокированным паролем;
существующие защищённые данные не нужно удалять перед повторной сборкой.

`generate_root_shadow.py --output FILE` выбирает место локального
парольного файла; пароль вводят интерактивно. Этот путь нужно согласовать
с `NEUTRINO_ROOT_SHADOW_FILE`.

## JTAG FreeRTOS

Сценарий: `scripts/vitis/run_jtag.sh [--debug|--prepare-debug] [BITSTREAM]`.
`RUN_JTAG_CONFIG` выбирает `.conf`, штатно `scripts/vitis/run_jtag.conf`.
Файл может задать `XILINX_SETTINGS`, `ELF_FILE`, `BITSTREAM_FILE` и
`PS7_INIT_TCL`. Позиционный bitstream заменяет загруженное значение.

Без переопределения ELF — `vitis_ws/app_bvstk/Debug/app_bvstk.elf`.
Bitstream ищется последовательно:

1. `artifacts/fpga/design.bit`;
2. старый соседний `../bvstk_hw/tmp/design.bit`;
3. `vitis_ws/plat_bvstk/export/plat_bvstk/hw/Burevestnik_top.bit`.

Порядок поиска `ps7_init.tcl` обычного JTAG-сценария:

1. `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl`;
2. `vitis_ws/plat_bvstk/hw/ps7_init.tcl`;
3. `vitis_ws/app_bvstk/_ide/psinit/ps7_init.tcl`.

Отсутствие входа — ошибка. `--debug` использует подготовку из
`scripts/vscode/`, останавливает ядро и оставляет подключение GDB.
Порты: TCF 3121, GDB 3000. Не считайте найденный запасной файл
автоматически совместимым с новым ELF.

## JTAG и SSH-проверка Нейтрино

| Переменная | Умолчание и смысл |
|---|---|
| `IFS_FILE` | `ifs-zynq7000-ax7020-bvstk.raw` в `NEUTRINO_BUILD_DIR` |
| `BITSTREAM_FILE` | `artifacts/fpga/design.bit` |
| `PS7_INIT_TCL` | `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl` |
| `UART_DEVICE` | `/dev/ttyUSB1`, требуется проверка физического адаптера |
| `UART_BAUD` | `115200` |
| `LOG_FILE` | `neutrino-uart.log` в каталоге сборки |
| `DEVICE_IP` | `192.168.0.10` для последующей SSH-проверки |
| `SSH_USER` | `root` |
| `SSH_IDENTITY` | `ax7020_ssh_client` в каталоге сборки |

`NEUTRINO_IFS_FILE` относится к сборке, `IFS_FILE` — к загрузке: это разные
переменные. Прямой `scripts/neutrino/run_jtag.sh` не вызывает SSH-проверку;
её добавляет корневой `run.sh`. Проверка отключает сохранённый контроль
ключа сервера и предназначена для изолированного стенда.

## Отладочные инструменты

| Переменная | Назначение |
|---|---|
| `VITIS_ARM_GCC_BIN` | Каталог ARM toolchain для базы компиляции и инструментов |
| `VITIS_ARM_GDB` | Путь исполняемого GDB для обёртки |
| `GDB_CWD` | Рабочий каталог обёртки GDB |
| `BEAR_LIBRARY_PATH` | Библиотека перехвата Bear |

`gen_compile_commands.sh` пересоздаёт корневой `compile_commands.json`
и запускает принудительную пересборку Make. Подробности — в [отладке](../build/debug.md).

## Загрузчик веб-ресурсов

Команда: `python3 web/upload_flash_www.py HOST [параметры]`.

| Параметр | Умолчание и действие |
|---|---|
| `--src DIR` | `web/assets`, источник файлов |
| `--dst PATH` | `flash:/www`, путь для консольного `mkdir` |
| `--http-prefix PATH` | `/flash/www`, назначение HTTP PUT |
| `--manifest NAME` | `manifest.json` |
| `--http-port N` | `80` |
| `--console-port N` | `8888` |
| `--force` | Отправить все файлы, игнорируя сравнение с манифестом |
| `--no-mkdir` | Не создавать каталоги; они должны существовать |
| `--dry-run` | Показать действия без загрузки; допускает чтение удалённого состояния |

Пути `--dst` и `--http-prefix` должны обозначать один целевой каталог
через два интерфейса. Обновление перезаписывает файлы и не является
атомарной заменой всего дерева. Параметры DCP2-клиента находятся в
[практической главе](../operations/dcp2.md) и его `--help`.
