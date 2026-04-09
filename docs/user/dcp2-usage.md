# DCP2 (порт 8889)

Практическое руководство по использованию DCP2, сервиса NOTIFY и клиентского скрипта monitor_notify.py.


### Обзор DCP2

DCP2 — бинарный протокол внешнего клиента поверх TCP. Он дополняет TCP‑консоль и HTTP‑API, но решает другую задачу: компактный request/response обмен и асинхронные `event` в рамках одного постоянного соединения.

Параметры сервиса:
- транспорт: TCP
- порт: `8889`
- версия протокола: `0x0002`
- нормативное описание: [dcp2.md](../dcp2.md)

В текущей прошивке реализованы сервисы:
- `PING`
- `MEM`
- `I2C`
- `SMI`
- `NOTIFY`

DCP2 сервер стартует отдельно от telnet‑подобной TCP‑консоли. Типичный порядок работы клиента:
1. открыть TCP‑соединение с устройством на `8889`
2. отправить `PING`
3. при необходимости отправить `NOTIFY_SUBSCRIBE`
4. принимать обычные `response` и асинхронные `event` по тому же сокету

### NOTIFY

`NOTIFY` — сервис асинхронных уведомлений о событиях управления и изменения состояния. Это не потоковая телеметрия PL, а отдельный канал для control/state событий.

Через `NOTIFY_EVENT` клиент может получать:
- попытки записи в регистры
- успешные записи
- denied/error события
- уведомления от разных источников: `TELNET`, `HOST`, `DCP`, `INTERNAL`

Это позволяет наблюдать по DCP2 не только действия самого DCP‑клиента, но и внешние транзакции, например команды shell:
```text
i2c axp15060 w 0x13 0x12
```

Подписка на события фильтруется через:
- `class_mask`
- `source_mask`
- `bus_mask`
- `flags`

### Скрипт monitor_notify.py

Для быстрой работы с `NOTIFY` в репозиторий добавлен клиентский скрипт [monitor_notify.py](../../scripts/dcp2/monitor_notify.py).

Базовый запуск:
```sh
./scripts/dcp2/monitor_notify.py 192.168.0.10 --port 8889
```

Только I2C‑события:
```sh
./scripts/dcp2/monitor_notify.py 192.168.0.10 --port 8889 --buses i2c
```

Только события от telnet и host:
```sh
./scripts/dcp2/monitor_notify.py 192.168.0.10 --port 8889 --sources telnet,host
```

Без `time_us` в `NOTIFY_EVENT`:
```sh
./scripts/dcp2/monitor_notify.py 192.168.0.10 --port 8889 --no-timestamp
```

Скрипт:
- подключается к DCP2 серверу
- выполняет `PING`
- делает `NOTIFY_SUBSCRIBE`
- печатает входящие `event` в человекочитаемом виде

### Быстрая проверка DCP2/NOTIFY

Минимальный smoke‑test после сборки и прошивки:

1. Собрать и загрузить ELF:
```sh
./scripts/vitis/build.sh
./scripts/vitis/run_jtag.sh
```

2. Проверить, что устройство доступно по telnet:
```sh
telnet <device-ip> 8888
```

3. В отдельном терминале запустить монитор DCP2:
```sh
./scripts/dcp2/monitor_notify.py <device-ip> --port 8889 --buses i2c
```

4. В shell выполнить запись в регистр:
```text
i2c axp15060 w 0x13 0x12
```

5. Убедиться, что:
- shell вернул `OK...` или `ERR...`
- DCP2‑клиент получил `NOTIFY_EVENT` с `bus=I2C`
