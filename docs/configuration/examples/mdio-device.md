# Профиль MDIO

Структура профиля сохраняется: экземпляр, подключение, ресурсы,
политика и начальные значения. Меняются обработчик и параметры
обслуживания шины.

Условное устройство `device_m` имеет адрес `0x01` и 32 регистра
по 16 бит. В отличие от I²C, адрес регистра входит в кадр MDIO,
поэтому объекта `transfer` здесь нет. Пакет размещён в
`config/mdio/device_m/device_m.json`.

## Полный профиль

```json
{
  "schema_version": 2,
  "id": "device_m",
  "device": {
    "model": "Generic 32-register device",
    "class": "generic",
    "label": "device_m"
  },
  "transport": {
    "bus": "mdio",
    "controller": "pl_mdio_0",
    "address": "0x01"
  },
  "protocol": {
    "driver": "bvstk_mdio_bridge",
    "options": {
      "autopoll_enabled": true,
      "autopoll_reg_delay_ms": 2,
      "autopoll_cycle_delay_ms": 50,
      "autopoll_order": [
        "reg_01",
        "reg_05",
        "reg_11",
        "reg_1f"
      ]
    }
  },
  "defaults": {
    "register": {
      "width_bits": 16,
      "access": "rw",
      "write_mask": "0xFFFF",
      "cache": {
        "mode": "driver"
      },
      "verify": {
        "mode": "driver"
      }
    }
  },
  "resources": [
    {"id": "reg_00", "kind": "register", "address": "0x00"},
    {"id": "reg_01", "kind": "register", "address": "0x01"},
    {"id": "reg_02", "kind": "register", "address": "0x02"},
    {"id": "reg_03", "kind": "register", "address": "0x03"},
    {"id": "reg_04", "kind": "register", "address": "0x04"},
    {"id": "reg_05", "kind": "register", "address": "0x05"},
    {"id": "reg_06", "kind": "register", "address": "0x06"},
    {"id": "reg_07", "kind": "register", "address": "0x07"},
    {"id": "reg_08", "kind": "register", "address": "0x08"},
    {"id": "reg_09", "kind": "register", "address": "0x09"},
    {"id": "reg_0a", "kind": "register", "address": "0x0A"},
    {"id": "reg_0b", "kind": "register", "address": "0x0B"},
    {"id": "reg_0c", "kind": "register", "address": "0x0C"},
    {"id": "reg_0d", "kind": "register", "address": "0x0D"},
    {"id": "reg_0e", "kind": "register", "address": "0x0E"},
    {"id": "reg_0f", "kind": "register", "address": "0x0F"},
    {"id": "reg_10", "kind": "register", "address": "0x10"},
    {"id": "reg_11", "kind": "register", "address": "0x11"},
    {"id": "reg_12", "kind": "register", "address": "0x12"},
    {"id": "reg_13", "kind": "register", "address": "0x13"},
    {"id": "reg_14", "kind": "register", "address": "0x14"},
    {"id": "reg_15", "kind": "register", "address": "0x15"},
    {"id": "reg_16", "kind": "register", "address": "0x16"},
    {"id": "reg_17", "kind": "register", "address": "0x17"},
    {"id": "reg_18", "kind": "register", "address": "0x18"},
    {"id": "reg_19", "kind": "register", "address": "0x19"},
    {"id": "reg_1a", "kind": "register", "address": "0x1A"},
    {"id": "reg_1b", "kind": "register", "address": "0x1B"},
    {"id": "reg_1c", "kind": "register", "address": "0x1C"},
    {"id": "reg_1d", "kind": "register", "address": "0x1D"},
    {"id": "reg_1e", "kind": "register", "address": "0x1E"},
    {"id": "reg_1f", "kind": "register", "address": "0x1F"}
  ],
  "policy": {
    "read": {
      "mode": "blacklist",
      "rules": []
    },
    "write": {
      "mode": "whitelist",
      "rules": [
        {
          "resource": "reg_03"
        },
        {
          "resource": "reg_07",
          "match": {
            "value": "0x0005",
            "mask": "0x000F"
          }
        }
      ],
      "inactive_rules": []
    }
  },
  "settings": {
    "cache_seed": []
  }
}
```

## Опрос и правила

После включения опроса сервис последовательно читает четыре ресурса,
указанных в `autopoll_order`. Между ними выдерживается задержка
2 мс, между проходами — 50 мс. Наличие остальных ресурсов в профиле
позволяет обращаться к ним отдельными запросами; оно не добавляет
их в периодический опрос.

Первое правило разрешает для `reg_03` любое 16-битное значение:
у него нет ограничения `match`. Второе разрешает для `reg_07`
только значения, у которых младшие четыре бита равны `0x5`.
Например, `0x0015` подходит, `0x0016` — нет.
Маска правила сравнивает значение и не выполняет чтение с последующей
записью отдельных битов.

Профиль применяется загрузчиком MDIO во FreeRTOS.
В текущей рабочей среде Нейтрино подключён загрузчик I²C;
наличие общего кодека MDIO не создаёт службу MDIO автоматически.

## Параметры обработчика

В этом профиле сочетаются `bus: "mdio"`, `controller: "pl_mdio_0"`
и `driver: "bvstk_mdio_bridge"`. Адрес PHY лежит от `0x00` до `0x1F`.
`address_bits`, `parent`, `channel` и `address_spaces` не принимаются,
как и `protocol.transfer` с `protocol.switch`.

Регистров может быть от 1 до 32, адреса начинаются с нуля и доходят
до `0x1F`. Разрядность всегда 16 бит, `write_mask` равна `0xFFFF`,
а `access` принимает только `rw`. В карте регистров используются
те же `bus`, разрядность и число ресурсов.

В `options` разрешены только параметры опроса; объект можно целиком
опустить. При отсутствии отдельных полей действуют значения:
`autopoll_enabled: false`, `autopoll_order: []`,
`autopoll_reg_delay_ms: 0`, `autopoll_cycle_delay_ms: 50`.
Пауза между регистрами допускает от 0 до 60000 мс,
между циклами — от 0 до 600000 мс. Порядок опроса содержит
до 32 существующих ресурсов без повторов.

В каждом из списков политики помещается до 64 правил. `match` можно
опустить или оставить пустым: правило будет относиться ко всему
диапазону значений ресурса. Если сравнение задано, нужны одновременно
`value` и `mask`; оба лежат от `0x0000` до `0xFFFF`.
В `cache_seed` помещается до 32 записей с 16-битными значениями.
Кэш обслуживается сервисом MDIO и связан с BRAM; настройки
I²C-передачи к этому пути не применяются.

Далее: [общая структура профиля](../device-profile.md).
