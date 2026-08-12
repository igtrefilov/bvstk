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

- `src/hardware/pl/` хранит регистровые контракты FPGA-ядер;
- `src/hardware/boards/ax7020/` задаёт общую для двух ОС карту MMIO, BRAM и IRQ;
- `src/shared/` содержит нормализованные статусы, модели, contracts, события и проверенный доступ к PL;
- `src/drivers/pl/` содержит OS-independent transaction cores для I2C, SMI и SPI;
- `src/services/` содержит общие device/policy/cache/autopoll сервисы;
- `src/protocols/dcp2/` содержит общий codec и request handler поверх control facade;
- `src/shared/interfaces/` определяет узкие контракты, реализуемые target ports;
- `design.bit`, `design.xsa`, адреса MMIO, layout BRAM и IRQ должны соответствовать одной версии аппаратного дизайна;
- прикладная семантика операций над PL должна оставаться одинаковой независимо от ОС.

Общий код не зависит от FreeRTOS, lwIP, Xilinx BSP или Neutrino API.
ОС-зависимые адаптеры находятся в `src/ports/`, а выбор исходников выполняется
сборкой, не условными блоками `__QNXNTO__`. Полные правила и карта каталогов
описаны в [`source-layout.md`](source-layout.md).

## Что различается

| Область | FreeRTOS | Neutrino |
|---|---|---|
| Composition root | `src/apps/freertos/` | `src/apps/neutrino/` |
| Платформенный слой | `src/ports/freertos-xilinx/` | `src/ports/neutrino-zynq7000/` |
| Доступ к MMIO | Xilinx API и прямой MMIO | `ThreadCtl(_NTO_TCTL_IO)` и `mmap_device_memory()` |
| Основной результат | `vitis_ws/app_bvstk/Debug/app_bvstk.elf` | `build/neutrino/bvstkctl` + `build/neutrino/bvstkd` |
| Загрузочный образ | JTAG-загрузка ELF | `build/neutrino/ifs-zynq7000-ax7020-bvstk.raw` |
| Текущий пользовательский слой | сеть, файловые системы, TCP/SSH-консоли, HTTP и DCP2 | `/usr/bin/bvstkctl` + `/usr/bin/bvstkd` внутри IFS |

FreeRTOS-вариант сохраняет наиболее полный сетевой, файловый и legacy slave/IRQ runtime. Его I2C/SMI/SPI policy surfaces уже используют общие services, а master transaction cores и DCP2 control переиспользуются из `src/drivers/`, `src/services/` и `src/protocols/`. Neutrino daemon использует те же слои напрямую; сеть и файловое хранение остаются отдельными composition-root adapters.

## Команды сборки

Корневой `build.sh` выбирает требуемый вариант:

```sh
./build.sh check          # границы слоёв и host-тесты shared-кода
./build.sh freertos       # FreeRTOS ELF
./build.sh neutrino       # bvstkctl и bvstkd
./build.sh neutrino-image # bvstkctl, bvstkd и IFS для AX7020
./build.sh all            # FreeRTOS ELF и Neutrino IFS
```

FPGA собирается отдельно:

```sh
./scripts/fpga/build_fpga.sh
```

Результаты Vivado попадают в `artifacts/fpga/`, FreeRTOS workspace — в `vitis_ws/`, а результаты Neutrino — в `build/neutrino/`. Компилятор и утилита `mkifs` из SDK Neutrino остаются внешней зависимостью хост-машины, а необходимый для AX7020 BSP runtime snapshot хранится в `third_party/neutrino/bsp/ax7020/`.

## Запуск по JTAG

```sh
./run.sh freertos jtag
./run.sh neutrino jtag
```

FreeRTOS-flow программирует PL, инициализирует PS7, загружает ELF и запускает core0. Neutrino-flow загружает IFS в DDR, проверяет сигнатуру старта по UART, после чего корневой `run.sh` запускает проверку `/usr/bin/bvstkctl` через SSH. Подробности и переменные переопределения описаны в [`run-and-debug.md`](run-and-debug.md).
