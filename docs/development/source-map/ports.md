# ports

[Карта исходников](../source-map.md)

[src/ports](../../../src/ports) связывает общие модули с ОС и платформой.
Верхние ветви соответствуют целевым средам. Внутри каждой отдельно
размещены системные операции, обслуживание аппаратных событий и носители.
Общие интерфейсы объявлены в [shared](shared.md).

Для MMIO оба порта реализуют одни и те же функции. Связь этих файлов
с общим заголовком показана на [рисунке в главе shared](shared.md#sharedinterfaces).

## ports/freertos-xilinx

Порт FreeRTOS использует средства ядра и Xilinx BSP. В него также
включены связующий код FatFs и файлы линковки приложения.

### ports/freertos-xilinx/board

Проверки соответствия общего описания платы выбранному BSP.

#### ports/freertos-xilinx/board/ax7020

| Файл | Назначение |
|---|---|
| [bvstk_hw_validate.c](../../../src/ports/freertos-xilinx/board/ax7020/bvstk_hw_validate.c) | Содержит проверки адресов и размеров областей по `xparameters.h` для выбранного аппаратного состава. |

### ports/freertos-xilinx/os

Реализации общих системных операций. Драйверу предоставляются MMIO,
часы и блокировки; типы FreeRTOS остаются в этой части исходников.

| Файл | Назначение |
|---|---|
| [bvstk_platform_freertos.c](../../../src/ports/freertos-xilinx/os/bvstk_platform_freertos.c) | Выполняет MMIO через BSP, получает время по тикам FreeRTOS, реализует ожидание и начальную подготовку платформы. |
| [bvstk_sync_freertos.h](../../../src/ports/freertos-xilinx/os/bvstk_sync_freertos.h) | Описывает объект mutex FreeRTOS вместе с его общим интерфейсом `bvstk_mutex_t`. |
| [bvstk_sync_freertos.c](../../../src/ports/freertos-xilinx/os/bvstk_sync_freertos.c) | Создаёт и уничтожает mutex, переводит время ожидания в тики и связывает системные операции с общим интерфейсом. |

#### ports/freertos-xilinx/os/i2c

Доставка событий ведомого I²C в общий сервис. Короткий обработчик
прерывания захватывает сведения о событии, дальнейшую работу
выполняет задача.

| Файл | Назначение |
|---|---|
| [bvstk_i2c_slave_freertos.h](../../../src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.h) | Определяет адаптер с драйвером, сервисом, очередью событий и задачей обработки. |
| [bvstk_i2c_slave_freertos.c](../../../src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c) | Подключает IRQ, передаёт события в очередь, вызывает ведомый сервис и записывает подготовленный ответ в PL. |

#### ports/freertos-xilinx/os/mdio

MDIO обслуживается периодически выполняемой задачей, которая вызывает
шаг общего сервиса. Здесь же задаются часы и короткие ожидания для
ведущего драйвера.

| Файл | Назначение |
|---|---|
| [bvstk_mdio_freertos.h](../../../src/ports/freertos-xilinx/os/mdio/bvstk_mdio_freertos.h) | Объявляет запуск задачи MDIO и получение используемых ею часов. |
| [bvstk_mdio_freertos.c](../../../src/ports/freertos-xilinx/os/mdio/bvstk_mdio_freertos.c) | Создаёт задачу обслуживания MDIO и реализует отсчёт времени через таймер процессора. |

### ports/freertos-xilinx/fs-fatfs

Нижняя граница FatFs: операции диска преобразуются в обращения
к нужному носителю. Над этим каталогом находится файловая система,
под ним — аппаратный доступ к QSPI, PS SD или PL SD.

| Файл | Назначение |
|---|---|
| [diskio.c](../../../src/ports/freertos-xilinx/fs-fatfs/diskio.c) | Реализует дисковые операции FatFs для трёх носителей, проверяет диапазоны секторов и направляет запрос соответствующему драйверу. |
| [ffsystem_freertos.c](../../../src/ports/freertos-xilinx/fs-fatfs/ffsystem_freertos.c) | Предоставляет FatFs выделение памяти и системные блокировки средствами FreeRTOS. |
| [xilffs_config.h](../../../src/ports/freertos-xilinx/fs-fatfs/xilffs_config.h) | Согласует тип блокировки и настройку длинных имён с используемым вариантом FatFs. |

### ports/freertos-xilinx/storage

Адаптация аппаратных носителей. Код приложения выше этой границы
монтирует тома и обслуживает файлы.

#### ports/freertos-xilinx/storage/qspi-flash

Операции с микросхемой QSPI через контроллер PS. Размещение окна
файловой системы задаётся общим описанием платы.

| Файл | Назначение |
|---|---|
| [qspi_flash.h](../../../src/ports/freertos-xilinx/storage/qspi-flash/qspi_flash.h) | Объявляет подготовку QSPI, чтение, программирование, стирание сектора и самопроверку. |
| [qspi_flash.c](../../../src/ports/freertos-xilinx/storage/qspi-flash/qspi_flash.c) | Управляет QSPI через BSP, формирует команды микросхемы и ожидает завершение записи или стирания. |

#### ports/freertos-xilinx/storage/sd-pl

Один адаптер подключает общий ведущий драйвер SD к IRQ, DMA и памяти
FreeRTOS. Второй предоставляет блочный доступ к выбранному FAT-тому
для `diskio.c`. Разделение позволяет сохранить управление картой
в общем сервисе.

| Файл | Назначение |
|---|---|
| [bvstk_sd_master_freertos.h](../../../src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.h) | Объявляет запуск адаптера после старта планировщика и получение экземпляра сервиса SD. |
| [bvstk_sd_master_freertos.c](../../../src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.c) | Подключает IRQ и параметры аппаратуры из BSP, настраивает доступ к BRAM, создаёт общий драйвер и сервис карты. |
| [bvstk_sd_pl.h](../../../src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.h) | Объявляет подготовку тома, проверку готовности и чтение или запись его секторов. |
| [bvstk_sd_pl.c](../../../src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.c) | Находит FAT-том на карте, переводит его секторы в LBA и передаёт блочные операции общему сервису SD. |

### ports/freertos-xilinx/linker

Файлы, которые определяют размещение приложения в памяти и его
стартовые объекты. Они подключаются сборкой отдельно от исходников C.

| Файл | Назначение |
|---|---|
| [lscript.ld](../../../src/ports/freertos-xilinx/linker/lscript.ld) | Задаёт области памяти, размещение секций, точку входа, стек и кучу приложения Cortex-A9. |
| [Xilinx.spec](../../../src/ports/freertos-xilinx/linker/Xilinx.spec) | Задаёт стартовые объектные файлы для вызова компоновщика через инструменты Xilinx. |

## ports/neutrino-zynq7000

Порт Нейтрино использует системное отображение аппаратной памяти,
потоки и mutex. Носители здесь представлены отдельными программами —
менеджерами ресурсов. У них есть собственные точки входа `main()`.

### ports/neutrino-zynq7000/os

| Файл | Назначение |
|---|---|
| [bvstk_platform_neutrino.c](../../../src/ports/neutrino-zynq7000/os/bvstk_platform_neutrino.c) | Получает права аппаратного доступа, отображает MMIO в процесс, реализует чтение и запись, часы и ожидание средствами ОС. |
| [bvstk_sync_neutrino.h](../../../src/ports/neutrino-zynq7000/os/bvstk_sync_neutrino.h) | Описывает mutex pthread вместе с общим интерфейсом блокировки. |
| [bvstk_sync_neutrino.c](../../../src/ports/neutrino-zynq7000/os/bvstk_sync_neutrino.c) | Создаёт и освобождает mutex, реализует операции захвата и отпускания для общего кода. |

#### ports/neutrino-zynq7000/os/i2c

Отдельный поток ожидает IRQ ведомого ядра I²C, затем вызывает общие
драйвер и сервис. Его время жизни связано с процессом `bvstkd`.

| Файл | Назначение |
|---|---|
| [bvstk_i2c_slave_neutrino.h](../../../src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.h) | Определяет состояние потока IRQ, согласование его запуска и связь с блокировкой рабочей среды. |
| [bvstk_i2c_slave_neutrino.c](../../../src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c) | Подключает прерывание, ожидает событие, обрабатывает запрос через сервис и разрешает дальнейшую доставку IRQ. |

### ports/neutrino-zynq7000/storage

Менеджеры предоставляют носитель как файл с доступом по смещению.
Следующий слой ОС подключает к нему блочный стек и файловую систему.
Эти программы собираются отдельно от `bvstkd`.

| Файл | Назначение |
|---|---|
| [bvstk_qspi_raw.c](../../../src/ports/neutrino-zynq7000/storage/bvstk_qspi_raw.c) | Реализует программу `bvstk-qspi-fat`: управляет QSPI PS и предоставляет чтение и запись окна, выделенного под FAT. |
| [bvstk_sd_raw.c](../../../src/ports/neutrino-zynq7000/storage/bvstk_sd_raw.c) | Реализует программу `bvstk-sd-raw`: инициализирует карту через контроллер SD PS и обслуживает доступ к её секторам. |

Здесь `bvstk_sd_raw.c` относится к SD-контроллеру PS. Путь SD через
PL описан в ветви FreeRTOS выше. Общая организация носителей показана
в [главе хранения данных](../../system/storage/storage.md).
