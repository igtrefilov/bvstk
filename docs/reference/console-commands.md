# Справочник консольных команд

## 1. Транспорт и синтаксис

Справочник относится к FreeRTOS command dispatcher. Он доступен через TCP `8888` и через FreeRTOS SSH `22`, если SSH включён в сборке.

Числа принимаются в десятичном виде и в формате `0x...`. Имена команд и большинство ключевых слов сравниваются без учёта регистра.

## 2. Общие команды

| Команда | Синтаксис | Назначение |
|---|---|---|
| help | `help` | вывести список команд |
| fs | `fs` или `fs -h` | справка файлового слоя |
| fs format | `fs format sd-pl confirm` | разрушительно форматировать PL SD в FAT32 |
| reboot | `reboot -y [delay_ms]` | перезапуск с подтверждением |
| reboot | `reboot confirm [delay_ms]` | альтернативная форма подтверждения |
| quit | `quit` или `exit` | закрыть сессию |

Пример:

```text
help
reboot -y 1000
```

## 3. Файловая навигация

| Команда | Синтаксис |
|---|---|
| pwd | `pwd` |
| ls | `ls [path]` |
| cd | `cd <path>` |
| mkdir | `mkdir <path>` |
| touch | `touch <path>` |
| cat | `cat <path>` |
| rm | `rm <path>` или `rm -r <path>` |
| cp | `cp <src> <dst>` или `cp -r <src> <dst>` |
| mv | `mv <src> <dst>` |

Поддерживаются абсолютные и относительные пути, а также префиксы `0:/`, `1:/`,
`2:/`, `sd:/`, `sd-pl:/`, `flash:/`, `sd/`, `sd-pl/` и `flash/`.

```text
cd flash
ls /config
cp flash:/config/network.json sd:/backup/network.json
cd sd-pl
ls 2:/
cp 2:/backup/input.bin 0:/backup/input.bin
```

`fs format sd-pl confirm` удаляет текущую файловую систему и все данные на карте,
после чего создаёт FAT32 через PL SD-контроллер. Без слова `confirm` команда не
выполняется.

## 4. Сеть

| Команда | Назначение |
|---|---|
| `ip addr show` | IP, prefix, netmask и gateway |
| `ip addr set <IPv4>/<prefix>` | установить IP и маску |
| `ip link show` | показать MAC |
| `ip link set address <mac>` | установить MAC |
| `ip route show` | показать default gateway |
| `ip route set default via <gw>` | установить gateway |
| `ip save` | сохранить текущий runtime netif |

Команды изменения сети применяют значения и записывают `flash:/config/network.json`.

## 5. I2C

| Команда | Назначение |
|---|---|
| `i2c list` | список устройств |
| `i2c <sel> info` | параметры устройства и policy |
| `i2c <sel> r <reg>` | чтение регистра |
| `i2c <sel> w <reg> <val>` | запись регистра |
| `i2c <sel> addr <addr_7b>` | изменить 7-битный адрес и сохранить |
| `i2c <sel> policy` | показать активную policy |
| `i2c <sel> policy show rules` | показать policy и оба списка |
| `i2c <sel> policy show whitelist` | показать whitelist |
| `i2c <sel> policy show blacklist` | показать blacklist |
| `i2c <sel> policy set whitelist` | выбрать whitelist |
| `i2c <sel> policy set blacklist` | выбрать blacklist |
| `i2c <sel> policy whitelist add <reg> <val>` | добавить пару |
| `i2c <sel> policy whitelist del <reg> <val>` | удалить пару |
| `i2c <sel> policy whitelist clear` | очистить список |
| `i2c <sel> policy blacklist add <reg> <val>` | добавить пару |
| `i2c <sel> policy blacklist del <reg> <val>` | удалить пару |
| `i2c <sel> policy blacklist clear` | очистить список |

`<sel>` — имя устройства, 7-битный адрес или селектор `@0xNN`. Policy проверяет пару `(reg, value)`.

## 6. SMI

| Команда | Назначение |
|---|---|
| `smi list` | список PHY |
| `smi r <phy> <reg>` | legacy-форма чтения |
| `smi w <phy> <reg> <data>` | legacy-форма записи |
| `smi <sel> info` | параметры PHY и policy |
| `smi <sel> r <reg>` | чтение регистра |
| `smi <sel> w <reg> <data>` | запись регистра |
| `smi <sel> phy <phy_addr>` | изменить PHY address |
| `smi <sel> policy <whitelist|blacklist>` | выбрать policy |
| `smi <sel> rules` | показать правила |
| `smi <sel> allow <reg>` | добавить разрешённый регистр |
| `smi <sel> deny <reg>` | добавить запрещённый регистр |
| `smi <sel> clear <reg>` | удалить правило |
| `smi <sel> autopoll` | показать autopoll |
| `smi <sel> autopoll on|off` | включить или выключить |
| `smi <sel> autopoll reg_delay <ms>` | задержка между регистрами |
| `smi <sel> autopoll cycle_delay <ms>` | задержка между циклами |
| `smi <sel> autopoll regs <r0> ...` | список регистров |
| `smi <sel> settings` | показать settings |
| `smi <sel> settings clear` | очистить settings |
| `smi <sel> save` | сохранить конфигурацию |

SMI policy проверяет номер регистра. Autopoll относится к SMI-конфигурации.

## 7. SPI

| Команда | Синтаксис |
|---|---|
| `spi info` | показать mode, timeout, divider и read |
| `spi cfg mode` | `spi cfg mode <single|multi|fallthrough>` |
| `spi cfg timeout` | `spi cfg timeout <ticks>` |
| `spi cfg div` | `spi cfg div <even>` в диапазоне `2..65535` |
| `spi cfg read` | `spi cfg read <on|off>` |
| `spi xfer` | `spi xfer <w0> [w1 ...]` |

`spi xfer` принимает 32-битные слова. Текущий shell ограничивает одну передачу 64 словами.

## 8. Память и архивы

| Команда | Синтаксис | Поведение |
|---|---|---|
| mem read | `mem r <addr>` | 32-битное чтение для выровненного адреса, 8-битное для невыровненного |
| mem write | `mem w <addr> <value>` | 32-битная запись для выровненного адреса; 8-битная при невыровненном адресе и значении до `0xFF` |
| tar create | `tar c <src_dir> <dst_tar>` | создать архив |
| tar extract | `tar x <src_tar> <dst_dir>` | распаковать архив |
| tar list | `tar t <src_tar>` | вывести содержимое |

MMIO и archive-команды предназначены для инженерной диагностики. Перед записью сверяйте адрес и значение с hardware contract.

## 9. Ответы

| Ответ | Значение |
|---|---|
| `OK` | операция выполнена |
| `ERR` | ошибка разбора или выполнения |
| `DENIED` | операция отклонена policy |
| `FS not ready` | выбранный том ещё не смонтирован |
| `I2C not ready` | I2C runtime ещё не завершил инициализацию |

Для автоматизации с формальными status-кодами используйте [HTTP API](http-api.md) или [DCP2](../dcp2.md).
