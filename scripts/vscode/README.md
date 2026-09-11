# Средства редактора и GDB

`gen_compile_commands.sh` пересоздаёт `compile_commands.json` через
Bear и принудительную пересборку приложения, FSBL и BSP. Это не только
чтение существующего проекта; сначала нужна собранная платформа Vitis.

`jtag_prepare_debug.tcl` подготавливает PL/PS и оставляет Cortex-A9 № 0
остановленным для GDB. Штатный вход — `scripts/vitis/run_jtag.sh --debug`.
Обёртка `arm-none-eabi-gdb.sh` выбирает компиляторный комплект и рабочий каталог.

[Полная инструкция отладки](../../docs/build/debug.md) описывает VSCode,
ручное подключение и UART.
[Справочник параметров](../../docs/reference/build-options.md#отладочные-инструменты)
содержит `VITIS_ARM_GCC_BIN`, `VITIS_ARM_GDB`, `GDB_CWD`
и `BEAR_LIBRARY_PATH`. Путь результата локален для рабочей станции;
не исправляйте базу компиляции вручную.
