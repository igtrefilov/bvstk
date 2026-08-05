# Организация исходного кода

Каталог `src/` разделён по принадлежности и направлению зависимостей. Путь к
файлу должен однозначно отвечать на два вопроса: можно ли использовать код в
обеих ОС и какой runtime предоставляет его внешние зависимости.

```text
src/
├── shared/                       общий переносимый C-код
│   ├── base/                     статусы и разбор простых значений
│   ├── config/                   общие модели конфигурации
│   ├── interfaces/               MMIO, clock, platform и output contracts
│   ├── pl/access/                проверенный доступ к областям PL
│   └── protocols/                общие модели HTTP и DCP2
├── hardware/
│   ├── boards/ax7020/            карта MMIO/BRAM/IRQ целевой платы
│   └── pl/                       регистровые контракты FPGA-ядер
├── ports/
│   ├── freertos-xilinx/          FreeRTOS, Xilinx BSP и FatFs adapters
│   └── neutrino-zynq7000/        Neutrino/POSIX adapters для Zynq-7000
├── apps/
│   ├── freertos/                 единый ELF, задачи, сервисы и storage runtime
│   └── neutrino/                 отдельные Neutrino-приложения
└── vendor/lwip/                  локально сопровождаемые исходники lwIP
```

## Правила зависимостей

Допустимое направление зависимостей:

```text
apps ───────> shared ───────> hardware
 │              ▲                ▲
 ├────────────> ports ────────────┘
 └────────────> vendor
```

- `shared/` не включает FreeRTOS, lwIP, FatFs, Xilinx BSP или Neutrino API;
- `hardware/` описывает железо и не зависит от ОС;
- `ports/` реализует интерфейсы для конкретной пары ОС/BSP и не зависит от
  `apps/`;
- `apps/` является composition root и выбирает нужные сервисы и port;
- `vendor/` не считается проектным переносимым API;
- include-пути задаются от корня `src/`; переходы `../` запрещены.

Эти ограничения проверяет `./build.sh check`. Проверка также собирает и
запускает host-тест общего PL service API через fake MMIO port.

## FreeRTOS

FreeRTOS-сборка больше не подключает весь `src/`. `scripts/vitis/build.tcl`
создаёт внутри `vitis_ws` source view только из следующих корней:

- `src/apps/freertos/`;
- `src/hardware/`;
- `src/ports/freertos-xilinx/`, кроме canonical linker-каталога;
- `src/shared/`;
- `src/vendor/lwip/`.

`lscript.ld` и `Xilinx.spec` хранятся в target port, но для совместимости со
сгенерированным Vitis make flow дополнительно появляются в корне производного
source view.

## Neutrino

Neutrino-сборка имеет отдельный явный список исходников в
`scripts/neutrino/build.sh`. Текущий `bvstkctl` использует общий PL access API,
аппаратный контракт AX7020 и Neutrino MMIO port. FreeRTOS-сервисы и Xilinx BSP
в эту сборку не попадают.

При появлении фоновой обработки IRQ или autopoll на Neutrino владельцем MMIO
должен стать отдельный сервис/resource manager, а `bvstkctl` — его клиентом.
Это не требует переносить FreeRTOS task model в процессную модель Neutrino.

## Где искать код

| Задача | Путь |
|---|---|
| FreeRTOS startup | `src/apps/freertos/main.c` |
| Shell dispatcher и команды | `src/apps/freertos/console/` |
| TCP, SSH, HTTP, DCP2 | `src/apps/freertos/services/` |
| I2C, SMI, SPI runtime | `src/apps/freertos/drivers/pl/` |
| SD/QSPI/FatFs runtime | `src/apps/freertos/storage/` |
| QSPI geometry and FatFs window | `src/hardware/boards/ax7020/bvstk_qspi_layout.h` |
| Xilinx BSP integration | `src/ports/freertos-xilinx/` |
| Neutrino MMIO integration | `src/ports/neutrino-zynq7000/` |
| AX7020 address map | `src/hardware/boards/ax7020/` |
| FPGA register maps | `src/hardware/pl/` |
| Общие модели и API | `src/shared/` |
| `bvstkctl` | `src/apps/neutrino/bvstkctl/` |
