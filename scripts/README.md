# Инструменты проекта

Сценарии вызываются из корня BVSTK. Этот каталог содержит точки входа,
а полные инструкции находятся в документации; локальные пути задаются
в доверенных файлах `.conf` или параметрах сценария.

| Каталог | Задача | Руководство |
|---|---|---|
| `fpga/` | Vivado, bitstream и XSA | [Аппаратная сборка](../docs/build/fpga.md) |
| `vitis/` | FreeRTOS, JTAG, BOOT.bin и QSPI | [FreeRTOS](../docs/build/freertos.md), [JTAG](../docs/build/jtag.md), [QSPI boot](../docs/build/qspi.md) |
| `neutrino/` | Программы, IFS, PS SD boot, JTAG и проверка SSH | [Нейтрино](../docs/build/neutrino.md) |
| `vscode/` | База компиляции и GDB | [Отладка](../docs/development/debugging.md) |
| `dcp2/` | Клиент файлов и событий | [Работа с DCP2](../docs/interfaces/dcp2/client.md) |
| `compat/` | Переход со старых путей сценариев | Для новых команд используйте корневые `build.sh` и `run.sh` |

`check_docs.sh` проверяет документацию. `check_architecture.sh`
дополнительно проверяет границы исходников и запускает тесты на компьютере.
[Подготовка среды](../docs/build/environment.md) описана отдельно.

Сценарии сборки могут очищать производные каталоги, JTAG сбрасывает плату.
Пароли и приватные ключи не добавляют к исходникам и журналам.
