# Сборка

Этот документ описывает текущую цепочку сборки `bvstk`: аппаратную платформу в Vivado, FreeRTOS-приложение через XSCT/Vitis и приложение с загрузочным образом Neutrino. Если сборочные скрипты меняются, этот текст тоже должен обновляться, потому что именно скрипты определяют фактический build flow.

## Как сейчас выглядит сборка

Штатная точка входа для программной части находится в корне репозитория:

```sh
./build.sh check
./build.sh freertos
./build.sh neutrino
./build.sh neutrino-image
./build.sh all
```

Корневой `build.sh` является диспетчером. Он передаёт управление в `scripts/vitis/build.sh` для FreeRTOS, в `scripts/neutrino/build.sh` для отдельной утилиты Neutrino и в `scripts/neutrino/build_image.sh` для загрузочного IFS. Команда `all` собирает FreeRTOS ELF и Neutrino IFS. Она не запускает Vivado.

Команда `check` проверяет направление зависимостей между слоями, запрещённые
include-зависимости и собирает host-тесты переносимого PL access API без Vitis
или Neutrino SDK.

FreeRTOS-скрипт `scripts/vitis/build.sh` определяет корень репозитория, при наличии подхватывает `build_vitis.conf`, опционально подключает `settings64.sh` через `XILINX_SETTINGS`, выбирает `XSA`, решает, надо ли удалять старый `vitis_ws/`, а затем запускает `xsct scripts/vitis/build.tcl`.

Основная FreeRTOS-логика находится в `scripts/vitis/build.tcl`. Именно этот скрипт создаёт workspace, генерирует встроенные дефолтные конфиги, создаёт платформу `plat_bvstk`, настраивает BSP, включает `lwIP` и `xilffs`, подменяет `diskio.c` на проектную версию, патчит FatFs для LFN/FreeRTOS и собирает приложение `app_bvstk`.

## Сборка аппаратной платформы

Исходный Vivado-проект `Burevestnik_21` хранится во внешнем каталоге `hw_platform/fpga`, но управляемая сборка запускается из `bvstk`:

```sh
./scripts/fpga/build_fpga.sh
```

Параметры машины задаются в `scripts/fpga/build_fpga.conf`; шаблон находится рядом в `build_fpga.conf.example`. Основные параметры:

| Параметр | Назначение | Значение по умолчанию |
|---|---|---|
| `FPGA_DIR` | каталог с `Burevestnik_21.tcl` | соседний `hw_platform/fpga` |
| `VIVADO_BIN` | команда или полный путь к Vivado | `vivado` во встроенных defaults |
| `OUTPUT_DIR` | каталог для `design.bit` и `design.xsa` | `artifacts/fpga/` |
| `VIVADO_LOG_DIR` | журналы и journal-файлы Vivado | `artifacts/vivado/logs/` |
| `JOBS` | параллелизм implementation | `8` |
| `CLEAN` | удалить `vivado_project` перед сборкой | `0` |

Параметры можно временно переопределять через CLI, например:

```sh
./scripts/fpga/build_fpga.sh --jobs 12 --clean
./scripts/fpga/build_fpga.sh --log-dir /tmp/bvstk-vivado-logs
```

Vivado запускается с явными `-journal` и `-log`, поэтому `create_project.jou/.log`, `build_hw.jou/.log` и создаваемые Vivado backup-файлы остаются в `artifacts/vivado/logs/`, а не засоряют корень репозитория. Каталог журналов игнорируется Git.

## Минимальный рабочий сценарий

Если `artifacts/fpga/design.xsa` и `design.bit` уже актуальны, повторно запускать Vivado не требуется. Минимальная сборка FreeRTOS выглядит так:

```sh
source <Vitis-install>/settings64.sh
cd <repo-root>
./build.sh freertos
```

По умолчанию FreeRTOS-скрипт берёт аппаратный экспорт из `artifacts/fpga/design.xsa`. Если там лежит экспорт от другой версии аппаратного дизайна, собираться будет не та hardware/software комбинация.

Если нужно собрать прошивку на основе другого `xsa`, путь передаётся обычной переменной окружения:

```sh
XSA=/abs/path/to/design.xsa ./build.sh freertos
```

Если требуется пересборка без удаления уже существующего `vitis_ws/`, используется `CLEAN=0`:

```sh
XSA=/abs/path/to/design.xsa CLEAN=0 ./build.sh freertos
```

Практически это означает следующее. `CLEAN=1` полезен, когда менялся hardware export, BSP или что-то в самой логике генерации workspace. `CLEAN=0` уместен, когда нужно просто быстро перестроить приложение внутри уже существующего `vitis_ws/` и вы уверены, что старая платформа не устарела.

Минимальная сборка загрузочного образа Neutrino:

```sh
source /etc/profile.d/kpda_env_2024.sh
./build.sh neutrino-image
```

Если `qcc` или `mkifs` не найдены, Neutrino-скрипты сами пытаются подключить `/etc/profile.d/kpda_env_2024.sh`. По умолчанию используется внешний BSP `kpda-bsp-xilinx-zynq7000` и его `images/zynq7000-ax7020-ssh.build`; пути можно изменить через `NEUTRINO_BSP_DIR` и `NEUTRINO_BASE_BUILD`.

Сборка IFS не создаёт `ps7_init.tcl`. Для последующего JTAG-запуска должен уже существовать export Vitis в `vitis_ws/plat_bvstk/export/plat_bvstk/hw/` либо путь нужно явно передать через `PS7_INIT_TCL`.

## Какие входные данные использует сборка FreeRTOS

Сборка опирается на небольшой набор артефактов и скриптов, каждый из которых играет свою роль.

| Что используется | Откуда берётся | Зачем нужно |
|---|---|---|
| `design.xsa` | `artifacts/fpga/design.xsa` или `XSA=...` | создание Vitis platform и BSP |
| `gen_default_configs.py` | `tools/codegen/gen_default_configs.py` | генерация `src/apps/freertos/config/default_configs.h` из `configs/` |
| `patch_ffconf_lfn.py` | `tools/codegen/patch_ffconf_lfn.py` | приведение FatFs в BSP к нужной конфигурации |
| `diskio.c` | `src/ports/freertos-xilinx/fs-fatfs/diskio.c` | замена сгенерированного `xilffs`-слоя на проектную реализацию |
| `configs/` | `configs/network.json`, `configs/i2c/*.json`, `configs/smi/*.json` | источник встроенных дефолтных JSON-конфигов |

Из этого видно, что сборка в `bvstk` не сводится к компиляции `src/*.c`. В процессе build создаётся и проектируется часть runtime-среды: в проект вшиваются дефолтные конфиги, модифицируется BSP и подменяется файловый слой для SD/QSPI. Поэтому изменения в `configs/` или в `src/ports/freertos-xilinx/fs-fatfs/diskio.c` так же влияют на итоговый артефакт, как и изменения в прикладном C-коде.

## Переменные окружения FreeRTOS

Текущий build flow использует прежде всего переменные окружения. Это нормальный интерфейс для повседневной работы, а не побочный механизм.

| Переменная | Что делает | Поведение по умолчанию |
|---|---|---|
| `XSA` | указывает путь к аппаратному export | `artifacts/fpga/design.xsa` |
| `CLEAN` | управляет пересозданием `vitis_ws/` | `1` |
| `LWIP_LIB` | принудительно задаёт имя lwIP-библиотеки BSP | сначала `lwip220`, затем `lwip211` |
| `BUILD_VITIS_CONFIG` | переопределяет путь к build-конфигу | `scripts/vitis/build_vitis.conf` |
| `XILINX_SETTINGS` | путь к `settings64.sh`, который надо source-нуть перед build | не используется, если не задан |
| `BVSTK_SSH_ENABLE` | включает SSH-сервер FreeRTOS | выключен |
| `BVSTK_WOLFSSL_ROOT` / `BVSTK_WOLFSSH_ROOT` | пути к ARM-сборкам wolfSSL/wolfSSH; wolfSSH должен быть собран с SCP | обязательны только при включённом SSH |
| `BVSTK_SSH_USER` / `BVSTK_SSH_PASSWORD` | учётные данные SSH | `root` / обязательный пароль при включённом SSH |

С практической стороны `build_vitis.conf` удобен как место для локальной machine-specific настройки, но он не обязателен. Если окружение уже активировано вручную, а путь к `xsa` передан через `XSA`, сборка полностью работоспособна и без него.

Типовой вызов с явной конфигурацией выглядит так:

```sh
XILINX_SETTINGS=/opt/Xilinx/Vitis/2024.2/settings64.sh \
XSA=/abs/path/to/design.xsa \
LWIP_LIB=lwip220 \
./build.sh freertos
```

Для сборки FreeRTOS с SSH см. [`freertos-ssh.md`](freertos-ssh.md). Включённый SSH увеличивает требуемый FreeRTOS heap и добавляет статические библиотеки wolfSSL/wolfSSH в линковку.

## Что именно делает FreeRTOS `build.tcl`

`scripts/vitis/build.tcl` полезно понимать не как чёрный ящик, а как последовательность вполне конкретных шагов.

| Шаг | Смысл |
|---|---|
| удаление `vitis_ws/` при `CLEAN=1` | очистка производного workspace перед полной пересборкой |
| генерация `src/apps/freertos/config/default_configs.h` | вшивание актуальных дефолтных JSON-конфигов в прошивку |
| `platform create -name plat_bvstk -hw $XSA ...` | создание платформы под `ps7_cortexa9_0` и `freertos10_xilinx` |
| настройка heap и библиотек BSP | подготовка FreeRTOS, lwIP и FatFs под нужную конфигурацию |
| копирование проектного `src/ports/freertos-xilinx/fs-fatfs/diskio.c` в BSP | подмена стандартной реализации файлового слоя |
| `bsp regenerate` и patch FatFs | обновление BSP и фиксация нужной конфигурации `ffconf` |
| `app create -name app_bvstk ...` | создание приложения как `Empty Application(C)` |
| target-specific source view в `vitis_ws/app_bvstk/src` | подключение только FreeRTOS app, shared, hardware, FreeRTOS/Xilinx port и lwIP vendor |
| `platform generate` и `app build` | генерация платформы и сборка `app_bvstk.elf` |

Эта последовательность важна при отладке сборки. Если, например, в build log нет стадии генерации `default_configs.h`, значит проблема может быть не в компиляции, а в Python-скрипте генерации. Если ломается `xilffs`, смотреть нужно не только в код приложения, но и в BSP-папку внутри `vitis_ws/`, потому что именно там выполняется подмена `diskio.c` и патч `ffconf`.

## Что получается в результате

После успешной сборки результаты разделены по этапам и ОС:

| Команда | Основной результат |
|---|---|
| `scripts/fpga/build_fpga.sh` | `artifacts/fpga/design.bit`, `artifacts/fpga/design.xsa` |
| `./build.sh freertos` | `vitis_ws/app_bvstk/Debug/app_bvstk.elf` |
| `./build.sh neutrino` | `build/neutrino/bvstkctl` |
| `./build.sh neutrino-image` | `build/neutrino/ifs-zynq7000-ax7020-bvstk.raw` |

FreeRTOS build производит не только ELF. Он создаёт целую локальную Vitis-среду, внутри которой лежат платформа, BSP, экспорт аппаратной части и служебные IDE-файлы.

Ниже приведена актуальная карта наиболее важных результатов сборки.

| Путь | Что это такое |
|---|---|
| `vitis_ws/app_bvstk/Debug/app_bvstk.elf` | основной ELF приложения |
| `vitis_ws/app_bvstk/src` | производное дерево symlink-файлов только для FreeRTOS target |
| `vitis_ws/app_bvstk/_ide/bitstream/design.bit` | копия bitstream в служебной структуре IDE |
| `vitis_ws/app_bvstk/_ide/psinit/ps7_init.tcl` | копия PS7 init для IDE/debug flow |
| `vitis_ws/plat_bvstk/hw/design.xsa` | локальная копия hardware export внутри платформы |
| `vitis_ws/plat_bvstk/hw/design.bit` | локальная копия bitstream внутри платформы |
| `vitis_ws/plat_bvstk/hw/ps7_init.tcl` | PS7 init, который используется JTAG-скриптами |
| `vitis_ws/plat_bvstk/export/plat_bvstk/plat_bvstk.xpfm` | экспорт платформы |
| `vitis_ws/plat_bvstk/zynq_fsbl/fsbl.elf` | автоматически созданный FSBL |

Стоит обратить внимание на один момент: в актуальном `vitis_ws/` export-папка платформы содержит `plat_bvstk.xpfm`, а аппаратные файлы `design.xsa`, `design.bit` и `ps7_init.tcl` лежат в `vitis_ws/plat_bvstk/hw/`. Для JTAG-запуска проект сейчас опирается именно на эти пути, а не на старые предположения о расположении артефактов.

## Как устроен `vitis_ws`

`vitis_ws/` полезно воспринимать как производный, но всё же значимый слой проекта. Его можно удалять и пересоздавать, однако во время разработки он играет роль не только сборочной директории, но и локального слепка платформы. Именно поэтому VSCode, JTAG debug и часть служебных скриптов завязаны на файлы внутри `vitis_ws/`.

Структурно `vitis_ws/` распадается на три ключевые зоны. `app_bvstk/` содержит само приложение и его build-артефакты. `plat_bvstk/` содержит platform, BSP, hardware snapshot и export. `app_bvstk_system/` и `.metadata/` относятся в основном к IDE-инфраструктуре Vitis/Eclipse и обычно интересуют разработчика только тогда, когда приходится разбираться в внутренней механике workspace.

## Что важно помнить при инкрементальной сборке FreeRTOS

Инкрементальная пересборка в этом проекте требует чуть больше дисциплины, чем в обычном standalone C-проекте. `CLEAN=0` ускоряет цикл, но не гарантирует корректность, если между сборками изменился `design.xsa`, состав BSP-библиотек, логика генерации `default_configs.h` или patch-процедура FatFs. В таких случаях лучше явно пересоздать workspace.

Есть и обратная сторона. Если менялся только код внутри уже подключённых FreeRTOS source roots, а платформа и BSP оставались прежними, полная чистка `vitis_ws/` обычно избыточна. Source view содержит symlink-файлы на исходники репозитория, поэтому rebuild приложения видит изменения без копирования. После добавления нового файла или изменения состава target roots следует повторно запустить `scripts/vitis/build.sh`, чтобы source view был сформирован заново.

## Типичные причины проблем со сборкой

Для этого проекта характерны не только обычные компиляторные ошибки, но и инфраструктурные проблемы. Наиболее частые причины выглядят так:

| Симптом | Что проверять первым |
|---|---|
| `xsct not found in PATH` | активировано ли Xilinx-окружение или задан ли `XILINX_SETTINGS` |
| `Vivado executable ... not found` | корректны ли `VIVADO_BIN` и `XILINX_SETTINGS` в `scripts/fpga/build_fpga.conf` |
| Vivado-журналы появились не там | какое значение получил `VIVADO_LOG_DIR`, не запускался ли Vivado вручную из корня репозитория |
| build идёт против “не той” платформы | какой именно `XSA` был подхвачен, и актуален ли `artifacts/fpga/design.xsa` |
| сломался `xilffs` или поведение ФС изменилось | применился ли project `diskio.c`, не сломался ли patch `ffconf` |
| после обновления hardware export поведение стало странным | был ли `vitis_ws/` пересоздан, а не использован инкрементально |
| не обновились встроенные дефолтные конфиги | отработал ли `gen_default_configs.py`, пересобрался ли ELF после изменения `configs/` |
| `qcc` или `mkifs` не найден | подключён ли SDK через `/etc/profile.d/kpda_env_2024.sh` |
| Neutrino BSP или base build не найден | корректны ли `NEUTRINO_BSP_DIR` и `NEUTRINO_BASE_BUILD` |

Архитектурно все эти проблемы нормальны для проекта, где сборка включает не только компиляцию кода, но и создание платформы, модификацию BSP и генерацию runtime-данных. Поэтому диагностика build-пайплайна здесь должна начинаться не с последней ошибки компилятора, а с понимания того, на каком именно шаге сломалась сборка.

## Что читать вместе с этим документом

Этот текст описывает build flow, но не заменяет документы про среду, запуск и общую архитектуру.

| Документ | Когда к нему переходить |
|---|---|
| `development-environment.md` | когда нужно проверить, что build запускается в корректной среде |
| `run-and-debug.md` | когда собранный ELF нужно загрузить и отладить |
| `architecture.md` | когда нужно понять, зачем сборка создаёт именно такие артефакты |
| `hardware-platform.md` | когда вопрос упирается в связь между firmware и `design.xsa` |
| `multi-os.md` | когда нужно понять границу общего кода, FreeRTOS и Neutrino |
| `freertos-ssh.md` | когда FreeRTOS нужно собрать со встроенной SSH-консолью |
