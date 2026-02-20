# VSCode step debug для Zynq-7000 (AX7020) + FreeRTOS через JTAG

Цель: ставить брейкпоинты и дебажить пошагово приложение FreeRTOS на Zynq-7000 из VSCode.

В этом репозитории используется стандартный Xilinx стек:

- `hw_server` — доступ к JTAG (TCF agent) и GDB-bridge порты
- `xsct` — скриптовая подготовка (program PL + `ps7_init` + halt CPU)
- `arm-none-eabi-gdb` — отладчик, который подключается к `hw_server` по GDB Remote
- VSCode `C/C++` debug (`cppdbg`) — UI для брейкпоинтов/step/variables

## 1) Требования

### 1.1 VSCode

- Расширение Microsoft: `C/C++` (ms-vscode.cpptools).

### 1.2 Xilinx Vitis (hw_server/xsct)

Нужно, чтобы команды были в `PATH`:

```bash
hw_server -h
xsct -h
```

Обычно это делается через:

```bash
source <Vitis>/settings64.sh
```

### 1.3 ARM GDB

Проверь:

```bash
arm-none-eabi-gdb --version
```

Если не находится — добавь в `PATH` директорию toolchain `.../gcc-arm-none-eabi/bin` (или задай `VITIS_ARM_GCC_BIN` как в `docs/VSCODE_XILINX.md`).

## 2) Что происходит при дебаге (важно для Zynq)

Чтобы можно было загрузить ELF в DDR и корректно шагать по коду:

1. Нужно **инициализировать PS7** (такты/DDR) — это делает `ps7_init.tcl`.
2. Для проектов с PL нужно **запрограммировать битстрим** (опционально, но обычно нужно).
3. CPU должен быть **halted** перед тем, как подключится GDB.

Эти шаги выполняет скрипт:

- `scripts/vscode/jtag_prepare_debug.tcl`

## 3) Как запускать дебаг из VSCode

В репозитории уже добавлены:

- `.vscode/tasks.json` — стартует `hw_server` и выполняет `xsct scripts/vscode/jtag_prepare_debug.tcl`
- `.vscode/launch.json` — подключает `arm-none-eabi-gdb` к `hw_server` и делает `load` + `tbreak main`

Порядок действий:

1. Собери Debug-версию ELF (важно: с `-g`, желательно `-O0`):
   - ELF по умолчанию: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`
2. В VSCode открой корень репозитория.
3. `Run and Debug` → выбери конфигурацию:
   - `Attach: Zynq-7000 (hw_server GDB, core0)` (полный автомат: hw_server + xsct prepare)
4. Нажми Start Debugging.
5. Поставь брейкпоинт и нажми Continue — приложение остановится на `main`, дальше можно `Step Over/Into`.

Альтернатива без VSCode (подготовка через существующий скрипт запуска):

```bash
./run_jtag.sh --debug [path/to/bitstream.bit]
```

Это сделает program PL + `ps7_init` и оставит core0 остановленным для подключения GDB.

После этого в VSCode выбирай конфигурацию:

- `Attach: Zynq-7000 (core0, after ./run_jtag.sh --debug)`

Важно про порты:

- `hw_server` всегда пишет URL вида `TCP:<host>:3121` — это **TCF порт** для `xsct/xsdb`.
- GDB подключается к **отдельному порту** (для ARM32 обычно `3000`). Он может не печататься в stdout.
- Поэтому в `launch.json` используется `target remote :3000`.

## 4) Bitstream path (если у тебя другой файл)

По умолчанию скрипт берет битстрим из exported platform:

- `vitis_ws/plat_bvstk/export/plat_bvstk/hw/Burevestnik_top.bit`

Если у тебя другой `.bit`, можно перед запуском дебага задать переменную:

```bash
export BITSTREAM_FILE=/abs/path/to/your.bit
```

Запуск из VSCode task унаследует переменную из окружения, из которого был запущен VSCode.

## 5) Типичные проблемы

### 5.1 VSCode не ставит/не ловит брейкпоинты

Причины и решения:

- Код в Flash/ROM: нужны аппаратные брейкпоинты, их мало → загружай ELF в DDR (у нас делается `load`).
- Оптимизация: `-O2/-O3` мешают пошаговому дебагу → используй Debug (`-O0`/`-Og`) + `-g`.
- CPU не halted / не сделан `ps7_init` → запусти `Xilinx: JTAG prepare for debug (xsct)`.

### 5.2 `xsct`/`hw_server` не находится

Обычно забыли `source <Vitis>/settings64.sh`.

### 5.3 Порт 3000 занят

`hw_server` по умолчанию поднимает GDB-порт(ы) с базы `3000`. Если занято — поменяй `-p 3000` в `.vscode/tasks.json` и `localhost:3000` в `.vscode/launch.json`.
