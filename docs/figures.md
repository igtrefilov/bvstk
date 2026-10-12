# Исходники рисунков

Рисунки статьи о BVSTK хранятся в `system/figures/` и включены
в главы, начиная с [system.md](system/system.md).
Остальные руководства используют пять SVG из DOT и Mermaid.
Их исходники расположены рядом с изображениями.

| Рисунок | Исходник | Глава |
|---|---|---|
| Происхождение ELF/IFS | [artifacts.dot](build/figures/artifacts.dot) | [Сборка](build/README.md) |
| Граница PS/PL | [ps-pl.dot](reference/figures/ps-pl.dot) | [Плата](reference/board.md) |
| Зависимости исходников | [dependencies.dot](development/figures/dependencies.dot) | [Карта исходников](development/source-map.md) |
| Ведомый IRQ двух ОС | [i2c-irq.dot](development/figures/i2c-irq.dot) | [I²C](development/drivers/i2c.md) |
| Смена IP до ответа | [network-change.mmd](operations/figures/network-change.mmd) | [Сеть](operations/network.md) |

## Воспроизведение

Из корня репозитория, например:

```sh
dot -Tsvg docs/build/figures/artifacts.dot \
  -o docs/build/figures/artifacts.svg
mmdc -i docs/operations/figures/network-change.mmd \
  -o docs/operations/figures/network-change.svg \
  -c docs/operations/figures/mermaid.json -b white
```

Для остальных DOT используется та же команда с соответствующими путями.
Проверенные средства при подготовке комплекта: Graphviz 2.43.0,
Mermaid CLI 11.12.0, шрифт DejaVu Sans. Mermaid CLI требует доступный
Chromium; путь к браузеру при нестандартной установке задают локальной
конфигурацией Puppeteer, не путём конкретного компьютера в репозитории.

Проверьте после экспорта подписи, переносы, пересечения стрелок и масштаб
основной колонки. В SVG должен оставаться текст, а не растровая копия
всей схемы. Временные прототипы в комплект опубликованных рисунков не
входят.

## Схемы архитектуры

Обзоры и подробные схемы хранятся в `system/figures/architecture/`.
Эти SVG редактируются непосредственно. Ссылки в названиях блоков
связывают соответствующие части разных рисунков.

| Схема | Содержание |
|---|---|
| [Уровни абстракции](system/figures/architecture/levels.svg) | Принятое деление системы на L0–L4 |
| [BVSTK в двух ОС](system/figures/architecture/architecture_bvstk_os.svg) | Части FreeRTOS и Нейтрино, общий код |
| [L4. Приложения](system/figures/architecture/architecture_bvstk_l4.svg) | Запуск, настройки и пользовательские интерфейсы |
| [L3. Сервисы и драйверы](system/figures/architecture/architecture_bvstk_l3.svg) | Обработка запросов и работа подсистем |
| [L1. Платформа](system/figures/architecture/architecture_bvstk_l1.svg) | Общие интерфейсы, порты ОС и обращение к ядру L2 |
| [L0–L1. Аппаратные пути](system/figures/architecture/architecture_bvstk_l0_l1.svg) | Доступ к оборудованию и обработка прерываний |
| [L0. Взаимодействие PS и PL](system/figures/architecture/architecture_bvstk_ps_pl.svg) | AXI, буферы, DMA и источники прерываний |

Состав включённых функций каждой сборки приведён в
[справочнике профилей](reference/profiles.md).

### Подсистемы PL

Глава [о подсистемах PL](system/pl-subsystem/pl-subsystem.md)
раскрывает устройство PL и взаимодействие с программой PS.
Рисунки включены в текст соответствующих разделов статьи.
В комплекте 52 рисунка:

| Раздел | Содержание |
|---|---|
| [BVSTK как мост](system/figures/architecture/pl-subsystem/overview/bridge.svg) | Два участка шины, промежуточная BRAM, передача или остановка транзакции по решению политики |
| [Кэш и подготовка ответов](system/figures/architecture/pl-subsystem/overview/cache.svg) | Обновление значений, обработка записи, достоверность и публикация в BRAM |
| [Состав подсистемы](system/figures/architecture/pl-subsystem/overview/subsystems.svg) | Ответственность сервиса, драйвера, платформы и аппаратных ядер |
| [Порядок операций](system/figures/architecture/pl-subsystem/overview/exchange.svg) | Готовый ответ, получение нового значения и обработка записи |
| [Области обмена](system/figures/architecture/pl-subsystem/overview/memory.svg) | Кто записывает и читает данные, как определяется готовность |
| [I²C](system/figures/architecture/pl-subsystem/i2c/protocol/interface.svg) | Интерфейс, формат транзакции, чтение, запись и повторный START |
| [Ведомое ядро I²C](system/figures/architecture/pl-subsystem/i2c/cores/slave/structure.svg) | Состав ядра, обработка адреса, запись, чтение, повторный START, окна BRAM и завершение |
| [Ведущее ядро I²C](system/figures/architecture/pl-subsystem/i2c/cores/master/structure.svg) | Команды TX FIFO, обмен на шине, приёмные буферы, тактирование и завершение |
| [Протокол MDIO](system/figures/architecture/pl-subsystem/mdio/protocol/interface.svg) | Линии, кадры Clause 22, смена передатчика и временная диаграмма ответа |
| [MDIO](system/figures/architecture/pl-subsystem/mdio/subsystem.svg) | Кадр Clause 22, последние записи и подготовленные ответы |
| [Протокол SPI](system/figures/architecture/pl-subsystem/spi/protocol/interface.svg) | Линии, одновременная передача, режимы CPOL и CPHA, чтение и запись |
| [SPI и SD](system/figures/architecture/pl-subsystem/spi/subsystem.svg) | Потоки, DMA, оба ядра SPI и чтение блока SD |
| [Карта памяти реализации](system/figures/architecture/pl-subsystem/reference/memory-map.svg) | Адреса BRAM и области обмена конкретных подсистем |
| [Программа PS](system/figures/architecture/pl-subsystem/ps-interaction/startup.svg) | Профили, IRQ, политика, кэш, команды и сохранение настроек |

Цвета слоёв и типы стрелок совпадают с общим архитектурным рядом.
Ссылки в заголовках ведут к соответствующим блокам соседних листов.
Последовательности операций дополняют схемы состава.
Схемы в `overview` описывают общий случай без привязки к шине.
В `i2c/protocol` собрана вводная часть о протоколе с 7-битной адресацией.
Она основана на [NXP UM10204](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
и [TI SLVA704](https://www.ti.com/lit/an/slva704/slva704.pdf).
Аналогичные вводные главы о [SPI](system/pl-subsystem/spi/protocol/protocol.md)
и [MDIO](system/pl-subsystem/mdio/protocol/protocol.md) включают семь
рисунков из `spi/protocol` и `mdio/protocol`. Они объясняют интерфейсы
в общем случае; реализация BVSTK описана в соседних главах.
Девять рисунков в `i2c/cores/slave` показывают приём запросов,
подготовку ответа, окна BRAM и события ведомого ядра.
Двенадцать рисунков в `i2c/cores/master` раскрывают путь команды,
обмен на шине, приём результата и тактирование.
Пояснения, таблицы и примеры находятся в [описании ядер I²C](system/pl-subsystem/i2c/cores/cores.md).
Каждый раздел включает относящиеся к нему рисунки.
Аппаратные адреса и особенности реализации раскрыты в карте памяти
и схемах конкретных ядер.

Новый комплект создаётся из
[generate_pl_subsystem.py](../tools/docs/generate_pl_subsystem.py).
Вводная часть I²C задана в [i2c_protocol.py](../tools/docs/i2c_protocol.py).
Вводные части SPI и MDIO — в [spi_protocol.py](../tools/docs/spi_protocol.py)
и [mdio_protocol.py](../tools/docs/mdio_protocol.py).
Их общие элементы временных диаграмм — в [protocol_drawing.py](../tools/docs/protocol_drawing.py).
Схемы ведомого ядра — в [i2c_slave.py](../tools/docs/i2c_slave.py).
Схемы ведущего ядра — в [i2c_master.py](../tools/docs/i2c_master.py).
Общие элементы последовательностей и форматов слов — в [i2c_core.py](../tools/docs/i2c_core.py).
Тексты и координаты редактируются в этих файлах:

```sh
python3 tools/docs/generate_pl_subsystem.py
```

Семь общих схем редактируются непосредственно в SVG.
Генератор обновляет рисунки подсистем PL.
Генератор не создаёт Markdown и не меняет текст статьи.

## Статья и вставка рисунков

Основной файл статьи — [system.md](system/system.md).
Вводный файл каждого вложенного раздела назван по его папке:
`pl-subsystem.md`, `i2c.md`, `protocol.md`, `cores.md` и так далее.
Отдельные темы, например `slave.md` и `master.md`, располагаются рядом
с вводным файлом своего раздела.

Используется обычная вставка изображения с относительным путём.
Например, из `docs/system/system.md`:

```md
![Уровни абстракции BVSTK](figures/architecture/levels.svg)
```

Рисунок помещается после вводящего абзаца. Его название входит
в подпись «Рисунок N…» в Markdown. В самом SVG остаются схема,
подписи её элементов и легенда. Пояснения и переходы между главами
размещаются в тексте статьи.
Для Obsidian следует открыть хранилище, которое включает
всю папку `docs/system`, чтобы рядом с текстом оставались SVG.
В режиме чтения Markdown изображение отображается внутри главы.
Относительные пути сохраняют связь при переносе всей папки.

Технические сведения об исходниках каждого рисунка сохранены в его
метаданных. Аппаратные схемы основаны на `Burevestnik_21.tcl` и RTL
из соседнего проекта `hw_platform/fpga`; программные сценарии —
на исходниках BVSTK.
