# Коды состояний

## 1. Общий статус C API

`bvstk_status_t` используется общими cores, services и control API.

| Enum | Смысл |
|---|---|
| `BVSTK_OK` | операция завершена |
| `BVSTK_ERR_MALFORMED` | входные данные имеют неверный формат |
| `BVSTK_ERR_UNSUPPORTED` | операция или параметр не поддерживается |
| `BVSTK_ERR_DENIED` | запись отклонена policy |
| `BVSTK_ERR_BUSY` | ресурс временно занят |
| `BVSTK_ERR_TIMEOUT` | операция превысила timeout |
| `BVSTK_ERR_RANGE` | адрес, индекс или значение вне диапазона |
| `BVSTK_ERR_NOT_READY` | runtime или storage ещё не готов |
| `BVSTK_ERR_NOT_FOUND` | устройство, регион или файл не найден |
| `BVSTK_ERR_IO` | ошибка MMIO, файлового или транспортного ввода-вывода |
| `BVSTK_ERR_INTERNAL` | внутренняя ошибка |

## 2. DCP2

DCP2 кодирует результат в двухбайтовом поле status после service/opcode/sequence.

| Код | Имя | Источник |
|---:|---|---|
| `0x0000` | `OK` | `BVSTK_OK` |
| `0x0001` | `ERR_MALFORMED` | `BVSTK_ERR_MALFORMED` |
| `0x0002` | `ERR_UNSUPPORTED` | `BVSTK_ERR_UNSUPPORTED` |
| `0x0003` | `ERR_DENIED` | `BVSTK_ERR_DENIED` |
| `0x0004` | `ERR_BUSY` | `BVSTK_ERR_BUSY` |
| `0x0005` | `ERR_TIMEOUT` | `BVSTK_ERR_TIMEOUT` |
| `0x0006` | `ERR_RANGE` | `BVSTK_ERR_RANGE` |
| `0x0007` | `ERR_INTERNAL` | `NOT_READY`, `NOT_FOUND`, `IO`, `INTERNAL` и прочие |

Правила повторов:

| Статус | Рекомендация |
|---|---|
| `ERR_BUSY`, `ERR_TIMEOUT` | повторить с ограничением количества попыток |
| `ERR_MALFORMED`, `ERR_RANGE` | исправить формат или параметр |
| `ERR_DENIED` | проверить policy и полномочия операции |
| `ERR_UNSUPPORTED` | выбрать поддержанный service/opcode |
| `ERR_INTERNAL` | собрать runtime log и проверить platform contract |

## 3. HTTP

| HTTP status | Типичная причина |
|---:|---|
| `200` | успешный JSON/file/reboot response |
| `400` | malformed body, диапазон, отсутствует `confirm` |
| `403` | policy отказала записи |
| `404` | неизвестный маршрут, device или file |
| `405` | неподдерживаемый HTTP method |
| `411` | отсутствует `Content-Length` или chunked body |
| `500` | runtime, MMIO или файловая ошибка |
| `503` | сервис ожидает `config_store` или volume readiness |

## 4. Shell

Shell использует текстовые маркеры:

| Маркер | Интерпретация |
|---|---|
| `OK` | операция выполнена |
| `ERR` | ошибка разбора или выполнения |
| `ERR DENIED` / `DENIED` | policy отказала записи |
| `FS not ready` | том ещё не смонтирован |
| `I2C not ready` | I2C runtime ещё не завершил инициализацию |
| `WARN` | основная операция выполнена, сохранение или вспомогательный шаг завершился с предупреждением |

Текстовый shell предназначен для человека. Для автоматизированного клиента рекомендуется проверять DCP2 status или HTTP status вместе с JSON-полем `ok`/`saved`.
