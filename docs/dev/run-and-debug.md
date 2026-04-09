# Запуск и отладка

Подробности запуска по JTAG, step-debug и первичных способов подключения к сервисам устройства.


### Запуск по JTAG

JTAG‑запуск используется для разработки/отладки: программируется PL (bitstream), инициализируется PS7, загружается ELF приложения и выполняется `con` (run).

**Предусловия**
- Активировано окружение Xilinx (чтобы `xsct` был в `PATH`): `source <Vitis-install>/settings64.sh`
- Запущен `hw_server` (локально или на удалённой машине) и доступен JTAG‑кабель.
- Собран ELF: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`

**Команда**
```sh
./run_jtag.sh /abs/path/to/design.bit
```
Если не передавать аргумент, `run_jtag.sh` берёт `BITSTREAM_FILE` из окружения/`scripts/vitis/run_jtag.conf`; иначе `run_jtag.tcl` использует дефолт (`../bvstk_hw/tmp/design.bit`, fallback `vitis_ws/plat_bvstk/export/.../hw/`).

**Что делает `run_jtag.tcl` (упрощённо)**
1. `connect` к `hw_server`
2. `rst -system` + остановка CPU
3. `fpga -f <bit>` — программирование PL
4. `source ps7_init.tcl`, затем `ps7_init` и `ps7_post_config`
5. `dow <app_bvstk.elf>` — загрузка ELF в core0
6. `con` — запуск выполнения

### JTAG step‑debug (VSCode/GDB)

Если нужно ставить брейкпоинты и шагать по коду из VSCode, удобнее подготовить таргет так, чтобы core0 остался остановленным, а затем подключить GDB.

1) Подготовить таргет (PL + PS7 init + halt core0):
```sh
./run_jtag.sh --debug /abs/path/to/design.bit
```

2) В VSCode:
- `Run and Debug` → `Attach: Zynq-7000 (core0, after ./run_jtag.sh --debug)` → `F5`.

Важно:
- `hw_server` печатает порт `3121`, это **TCF** (для `xsct/xsdb`).
- GDB подключается к другому порту (для ARM32 обычно `3000`). Если он не поднят, запускайте `hw_server` с `-p 3000`.

**Замечания**
- Путь к ELF фиксирован: `vitis_ws/app_bvstk/Debug/app_bvstk.elf`.
- Путь к `ps7_init.tcl` берётся из export платформы: `vitis_ws/plat_bvstk/export/plat_bvstk/hw/ps7_init.tcl`.
- При проблемах с `.bit` проверьте приоритет: аргумент `run_jtag.sh` → `BITSTREAM_FILE` (env/conf) → дефолт из `run_jtag.tcl`.

### Подключение к TCP‑консоли

TCP‑консоль — основной интерактивный канал управления (порт `8888`). Подключение похоже на telnet‑сессию: ввод команд строками, вывод — текст с `OK/ERR`.

**Подключение**
```sh
telnet <device-ip> 8888
```

Если `telnet` не установлен, можно использовать `nc`:
```sh
nc <device-ip> 8888
```

**Первичные проверки**
```
help
ip addr show
fs pwd
fs ls
```

**Важно**
- Если вы меняете IP через `ip addr set ...`, текущая сессия может оборваться — это ожидаемо, переподключайтесь к новому адресу.
- Для работы команд `fs` должны быть смонтированы тома `sd:/` и/или `flash:/` (монтирование делается фоновыми задачами).

### Подключение к DCP2

DCP2 — отдельный бинарный TCP‑сервис управления на порту `8889`. Он не использует telnet‑совместимый текстовый ввод и работает независимо от TCP‑консоли.

Основные свойства:
- транспорт: TCP
- порт по умолчанию: `8889`
- версия wire‑protocol: `0x0002`
- полная спецификация: `docs/dcp2.md`

Подключение клиентом выполняется как обычное TCP‑соединение с последующим обменом DCP2‑кадрами:
```text
connect(<device-ip>, 8889)
  -> PING request/response
  -> optional NOTIFY_SUBSCRIBE
  -> дальнейший request/response и async event по тому же сокету
```

Для первичной ручной проверки удобно использовать Python‑клиент:
```sh
./scripts/dcp2/monitor_notify.py <device-ip> --port 8889
```

Если нужен только мониторинг событий I2C от telnet и host:
```sh
./scripts/dcp2/monitor_notify.py <device-ip> --port 8889 --buses i2c --sources telnet,host
```
