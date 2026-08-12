# TCP-консоль

## 1. Подключение

TCP-консоль FreeRTOS слушает порт `8888` и предоставляет строковый command dispatcher.

```sh
telnet <device-ip> 8888
```

```sh
nc <device-ip> 8888
```

SSH на порту `22`, если включён в сборке, использует тот же dispatcher. Отличия SSH-терминала описаны в [документе SSH](../dev/freertos-ssh.md).

## 2. Модель сессии

| Свойство | Поведение |
|---|---|
| ввод | одна команда на строку, выполнение после `Enter` |
| регистр | имена команд сравниваются без учёта регистра |
| prompt | показывает выбранный том и текущий каталог |
| история | поддерживает переходы `Up`/`Down` |
| редактирование | backspace, стрелки и базовое перемещение курсора |
| completion | команды, подкоманды, устройства и пути для поддержанных поверхностей |
| завершение | `quit` или `exit` закрывают TCP-сессию |

После смены IP активное соединение может завершиться; переподключение выполняется по новому адресу.

## 3. Верхнеуровневые команды

| Команда | Назначение |
|---|---|
| `help` | список команд |
| `pwd`, `ls`, `cd` | файловая навигация |
| `mkdir`, `touch`, `cat`, `rm`, `cp`, `mv` | файловые операции |
| `ip` | адрес, маршрут и MAC |
| `i2c` | I2C устройства, регистры и policy |
| `smi` | SMI/MDIO устройства, policy и autopoll |
| `spi` | SPI runtime configuration и transfer |
| `mem` | диагностическое чтение/запись памяти и MMIO |
| `tar` | архивирование и распаковка |
| `reboot` | перезапуск с явным подтверждением |
| `fs` | справка по файловому слою |

Проверка после подключения:

```text
help
ip addr show
pwd
ls
```

## 4. Файловые команды

Файловые операции вызываются непосредственно:

```text
cd flash
ls /config
cat /config/network.json
cp flash:/config/network.json sd:/backup/network.json
```

| Команда | Формат |
|---|---|
| `pwd` | текущий каталог |
| `ls [path]` | содержимое |
| `cd <path>` | переход |
| `mkdir <path>` | создание каталога |
| `touch <path>` | создание файла |
| `cat <path>` | вывод содержимого |
| `rm <path>` | удаление |
| `rm -r <path>` | рекурсивное удаление |
| `cp <src> <dst>` | копирование файла |
| `cp -r <src> <dst>` | копирование каталога |
| `mv <src> <dst>` | перемещение или переименование |

Том можно выбрать явным префиксом `sd:/` или `flash:/`, либо командой `cd sd`/`cd flash`.

## 5. Сеть

```text
ip addr show
ip link show
ip route show
ip addr set 192.168.0.20/24
ip route set default via 192.168.0.1
ip link set address 00:0a:35:00:01:02
ip save
```

Команды `set` применяют значения к живому `netif` и сохраняют сетевой JSON. `ip save` фиксирует текущее runtime-состояние. Формат и HTTP-эквивалент приведены в [сети](network.md).

## 6. I2C

Выбор устройства выполняется по имени, адресу или селектору `@0xNN`:

```text
i2c list
i2c axp15060 info
i2c axp15060 r 0x10
i2c axp15060 w 0x10 0x01
```

Актуальный синтаксис policy:

```text
i2c axp15060 policy
i2c axp15060 policy show rules
i2c axp15060 policy show whitelist
i2c axp15060 policy set whitelist
i2c axp15060 policy whitelist add 0x10 0x01
i2c axp15060 policy whitelist del 0x10 0x01
i2c axp15060 policy whitelist clear
```

`whitelist` и `blacklist` содержат пары `(register, value)`. Изменения policy и списков сохраняются в `flash:/config/i2c/<device>.json` после успешной операции.

## 7. SMI и SPI

SMI выбирает PHY и поддерживает операции чтения/записи, policy, settings и настройки autopoll:

```text
smi list
smi lan8720 info
smi lan8720 r 0x01
smi lan8720 w 0x00 0x1200
smi lan8720 autopoll
```

SPI предоставляет runtime-команды, доступные через `spi -h`; точный синтаксис зависит от текущего shell-модуля и его конфигурации. Общая регистровая модель приведена в [SPI](../dev/pl/spi.md).

## 8. Перезапуск

`reboot` требует явного подтверждения:

```text
reboot -y 1000
```

Поддерживается форма `reboot confirm [delay_ms]`. Без подтверждения операция завершается сообщением об ошибке.

## 9. Ответы и автоматизация

Текстовые ответы используют формы `OK`, `ERR`, `DENIED`, `FS not ready` и диагностические сообщения. TCP-консоль подходит для ручных процедур и простых smoke-тестов. Для строгого machine-to-machine контракта используйте HTTP JSON или DCP2.

## 10. Диагностика

| Симптом | Действие |
|---|---|
| порт `8888` закрыт | проверить IP, LAN startup и bitstream |
| после смены IP сессия исчезла | подключиться по новому адресу |
| `FS not ready` | дождаться mount-задач |
| `I2C not ready` | проверить `config_store` и JSON устройств |
| policy list пуст | проверить имя устройства и `policy show rules` |
| `DENIED by policy` | проверить активную policy и пару `(reg, value)` |
| SMI command unavailable | проверить готовность общего SMI service и конфигурацию |
