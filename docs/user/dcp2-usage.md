# Практика работы с DCP2

## 1. Подключение

DCP2 работает на TCP-порту `8889`.

```sh
nc -vz <device-ip> 8889
```

Формат кадров, service IDs и status codes описаны в [спецификации DCP2](../dcp2.md). Этот документ посвящён запуску и проверке.

## 2. Возможности профилей

| Операция | FreeRTOS server | Neutrino `bvstkd` |
|---|---|---|
| PING | да | да |
| MEM | да, 8/16/32/64-bit и allowlist | да, общий PL region control |
| I2C read/write/policy | да | да |
| SMI read/write | да | не включено в текущий Neutrino I²C-профиль |
| SPI transfer | `ERR_UNSUPPORTED` в обычном request path | не включено в текущий профиль |
| NOTIFY subscribe | да | профиль не подключён |
| PL stream control | control state | профиль не подключён |

## 3. Мониторинг notify

Reference client подписывается на NOTIFY и печатает события:

```sh
./scripts/dcp2/monitor_notify.py <device-ip> --port 8889
```

Фильтры:

```sh
./scripts/dcp2/monitor_notify.py <device-ip> --buses i2c
./scripts/dcp2/monitor_notify.py <device-ip> --classes attempt,commit,denied
./scripts/dcp2/monitor_notify.py <device-ip> --sources telnet,host
./scripts/dcp2/monitor_notify.py <device-ip> --no-timestamp
```

| Опция | Значение по умолчанию |
|---|---|
| `--port` | `8889` |
| `--classes` | `all` |
| `--sources` | `telnet,host,dcp,internal` |
| `--buses` | `i2c` |
| `--timeout` | `5.0` секунд |
| `--snapshot` | выключен |
| `--no-timestamp` | выключен |

## 4. Smoke-тест I2C notify

Терминал 1:

```sh
./scripts/dcp2/monitor_notify.py 192.168.0.10 --buses i2c
```

Терминал 2:

```sh
nc 192.168.0.10 8888
```

В shell выполнить:

```text
i2c axp15060 policy show rules
i2c axp15060 w 0x13 0x10
```

В зависимости от policy и результата hardware monitor печатает `REG_ATTEMPT`, `REG_COMMIT`, `REG_DENIED` или `FAULT`.

## 5. Диагностика PING

`monitor_notify.py` автоматически отправляет PING перед подпиской. Ошибка PING указывает на TCP, version или frame parsing problem.

| Симптом | Проверка |
|---|---|
| соединение закрывается сразу | magic, version `0x0002`, payload length |
| `ERR_UNSUPPORTED` | service/opcode в профиле конкретной ОС |
| `ERR_BUSY` | readiness config store и runtime service |
| `ERR_RANGE` | адрес устройства, регистр, count или MMIO range |
| `ERR_DENIED` | policy устройства |
| notify отсутствует | активна ли подписка, подходит ли mask, создаёт ли операция событие |

## 6. Подписка на события

NOTIFY SUBSCRIBE body состоит из `class_mask:u32`, `source_mask:u32`, `bus_mask:u32`, `flags:u8`.

Практические имена reference client:

| Фильтр | Значения |
|---|---|
| classes | `attempt`, `commit`, `denied`, `state`, `fault`, `all` |
| sources | `telnet`, `host`, `dcp`, `internal` |
| buses | `i2c`, `smi`, `spi`, `uart`, `sys` |

События передаются тем же TCP-соединением, что и response. Host-клиент обязан отличать event bit от response bit и сопоставлять response по sequence.

## 7. Повторные запросы

| Status | Действие |
|---|---|
| `OK` | использовать response body |
| `ERR_BUSY` | повторить после короткой задержки с лимитом |
| `ERR_TIMEOUT` | повторить после проверки PL и timeout |
| `ERR_DENIED` | пересмотреть policy |
| `ERR_RANGE` | исправить параметр |
| `ERR_UNSUPPORTED` | выбрать другой профиль или opcode |

## 8. Связанные материалы

| Документ | Содержание |
|---|---|
| [DCP2 specification](../dcp2.md) | wire format и body layouts |
| [HTTP API](../reference/http-api.md) | альтернативный JSON интерфейс |
| [I2C](../dev/pl/i2c.md) | service и policy |
| [Коды состояний](../reference/status-codes.md) | status mapping |
