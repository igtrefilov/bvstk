# Приложения

Справочные приложения: карта директорий, таблица портов и набор примеров команд и запросов.


### Карта директорий

Ключевые каталоги репозитория `bvstk/`:
- `src/` — исходники FreeRTOS, общие PL/service-компоненты, Neutrino platform и `bvstkctl`.
- `configs/` — исходные JSON‑конфиги (сеть + `i2c/*.json`, `smi/*.json`), из которых при сборке генерируются дефолты в `src/config/default_configs.h`.
- `scripts/fpga/` — пакетная сборка Vivado; результаты `design.bit/design.xsa` попадают в `artifacts/fpga/`, журналы — в `artifacts/vivado/logs/`.
- `scripts/vitis/` — сборка и JTAG-запуск FreeRTOS.
- `scripts/neutrino/` — сборка `bvstkctl`, IFS, JTAG-запуск и SSH-проверка Neutrino.
- `web/`
  - `web/assets/` — статические файлы Web UI (кладутся на устройство в `flash:/www/`).
  - `web/upload_flash_www.sh`, `web/upload_flash_www.py` — утилиты загрузки `web/assets/` в `flash:/www/` (mkdir через TCP‑консоль, PUT через HTTP).
- `vitis_ws/` — Vitis workspace (артефакт сборки; пересоздаётся скриптами, вручную обычно не редактируется).
- `build/neutrino/` — производные файлы Neutrino: `bvstkctl`, `.build`, IFS и UART-лог.
- `dot/` — вспомогательные материалы/диаграммы (если используются в проекте).
- `build.sh` — корневой диспетчер сборки `freertos`, `neutrino`, `neutrino-image` и `all`.
- `run.sh` — корневой диспетчер JTAG-запуска `freertos` или `neutrino`.

### Таблица портов/протоколов

Порты, на которых прошивка слушает входящие подключения:

| Назначение | Протокол | Порт | Где описано |
|---|---:|---:|---|
| TCP‑консоль | TCP | 8888 | раздел “TCP‑консоль (порт 8888)” |
| SSH-консоль FreeRTOS | SSH/TCP | 22 | `../dev/freertos-ssh.md`; только при `BVSTK_SSH_ENABLE=1` |
| DCP2 (binary protocol + NOTIFY events) | TCP | 8889 | раздел “DCP2 (порт 8889)” |
| HTTP‑сервер (API + файловый доступ + Web UI) | TCP | 80 | раздел “HTTP‑сервер (порт 80)” |

Neutrino IFS для AX7020 также поднимает SSH на порту `22`, но это системный SSH-сервер Neutrino, а не FreeRTOS-консоль на wolfSSH. Текущая автоматическая проверка запускает через него `/usr/bin/bvstkctl`.

Клиентские (исходящие) сетевые протоколы могут использоваться опционально (зависит от конфигурации/сборки): DHCP, DNS, SNTP, MQTT и т.п. — они не “слушают” порт, а инициируют исходящие запросы.

### Примеры команд и запросов
Примеры и форматы запросов уже приведены в профильных разделах:
- TCP‑консоль: `../user/tcp-console.md`
- HTTP JSON‑API и файловый доступ: `../user/http.md`
- DCP2 и `NOTIFY`: `../user/dcp2-usage.md`
- Web UI и загрузка в `flash:/www/`: `../user/web-ui.md`
- сеть и адресация: `../user/network.md`
- FreeRTOS SSH: `../dev/freertos-ssh.md`

Минимальная “проверка связи” (подставьте IP устройства вместо `<ip>`):
```sh
curl http://<ip>/api/version
curl http://<ip>/api/fs
```
