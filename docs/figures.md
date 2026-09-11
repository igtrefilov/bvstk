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

Существующие обзорные [схемы проекта](../svg/) сохранены отдельно от
активного комплекта. Они не используются как источник перечня активных
аппаратных возможностей.
