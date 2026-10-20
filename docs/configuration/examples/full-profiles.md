# Полные профили устройства

Ниже приведены целые файлы JSON без пропущенных разделов. В примерах I²C
устройство `device_a` имеет 16 регистров с адресами от `0x00` до `0x0F`,
в варианте MDIO — 32 регистра от `0x00` до `0x1F`. Название, адрес
устройства и значения условные; они не описывают конкретную микросхему.

У полностью заполненного профиля должны быть конкретные `bus`,
`controller` и `driver`: значения вроде `bus_a` текущий кодек не принимает.
Первые два файла используют обработчик I²C. В конце приведён полный
вариант для MDIO, чтобы показать зависимые от обработчика поля.
Подключение реального устройства требует его собственного описания.

## Минимальный профиль

Файл `device_a.json` задаёт подключение, обработчик, 16 ресурсов
и разрешённую текущей прошивкой политику чтения. Одинаковые свойства
регистров пока записаны непосредственно в каждом ресурсе:

```json
{
  "schema_version": 2,
  "id": "device_a",
  "device": {
    "model": "Условное устройство"
  },
  "transport": {
    "bus": "i2c",
    "controller": "pl_i2c_0",
    "address": "0x20"
  },
  "protocol": {
    "driver": "bvstk_i2c_bridge",
    "options": {
      "max_write_value": "0x40"
    }
  },
  "resources": [
    {
      "id": "reg_00", "kind": "register", "address": "0x00",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_01", "kind": "register", "address": "0x01",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_02", "kind": "register", "address": "0x02",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_03", "kind": "register", "address": "0x03",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_04", "kind": "register", "address": "0x04",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_05", "kind": "register", "address": "0x05",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_06", "kind": "register", "address": "0x06",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_07", "kind": "register", "address": "0x07",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_08", "kind": "register", "address": "0x08",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_09", "kind": "register", "address": "0x09",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_0a", "kind": "register", "address": "0x0A",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_0b", "kind": "register", "address": "0x0B",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_0c", "kind": "register", "address": "0x0C",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_0d", "kind": "register", "address": "0x0D",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_0e", "kind": "register", "address": "0x0E",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    },
    {
      "id": "reg_0f", "kind": "register", "address": "0x0F",
      "width_bits": 8, "access": "rw", "write_mask": "0xFF",
      "cache": {"mode": "driver"}, "verify": {"mode": "driver"}
    }
  ],
  "policy": {
    "read": {"mode": "blacklist", "rules": []}
  }
}
```

Поля `write` нет, поэтому при загрузке создаётся пустой белый список:
записи запрещены. Раздела `settings` тоже нет — профиль не задаёт
начальных значений кэша. Отсутствие `register_map` означает, что
расшифровка по карте для этого пакета не подключена.

`max_write_value` обязателен для выбранного обработчика даже при запрете
записи: это отдельное ограничение допустимого значения. Оно сохраняет
силу, если политику позднее изменят. Верхняя граница `0x40` относится
к записи; прочитанное значение восьмибитного регистра может быть больше.

Все 16 адресов перечислены явно: разрывы в диапазоне ресурсов текущий
кодек не допускает. Наличие регистра в `resources` ещё не разрешает
запись в него и не задаёт начальное значение кэша.

Класс устройства становится `generic`, разрядность адреса — семь бит.
Эти значения приняты кодеком, хотя в исходном JSON явно не указаны.
В [примере сохранения](defaults-and-save.md) показано, как весь этот файл
будет выглядеть после формирования JSON из модели.

## Профиль с политикой, начальным кэшем и отображением

Теперь разрешим выбранные значения в пяти регистрах, заполним начальный
кэш и подключим расшифровку с собственной формой. Для `reg_03` допустимы
два значения, поэтому ему соответствуют два правила. Полный перечень
из 16 ресурсов сохранится, а их одинаковые свойства перенесём
в `defaults.register`:

```json
{
  "schema_version": 2,
  "id": "device_a",
  "register_map": "register_map.json",
  "view": "view.js",
  "device": {
    "model": "Условное устройство",
    "class": "generic",
    "label": "Устройство A"
  },
  "transport": {
    "bus": "i2c",
    "controller": "pl_i2c_0",
    "address": "0x20",
    "address_bits": 7
  },
  "protocol": {
    "driver": "bvstk_i2c_bridge",
    "options": {
      "max_write_value": "0x40",
      "capture_cache_on_save": true
    }
  },
  "defaults": {
    "register": {
      "width_bits": 8,
      "access": "rw",
      "write_mask": "0xFF",
      "cache": {"mode": "driver"},
      "verify": {"mode": "driver"}
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
    {"id": "reg_0f", "kind": "register", "address": "0x0F"}
  ],
  "policy": {
    "read": {"mode": "blacklist", "rules": []},
    "write": {
      "mode": "whitelist",
      "rules": [
        {
          "resource": "reg_01",
          "match": {"value": "0x01", "mask": "0xFF"}
        },
        {
          "resource": "reg_03",
          "match": {"value": "0x10", "mask": "0xFF"}
        },
        {
          "resource": "reg_03",
          "match": {"value": "0x20", "mask": "0xFF"}
        },
        {
          "resource": "reg_07",
          "match": {"value": "0x04", "mask": "0xFF"}
        },
        {
          "resource": "reg_0a",
          "match": {"value": "0x2A", "mask": "0xFF"}
        },
        {
          "resource": "reg_0f",
          "match": {"value": "0x01", "mask": "0xFF"}
        }
      ],
      "inactive_rules": [
        {
          "resource": "reg_03",
          "match": {"value": "0x00", "mask": "0xFF"}
        },
        {
          "resource": "reg_0f",
          "match": {"value": "0x00", "mask": "0xFF"}
        }
      ]
    }
  },
  "settings": {
    "desired": [],
    "cache_seed": [
      {"resource": "reg_01", "value": "0x00"},
      {"resource": "reg_03", "value": "0x10"},
      {"resource": "reg_07", "value": "0x00"},
      {"resource": "reg_0a", "value": "0x08"},
      {"resource": "reg_0e", "value": "0xA5"}
    ],
    "apply_on_startup": false
  },
  "observability": {
    "telemetry": true,
    "events": true
  }
}
```

В `rules` шесть записей для пяти регистров. В `reg_03` можно записать
`0x10` или `0x20`; другое значение проверку не пройдёт. Для регистров,
которых в белом списке нет, запись запрещена. Непустой `inactive_rules`
содержит два правила чёрного списка, которые сейчас не участвуют
в допуске. При переключении режима через интерфейс управления списки
поменяются ролями: тогда запретятся именно пары `reg_03 = 0x00`
и `reg_0f = 0x00`, а остальные записи будут допущены политикой.
Ограничения обработчика продолжат действовать.

Набор `cache_seed` выбирается отдельно. Например, `reg_0e` имеет начальное
значение `0xA5`, хотя ни одного разрешающего правила для него нет.
Начальное заполнение кэша не является записью в устройство и не проходит
проверку политики. Значение также может превышать предел физической записи
`max_write_value`, оставаясь в разрядности регистра. В этом профиле
начальные значения заданы для пяти адресов; остальные ресурсы перечислены
без них.

Для `reg_01` сохранено `0x00`, а разрешена запись `0x01`: первое используется
при создании кэша, второе — при запросе записи. Эта связь отдельно
разобрана на [схеме ресурса и правила](profile-and-policy.md).

Ссылки `register_map` и `view` добавлены автором. Для отображения
рядом с профилем должны находиться соответствующие файлы:
они не создаются из этих строк автоматически. Порядок их подключения
описан в [сборке пакета](package-assembly.md).

Некоторые поля здесь записаны явно, хотя их можно было опустить:
`device.class`, `address_bits`, `capture_cache_on_save`, `desired`,
`apply_on_startup` и `observability`. Именно такие значения кодек указывает
при сохранении. Непустые списки правил и начальных значений, напротив,
требуют явного задания.

Флаги `observability` сейчас принимаются только со значением `true`.
Указать `false` для отключения наблюдений через профиль нельзя:
такой документ будет отклонён.

`capture_cache_on_save` сейчас принимается только со значением `true`.
Фактический состав сохраняемого кэша зависит от операции сохранения;
различия перечислены в [главе загрузки и сохранения](../lifecycle.md#сохранение-и-ошибки).
`desired` и `apply_on_startup` пока зарезервированы: этот профиль
не выполняет физическую запись `0x00` при запуске.

## Вариант с параметрами опроса

Ниже самостоятельный профиль условного устройства для обработчика MDIO.
Он описывает 32 регистра, но опрашивает только пять из них. Политика
содержит четыре правила, начальные значения заданы для четырёх адресов.
Карта и форма не подключены. Вместо параметров I²C появились настройки
опроса в `protocol.options`:

```json
{
  "schema_version": 2,
  "id": "device_a",
  "device": {
    "model": "Условное устройство",
    "class": "generic"
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
      "autopoll_order": [
        "reg_01",
        "reg_11",
        "reg_05",
        "reg_0c",
        "reg_1f"
      ],
      "autopoll_reg_delay_ms": 2,
      "autopoll_cycle_delay_ms": 50
    }
  },
  "defaults": {
    "register": {
      "width_bits": 16,
      "access": "rw",
      "write_mask": "0xFFFF",
      "cache": {"mode": "driver"},
      "verify": {"mode": "driver"}
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
    "read": {"mode": "blacklist", "rules": []},
    "write": {
      "mode": "whitelist",
      "rules": [
        {
          "resource": "reg_01",
          "match": {"value": "0x0001", "mask": "0xFFFF"}
        },
        {
          "resource": "reg_04",
          "match": {"value": "0x0005", "mask": "0x000F"}
        },
        {"resource": "reg_0c"},
        {
          "resource": "reg_10",
          "match": {"value": "0x0100", "mask": "0x0300"}
        }
      ],
      "inactive_rules": []
    }
  },
  "settings": {
    "cache_seed": [
      {"resource": "reg_01", "value": "0x0000"},
      {"resource": "reg_05", "value": "0x0040"},
      {"resource": "reg_11", "value": "0x1234"},
      {"resource": "reg_1f", "value": "0x0000"}
    ]
  }
}
```

`autopoll_order` использует те же ID, что политика и начальные значения.
Здесь порядок обхода задан явно: `reg_01`, затем `reg_11`, `reg_05`,
`reg_0c` и `reg_1f`. Он отличается от порядка адресов в `resources`.
Между регистрами добавляется пауза 2 мс, после обхода списка — 50 мс.
Наличие остальных адресов в профиле само по себе не добавляет их в опрос.
При отключении `autopoll_enabled` список и интервалы можно оставить
для последующего включения.

Правила демонстрируют разные условия: для `reg_01` сравнивается всё слово,
для `reg_04` — младшие четыре бита, для `reg_10` — биты 8 и 9.
У `reg_0c` отсутствует `match`, поэтому правило разрешает любое допустимое
значение этого регистра. Часть разрешённых для записи регистров
не входит в опрос, а часть опрашиваемых не имеет начального значения.
Каждый список задаёт свою операцию над общим набором ресурсов.

Если весь `protocol.options` в профиле MDIO опущен, опрос выключен,
его список пуст, пауза между регистрами равна нулю, а между циклами —
50 мс. При сохранении эти параметры появятся явно. В данном примере
автор добавил `options`, потому что требуется включить конкретный опрос.

Замены одного имени `bus` недостаточно. Согласованы контроллер,
обработчик, разрядность ресурса и маски. Поля `address_bits`,
`max_write_value` и `capture_cache_on_save` в этом варианте отсутствуют,
потому что обработчик MDIO их не принимает. Допустимые сочетания
перечислены в [справочнике обработчиков](../reference/device-profile-v2.md#поддержка-в-текущей-прошивке).

Эти файлы — отдельные варианты одного примера. В рабочем пакете
выбирают один профиль. Поля, создаваемые при работе системы,
такие как `revision`, `saved` и `cache[].valid`, ни в один из них
не добавляют: их происхождение показано
в [главе о рабочем состоянии](../lifecycle.md#поля-рабочего-состояния-и-ответа).
