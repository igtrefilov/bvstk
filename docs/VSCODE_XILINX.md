# VSCode для Xilinx/Vitis проектов (навигация по BSP/платформе)

Цель: чтобы в VSCode работали `F12 (Go to Definition)`, автодополнение и поиск символов не только по `app`, но и по платформенной части (BSP/FSBL), включая `xparameters.h`.

Ключевая идея: VSCode C/C++ extension должен видеть **те же флаги компиляции**, что и сборка Vitis (`-I...`, `-D...`, `-mcpu...`). Для этого используется `compile_commands.json` (Compilation Database).

## 1) Требования

### 1.1 VSCode + расширение C/C++

- Установи расширение Microsoft: `C/C++` (идентификатор: `ms-vscode.cpptools`).

### 1.2 Bear (генератор compile_commands.json)

На Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y bear
```

Проверка:

```bash
bear --version
```

### 1.3 ARM toolchain (arm-none-eabi-gcc)

Проект собирается `arm-none-eabi-gcc`. Обычно он идет вместе с установленным Xilinx Vitis.

Проверка:

```bash
arm-none-eabi-gcc --version
```

Если команда не находится, нужно:

- либо добавить путь к toolchain в `PATH`,
- либо перед запуском скрипта задать переменную окружения `VITIS_ARM_GCC_BIN` (это *директория* `.../bin` где лежит `arm-none-eabi-gcc`).

Пример:

```bash
export VITIS_ARM_GCC_BIN=/path/to/Vitis/.../gcc-arm-none-eabi/bin
```

## 2) Генерация compile_commands.json

В репозитории есть скрипт:

- `scripts/gen_compile_commands.sh`

Он запускает `make` для `app`, `fsbl` и `bsp` через `bear` и пишет итоговую compilation database в:

- `compile_commands.json` (в корне репозитория)

Запуск:

```bash
cd <repo-root>
./scripts/gen_compile_commands.sh
```

Примечание:

- `compile_commands.json` специально **не коммитится** (генерируемый файл, зависит от путей на машине).
- После `git clone` файл обычно отсутствует, это нормально. Сгенерируй его локально командой `./scripts/gen_compile_commands.sh`.
- Внутри базы могут быть абсолютные пути к локальному toolchain (например к `arm-none-eabi-gcc`), поэтому файл должен генерироваться на каждой машине отдельно.

## 3) Настройка VSCode

В репозитории уже лежат настройки:

- `.vscode/c_cpp_properties.json`
- `.vscode/settings.json`

Они указывают C/C++ extension использовать:

- `${workspaceFolder}/compile_commands.json`

Что нужно сделать:

1. Открой в VSCode **корень** репозитория (`File → Open Folder...`).
2. После генерации `compile_commands.json` перезагрузи окно:
   - `Developer: Reload Window`
3. Если навигация не обновилась сразу, выполни:
   - `C/C++: Reset IntelliSense Database`

## 4) Проверка (что всё работает)

Открой любой файл, где есть:

```c
#include "xparameters.h"
```

Наведи курсор на `xparameters.h` / символы из BSP и нажми `F12`.
Если `compile_commands.json` корректный, VSCode должен переходить в BSP include директории (например `.../bspinclude/include/xparameters.h`).

## 5) Типичные проблемы

### 5.1 `arm-none-eabi-gcc: not found`

Решение:

- убедись, что Vitis toolchain установлен,
- задай `VITIS_ARM_GCC_BIN` или добавь `.../bin` в `PATH`.

### 5.2 “Go to Definition” работает только в `app`

Почти всегда значит, что VSCode не использует `compile_commands.json` (его нет / он пустой / VSCode открыт не в корне проекта).

Проверь:

- файл `compile_commands.json` существует и не пустой,
- VSCode открыт в корне репозитория,
- `C_Cpp.default.compileCommands` указывает на `${workspaceFolder}/compile_commands.json`,
- сделай `C/C++: Reset IntelliSense Database`.

### 5.3 Несколько платформ/домейнов (несколько BSP)

Если в workspace несколько платформ/домейнов, в `compile_commands.json` могут оказаться записи для нескольких BSP. Обычно это нормально.
Если VSCode уводит не в тот `xparameters.h`, открой правильный файл из нужного домейна и убедись, что собираешь именно ту конфигурацию, которая тебе нужна.
