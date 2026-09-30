# Исходники рисунков

В руководствах используются девять предметных SVG. Исходник DOT или
Mermaid расположен рядом с изображением. SVG включён обычной Markdown-ссылкой
и не требует выполнения JavaScript у читателя. Параметры Mermaid хранятся
рядом с исходной схемой.

| Рисунок | Исходник | Глава |
|---|---|---|
| Два варианта исполнения | [execution.dot](system/figures/execution.dot) | [Исполнение](system/execution.md) |
| Владение запросом I²C | [i2c-request.dot](system/figures/i2c-request.dot) | [Интерфейсы](system/interfaces.md) |
| Два файловых стека | [storage.dot](system/figures/storage.dot) | [Хранение](system/storage.md) |
| Файл, модель и оборудование | [configuration.dot](system/figures/configuration.dot) | [Конфигурация](system/configuration.md) |
| Происхождение ELF/IFS | [artifacts.dot](build/figures/artifacts.dot) | [Сборка](build/README.md) |
| Граница PS/PL | [ps-pl.dot](reference/figures/ps-pl.dot) | [Плата](reference/board.md) |
| Зависимости исходников | [dependencies.dot](development/figures/dependencies.dot) | [Карта исходников](development/source-map.md) |
| Ведомый IRQ двух ОС | [i2c-irq.dot](development/figures/i2c-irq.dot) | [I²C](development/drivers/i2c.md) |
| Смена IP до ответа | [network-change.mmd](operations/figures/network-change.mmd) | [Сеть](operations/network.md) |

## Воспроизведение

Из корня репозитория, например:

```sh
dot -Tsvg docs/system/figures/execution.dot \
  -o docs/system/figures/execution.svg
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
Эти SVG редактируются непосредственно. Переходы между листами встроены
в заголовки блоков и верхнюю панель навигации.

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

Отдельный комплект в
[pl-subsystem](system/figures/architecture/pl-subsystem/README.md)
раскрывает устройство PL и взаимодействие с программой PS.
Оглавление и порядок чтения находятся в README.
В комплекте 23 предметных листа:

| Раздел | Содержание |
|---|---|
| [BVSTK как мост](system/figures/architecture/pl-subsystem/overview/bridge.svg) | Два участка шины, промежуточная BRAM, передача или остановка транзакции по решению политики |
| [Кэш и подготовка ответов](system/figures/architecture/pl-subsystem/overview/cache.svg) | Обновление значений, обработка записи, достоверность и публикация в BRAM |
| [Состав подсистемы](system/figures/architecture/pl-subsystem/overview/subsystems.svg) | Ответственность сервиса, драйвера, платформы и аппаратных ядер |
| [Порядок операций](system/figures/architecture/pl-subsystem/overview/exchange.svg) | Готовый ответ, получение нового значения и обработка записи |
| [Области обмена](system/figures/architecture/pl-subsystem/overview/memory.svg) | Кто записывает и читает данные, как определяется готовность |
| [I²C](system/figures/architecture/pl-subsystem/i2c/subsystem.svg) | Подсистема, ведущий и ведомый контроллеры |
| [MDIO](system/figures/architecture/pl-subsystem/mdio/subsystem.svg) | Кадр Clause 22, последние записи и подготовленные ответы |
| [SPI и SD](system/figures/architecture/pl-subsystem/spi/subsystem.svg) | Потоки, DMA, оба ядра SPI и чтение блока SD |
| [Карта памяти реализации](system/figures/architecture/pl-subsystem/reference/memory-map.svg) | Адреса BRAM и области обмена конкретных подсистем |
| [Программа PS](system/figures/architecture/pl-subsystem/ps-interaction/startup.svg) | Профили, IRQ, политика, кэш, команды и сохранение настроек |

Цвета слоёв и типы стрелок совпадают с общим архитектурным рядом.
Заголовки со знаком ↗ ведут к соответствующим блокам соседних листов.
Последовательности операций дополняют схемы состава.
Схемы в `overview` описывают общий случай без привязки к шине.
Конкретные адреса и особенности реализации раскрыты в остальных разделах.

Новый комплект создаётся из
[generate_pl_subsystem.py](../tools/docs/generate_pl_subsystem.py).
Тексты и координаты его элементов редактируются в этом файле:

```sh
python3 tools/docs/generate_pl_subsystem.py
```

Скрипт также обновляет ссылки на детализацию в семи общих схемах.
Состав и геометрия этих общих схем по-прежнему редактируются в SVG.
