# Совместная архитектура FreeRTOS и Neutrino

Репозиторий `bvstk` собирает два ОС-зависимых варианта одного продукта. Они используют общий контракт аппаратной платформы Zynq/PL, но имеют разные исполняемые форматы, системные API и способы интеграции с ОС.

```text
Vivado design -> design.bit + design.xsa
                         |
          общий контракт PL-регистров, BRAM и IRQ
                         |
          +--------------+----------------+
          |                               |
   FreeRTOS + Xilinx BSP           Neutrino + Zynq BSP
          |                               |
   app_bvstk.elf                 bvstkctl + загрузочный IFS
```

## Что является общим

- `src/pl_common/` хранит версионируемый контракт областей PL;
- `src/services_common/` задаёт нормализованные статусы и проверенный доступ к PL;
- `design.bit`, `design.xsa`, адреса MMIO, layout BRAM и IRQ должны соответствовать одной версии аппаратного дизайна;
- прикладная семантика операций над PL должна оставаться одинаковой независимо от ОС.

Общий код не должен зависеть от FreeRTOS, lwIP, Xilinx BSP или Neutrino API. ОС-зависимые адаптеры находятся ниже общего service API.

## Что различается

| Область | FreeRTOS | Neutrino |
|---|---|---|
| Платформенный слой | `src/platform/freertos/` | `src/platform/neutrino/` |
| Доступ к MMIO | Xilinx API и прямой MMIO | `ThreadCtl(_NTO_TCTL_IO)` и `mmap_device_memory()` |
| Основной результат | `vitis_ws/app_bvstk/Debug/app_bvstk.elf` | `build/neutrino/bvstkctl` |
| Загрузочный образ | JTAG-загрузка ELF | `build/neutrino/ifs-zynq7000-ax7020-bvstk.raw` |
| Текущий пользовательский слой | сеть, файловые системы, TCP/SSH-консоли, HTTP и DCP2 | `/usr/bin/bvstkctl` внутри IFS |

FreeRTOS-вариант остаётся наиболее полным runtime проекта. Neutrino-вариант уже использует общий PL-контракт и собирается в загрузочный IFS, но перенос сервисов FreeRTOS выполняется постепенно; наличие общей аппаратной модели не означает автоматического наличия одинаковых сетевых и файловых сервисов.

## Команды сборки

Корневой `build.sh` выбирает требуемый вариант:

```sh
./build.sh freertos       # FreeRTOS ELF
./build.sh neutrino       # только bvstkctl
./build.sh neutrino-image # bvstkctl и IFS для AX7020
./build.sh all            # FreeRTOS ELF и Neutrino IFS
```

FPGA собирается отдельно:

```sh
./scripts/fpga/build_fpga.sh
```

Результаты Vivado попадают в `artifacts/fpga/`, FreeRTOS workspace — в `vitis_ws/`, а результаты Neutrino — в `build/neutrino/`. SDK Neutrino и бинарный BSP являются внешними зависимостями и в репозиторий не копируются.

## Запуск по JTAG

```sh
./run.sh freertos jtag
./run.sh neutrino jtag
```

FreeRTOS-flow программирует PL, инициализирует PS7, загружает ELF и запускает core0. Neutrino-flow загружает IFS в DDR, проверяет сигнатуру старта по UART, после чего корневой `run.sh` запускает проверку `/usr/bin/bvstkctl` через SSH. Подробности и переменные переопределения описаны в [`run-and-debug.md`](run-and-debug.md).
