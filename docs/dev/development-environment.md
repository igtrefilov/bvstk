# Окружение разработки

Этот документ описывает практическое окружение, в котором сейчас собирается и отлаживается `bvstk`. Его задача не перечислить все возможные варианты настройки Xilinx-инструментов, а зафиксировать, как проект реально устроен сегодня: откуда берутся аппаратные артефакты, какие скрипты считаются основными и какие каталоги в репозитории действительно участвуют в разработке.

`bvstk` нельзя рассматривать как изолированный Vitis-проект. Прошивка собирается поверх конкретного `xsa`, использует экспортированную платформу Vitis и предполагает, что адреса, IRQ и состав периферии совпадают с аппаратным дизайном. Поэтому разработческое окружение здесь начинается не с IDE, а с согласованности между firmware-репозиторием и hardware export.

## Что нужно в рабочем окружении

Набор инструментов зависит от собираемого слоя. Для аппаратной платформы нужен Vivado. Для FreeRTOS нужны `xsct`, `python3` и актуальный `*.xsa`; для JTAG-запуска дополнительно требуются `hw_server`, драйверы кабеля и рабочий `*.bit`. Для Neutrino нужны `qcc`, `mkifs` и установленный комплект разработчика 2024; AX7020 BSP runtime входит в репозиторий в `third_party/neutrino/bsp/ax7020/`. Если используется VSCode как основной редактор FreeRTOS-кода, полезны расширение `ms-vscode.cpptools`, `arm-none-eabi-gdb` и `bear`.

Проверка базового окружения выглядит так:

```sh
source <Vitis-install>/settings64.sh
xsct -version
python3 --version
vivado -version
```

Проверка комплекта Neutrino:

```sh
source /etc/profile.d/kpda_env_2024.sh
qcc -V
command -v mkifs
```

Для VSCode и JTAG-отладки обычно проверяют ещё и эти инструменты:

```sh
hw_server -h
arm-none-eabi-gdb --version
bear --version
```

Если ARM toolchain не лежит в `PATH`, проектная обвязка допускает отдельную настройку через `VITIS_ARM_GCC_BIN`, но в нормальной среде проще сразу иметь `arm-none-eabi-gcc` и `arm-none-eabi-gdb` доступными как обычные команды.

## Аппаратные артефакты

Текущий дефолтный путь для build-скриптов и JTAG-скриптов — `artifacts/fpga`. Готовые артефакты можно положить туда вручную или собрать из соседнего Vivado-проекта командой `./scripts/fpga/build_fpga.sh`.

| Артефакт | Для чего нужен | Текущий основной путь | Где используется |
|---|---|---|---|
| `design.xsa` | создание платформы Vitis и BSP | `artifacts/fpga/design.xsa` | `scripts/vitis/build.sh`, `scripts/vitis/build.tcl` |
| `design.bit` | программирование PL перед запуском | `artifacts/fpga/design.bit` | `scripts/vitis/run_jtag.sh`, `scripts/vitis/run_jtag.tcl` |
| `ps7_init.tcl` | инициализация PS7 при JTAG-старте | `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl` | `scripts/vitis/run_jtag.tcl`, debug flow |

В репозитории есть и резервная копия аппаратных артефактов в `artifacts/fpga/back/`, но рабочим источником по умолчанию остаются файлы из `artifacts/fpga/`. Для JTAG-запуска предусмотрен и legacy fallback на `../bvstk_hw/tmp/design.bit`, однако это именно fallback, а не основной путь, под который стоит документировать ежедневную работу.

Сборка Vivado пишет `create_project.jou/.log`, `build_hw.jou/.log` и backup-журналы в `artifacts/vivado/logs/`. Путь задаётся через `VIVADO_LOG_DIR` в `scripts/fpga/build_fpga.conf` или флагом `--log-dir`; вручную запускать Vivado из корня репозитория не требуется.

Если `XSA` не задан явно, `scripts/vitis/build.sh` берёт `artifacts/fpga/design.xsa`. Если `BITSTREAM_FILE` не задан и путь не передан аргументом, `scripts/vitis/run_jtag.tcl` сначала ищет `artifacts/fpga/design.bit`, затем legacy-вариант, и только потом пытается использовать bitstream из export-папки уже собранной платформы.

Практический смысл этого простой: сначала нужно убедиться, что `artifacts/fpga/design.xsa` и `artifacts/fpga/design.bit` действительно соответствуют тому аппаратному дизайну, с которым вы собираетесь работать. Иначе можно получить формально успешную сборку прошивки, которая будет запущена поверх другой версии hardware platform.

## Как сейчас устроена сборка

Основные пользовательские точки входа находятся в корне репозитория: `./build.sh` выбирает ОС, а `./run.sh` выбирает JTAG-flow. Скрипты в `scripts/vitis/`, `scripts/neutrino/` и `scripts/fpga/` остаются прямыми точками входа для настройки конкретного этапа.

`scripts/vitis/build.sh` делает очень немного сам по себе: подхватывает конфигурацию из `build_vitis.conf`, при необходимости подключает `settings64.sh`, выставляет `XSA` и `CLEAN`, а затем запускает `xsct` со скриптом `build.tcl`. Основная логика живёт именно в `build.tcl`: он создаёт или пересоздаёт `vitis_ws/`, генерирует `src/apps/freertos/config/default_configs.h`, создаёт платформу `plat_bvstk`, настраивает FreeRTOS и lwIP, включает `xilffs`, патчит FatFs под нужный режим и собирает приложение `app_bvstk`.

Минимальная сборка выглядит так:

```sh
cd <repo-root>
./build.sh freertos
```

Если нужно явно указать аппаратный экспорт или не хочется удалять уже существующий `vitis_ws/`, используются обычные переменные окружения:

```sh
XSA=/abs/path/to/design.xsa CLEAN=0 ./build.sh freertos
```

Скрипт понимает и `LWIP_LIB`, если приходится принудительно выбирать конкретную версию lwIP. По умолчанию он пробует `lwip220`, а если такая библиотека недоступна, откатывается к `lwip211`.

## Конфигурационные файлы build и JTAG

В `scripts/vitis/` лежат шаблоны `build_vitis.conf.example` и `run_jtag.conf.example`, а в рабочем дереве уже присутствует `build_vitis.conf`. Эти файлы полезны не как обязательный ритуал, а как место, где удобно зафиксировать machine-specific пути, не размазывая их по shell history.

| Файл | Назначение | Типичные поля |
|---|---|---|
| `scripts/fpga/build_fpga.conf` | сборка аппаратной платформы | `VIVADO_BIN`, `FPGA_DIR`, `OUTPUT_DIR`, `VIVADO_LOG_DIR`, `JOBS`, `CLEAN` |
| `scripts/vitis/build_vitis.conf` | параметры сборки Vitis | `XILINX_SETTINGS`, `XSA`, `CLEAN_DEFAULT` |
| `scripts/vitis/run_jtag.conf` | параметры JTAG-запуска | `XILINX_SETTINGS`, `BITSTREAM_FILE`, `ELF_FILE`, `PS7_INIT_TCL` |

Neutrino-скрипты настраиваются переменными окружения: `NEUTRINO_BSP_DIR`, `NEUTRINO_BASE_BUILD`, `NEUTRINO_BUILD_DIR`, `NEUTRINO_KEY_DIR`, `NEUTRINO_ROOT_SHADOW_FILE`, `QCC_VARIANT`, `NEUTRINO_IFS_FILE`, `UART_DEVICE`, `DEVICE_IP` и `SSH_IDENTITY`.

Если конфигурационные файлы не используются, все критичные параметры можно передавать через обычные environment variables. Для повседневной работы это часто даже проще, чем поддерживать несколько локальных конфигов.

## Рабочая структура репозитория

Репозиторий полезно воспринимать не как полный список папок, а как набор зон ответственности. Ниже приведена карта тех каталогов, которые реально важны в текущем цикле разработки.

| Путь | Роль в проекте |
|---|---|
| `artifacts/fpga/` | локальные `design.xsa` и `design.bit`, которые используются по умолчанию |
| `artifacts/vivado/logs/` | игнорируемые Git журналы пакетной сборки Vivado |
| `build/neutrino/` | `bvstkctl`, сгенерированный `.build`, IFS, UART-лог, локальные SSH-ключи и локальный `root.shadow` |
| `configs/` | шаблоны дефолтных JSON-конфигов, встраиваемых в прошивку |
| `docs/` | актуальная документация по архитектуре, сборке и эксплуатации |
| `scripts/fpga/` | пакетная сборка Vivado и экспорт `design.bit/design.xsa` |
| `scripts/vitis/` | сборка FreeRTOS в Vitis и JTAG-запуск ELF |
| `scripts/neutrino/` | сборка `bvstkctl`, IFS, JTAG-запуск и SSH-проверка Neutrino |
| `scripts/vscode/` | helper-скрипты для VSCode и step-debug |
| `scripts/compat/` | совместимость со старыми путями вызова |
| `src/` | исходники; Vitis получает target-specific source view из выбранных поддеревьев |
| `vitis_ws/` | генерируемый workspace Vitis с платформой, BSP и ELF |
| `web/` | статические web-ресурсы и скрипты их загрузки во flash |
| `.vscode/` | проектные настройки IntelliSense, tasks и launch-конфигурации |

Внутри `src/` принадлежность видна из пути. `src/apps/freertos/main.c` определяет порядок запуска задач и сервисов; `src/apps/freertos/config/` отвечает за `config_store`; `src/apps/freertos/storage/` и `src/ports/freertos-xilinx/` образуют файловый и BSP-слои. Внешние интерфейсы находятся в `src/apps/freertos/services/`, команды — в `src/apps/freertos/console/`, а OS-specific PL runtime — в `src/apps/freertos/drivers/pl/`. Общие PL cores находятся в `src/drivers/pl/`, policy/services — в `src/services/`, protocol adapters — в `src/protocols/`, contracts и hardware maps — в `src/shared/` и `src/hardware/`. Полная карта приведена в `source-layout.md`.

Отдельно важно понимать статус `vitis_ws/`. Это не исходный код, а производный артефакт сборки. Его можно удалить и пересоздать, но в процессе разработки он полезен не только как место, где лежит ELF, а ещё и как источник BSP-заголовков, `ps7_init.tcl`, экспортированной платформы и всего того, что использует VSCode для навигации по Xilinx-стеку.

## VSCode и навигация по BSP

В проекте уже есть настроенный `.vscode/`-набор: `tasks.json`, `launch.json`, `settings.json` и `c_cpp_properties.json`. Он завязан на `compile_commands.json` в корне репозитория и на пути внутри `vitis_ws/`.

Это означает, что IntelliSense в проекте по-настоящему полезен только после того, как были выполнены две вещи: собран или хотя бы сгенерирован `vitis_ws/`, и создан `compile_commands.json`. Генерируется он отдельным helper-скриптом:

```sh
./scripts/vscode/gen_compile_commands.sh
```

С практической точки зрения VSCode здесь используется не как замена Vitis IDE, а как более удобная оболочка для чтения кода, переходов по определениям и GDB attach. Именно поэтому `c_cpp_properties.json` включает не только `src/**`, но и заголовки из `vitis_ws/plat_bvstk/.../bsp/` и `lwip-*`.

## JTAG-запуск и step-debug

Обычный FreeRTOS JTAG-запуск через корневой диспетчер программирует PL, выполняет `ps7_init`, загружает `app_bvstk.elf` и запускает core0:

```sh
./run.sh freertos jtag
```

Если нужно переопределить bitstream, путь можно передать аргументом или через `BITSTREAM_FILE`:

```sh
./scripts/vitis/run_jtag.sh /abs/path/to/design.bit
```

Для step-debug предусмотрен отдельный режим:

```sh
./scripts/vitis/run_jtag.sh --debug
```

В этом режиме скрипт старается убедиться, что `hw_server` доступен не только на порту `3121`, но и с GDB-портом `3000`, после чего выполняет подготовку таргета через `scripts/vscode/jtag_prepare_debug.tcl` и оставляет core0 остановленным. Дальше можно подключаться из VSCode через конфигурацию `Attach: Zynq-7000 (core0, after ./run_jtag.sh --debug)`.

Neutrino запускается отдельным flow:

```sh
./build.sh neutrino-image
./run.sh neutrino jtag
```

По умолчанию образ создаётся с заблокированной учётной записью `root`. Для
включения входа по паролю сначала сгенерируйте локальный файл `root.shadow`:

```sh
./scripts/neutrino/generate_root_shadow.py
./build.sh neutrino-image
```

Файл сохраняется в `build/neutrino/root.shadow`, имеет права `0600` и
игнорируется Git. Для другого расположения используйте
`NEUTRINO_ROOT_SHADOW_FILE` при сборке образа. В работающем IFS `/etc/shadow`
доступен только для чтения, поэтому менять пароль командой `passwd` на плате
нельзя.

Он загружает IFS в DDR, проверяет старт по UART и затем выполняет SSH-проверку `bvstkctl`. Для этого должны быть корректно заданы UART-устройство и SSH identity.

Текущие `.vscode/tasks.json` и `.vscode/launch.json` уже отражают именно этот сценарий. Background task запускает `hw_server -s tcp::3121 -p 3000 -L-`, затем отдельная задача вызывает `xsct ${workspaceFolder}/scripts/vscode/jtag_prepare_debug.tcl`, а launch-конфигурация подключает `arm-none-eabi-gdb` к `localhost:3000` и загружает `vitis_ws/app_bvstk/Debug/app_bvstk.elf`.

## Что важно помнить про согласованность окружения

В этом проекте многие проблемы похожи на ошибки прошивки, хотя на самом деле начинаются раньше. Неактуальный `design.xsa`, несоответствующий `design.bit`, старый `vitis_ws/` после смены hardware export или VSCode, открытый до генерации BSP, дают очень похожие симптомы: отсутствующие адреса, странные include path, неверные IRQ, непредсказуемое поведение PL-подсистем или ложное ощущение, что код собран против другой платформы.

Поэтому нормальная последовательность работы выглядит так: сначала собрать или проверить `artifacts/fpga/design.xsa` и `artifacts/fpga/design.bit`, затем собрать выбранную ОС, после FreeRTOS-сборки при необходимости обновить `compile_commands.json`, и только после этого переходить к JTAG-запуску, сетевой проверке или step-debug.

## Что читать вместе с этим документом

Этот текст специально не дублирует руководство по сборке и не повторяет JTAG-последовательность покомандно во всех деталях. Если нужен более детальный разбор процесса, дальше стоит открыть соседние документы.

| Документ | Когда он нужен |
|---|---|
| `build.md` | когда нужно понять логику сборки, артефакты и переменные |
| `run-and-debug.md` | когда нужен JTAG-запуск, attach или первичная проверка сервисов |
| `hardware-platform.md` | когда вопрос упирается в соответствие firmware и аппаратного дизайна |
| `pl-cores.md` | когда работа касается I2C, SMI или SPI на границе PS и PL |
