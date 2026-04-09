# HTTP‑сервер (порт 80)

Подробный разбор HTTP-сервера, маршрутизации, JSON API и файлового доступа.


### Роутинг и форматы ответов

HTTP‑сервер — это тонкий слой поверх lwIP sockets, который обслуживает **один HTTP‑запрос на одно TCP‑соединение** (после ответа соединение закрывается). Маршрутизация и ответы реализованы в `src/http_fs/http_fs_routes.c`, парсинг запроса/тела — в `src/http/http_server.c`.

**Абстрактная схема обработки запроса**
1) Прочитать request line (`METHOD path HTTP/…`) и заголовки.  
2) Если есть тело запроса: читать его либо по `Content-Length`, либо по `Transfer-Encoding: chunked`.  
3) Прогнать `path` через роутер (строго сверху вниз) и отдать ответ.  
4) Закрыть соединение (`Connection: close`).

**Роутер (порядок важен)**
0) **`/api/*`** — JSON‑API.
   - Матчится по пути **без query** (например, `/api/i2c?name=…` → `/api/i2c`).
   - Подробности по эндпоинтам/форматам — в разделе [/api/*](#api).

1) **`/sd/...`, `/flash/...`, `/tar/...`** — файловый API.
   - Подробности по маппингу/ограничениям путей и tar‑режиму — в разделе [Файловый API](#Файловый%20API).

2) **Web UI (статика из `flash:/www/`)** — только `GET`.
   - Подробности по маппингу статики и `Content-Type` — в разделе [Раздача Web UI](#Раздача%20Web%20UI).

Если запрос не попал ни в один маршрут — возвращается `404 Not Found`.

**Форматы ответов**
- Сервер отвечает `HTTP/1.0` и всегда закрывает соединение (`Connection: close`), поэтому клиенту не стоит рассчитывать на keep‑alive.
- Текстовые ответы/ошибки — `text/plain` + `Content-Length` (например: `400`, `403`, `404`, `405`, `411`, `500`, `503`).
- JSON‑ответы — `application/json; charset=utf-8` **без `Content-Length`**; тело заканчивается `\n` (клиент читает до закрытия соединения).
- Отдача файлов и tar описана в разделах [Файловый API](#Файловый%20API) и [Раздача Web UI](#Раздача%20Web%20UI).

**Ограничения**
- API‑`PUT` читает тело в фиксированный буфер (порядка сотен/тысяч байт); передавайте компактный JSON. Файловые `PUT`/`tar` идут потоково и не требуют “влезать” в буфер.
- Роутинг и фильтрация путей **не заменяют** модель безопасности: файловые `PUT` и диагностические `PUT /api/diag/*` предполагают доверенный контур и осторожное применение.

### /api/*

`/api/*` — это небольшой JSON‑API для управления устройством и получения статуса. Он специально “плоский”: без авторизации/сессий, без keep‑alive, без стриминга JSON. Для операций, где нужна безопасность, используются явные “опасные” эндпоинты (`/api/diag/*`) и флаг подтверждения.

Общее:
- Методы: только `GET` и `PUT` (иначе `405`).
- `PUT` требует `Content-Length` или `Transfer-Encoding: chunked` (иначе `411`).
- Успешные ответы — `200` + JSON, ошибки — чаще `text/plain` с кодом `4xx/5xx` (см. “Роутинг и форматы ответов”).

#### `GET /api/version`
Сборочная информация.
- Ответ (пример):
  - `{"build_date":"Jan 14 2026","build_time":"13:21:09","http_port":80}`

#### `GET /api/rtos`
Статус FreeRTOS/памяти.
- Ответ:
  - `uptime_ms` — аптайм в миллисекундах.
  - `tick_rate_hz` — `configTICK_RATE_HZ`.
  - `heap_free`, `heap_min_ever` — текущий/минимальный за жизнь free heap.

#### `GET /api/net`
Текущее состояние сети (по `netif`).
- Ответ:
  - `ip`, `netmask`, `gateway`, `mac` — строки.
  - `mode` — `"dhcp"` или `"static"`.
  - `dhcp`, `up`, `link_up` — bool.

#### `PUT /api/net`
Записать сетевые настройки в `config_store` (и опционально применить на лету).
- Входной JSON:
  - `ip` — `"a.b.c.d"` или `"a.b.c.d/prefix"`.
  - `netmask` — `"a.b.c.d"` (если `ip` без `/prefix`).
  - `prefix` — число 0..32 (альтернатива `netmask`, если `ip` без `/prefix`).
  - `gateway` — `"a.b.c.d"`.
  - `mac` — `"aa:bb:cc:dd:ee:ff"`.
  - `apply` — `true|false` (по умолчанию `true`): применить к `netif` сразу.
- Ответ:
  - `{"ok":true,"saved":true|false,"applied":true|false}`
- Пример:
  - `curl -X PUT -H 'Content-Type: application/json' --data '{\"ip\":\"192.168.1.10/24\",\"gateway\":\"192.168.1.1\",\"mac\":\"02:12:34:56:78:9a\",\"apply\":true}' http://<ip>/api/net`

#### `GET /api/fs`
Состояние томов `flash`/`sd`.
- Ответ:
  - `{"volumes":[{"name":"flash","ready":true,"total_bytes":...,"free_bytes":...}, ...]}`

#### `GET /api/qspi`
Состояние QSPI FS и её диапазон во флеше.
- Ответ:
  - `ready` — готов ли `flash:/` (QSPI том).
  - `fs_base_bytes`, `fs_size_bytes` — базовый оффсет/размер FS (в байтах).

#### `GET /api/i2c` и `GET /api/i2c?name=<device>`
Конфигурация I²C‑устройств из `config_store`.
- Общий ответ содержит `ready` (готов ли `config_store`).
- `GET /api/i2c` возвращает “список”:
  - `count` и `devices[]` (краткая сводка: `name`, `addr_7b`, `file_name`, `policy`, длины списков/настроек).
- `GET /api/i2c?name=...` возвращает “одно устройство”:
  - `device{ name, addr_7b, file_name, policy, autopoll_enabled, autopoll_reg_delay_ms, autopoll_cycle_delay_ms, autopoll_regs[], whitelist[], blacklist[] }`
  - Если имя неизвестно, сервер всё равно отвечает `200` и кладёт `error:"unknown device"` в JSON.
- Примеры:
  - `curl http://<ip>/api/i2c`
  - `curl 'http://<ip>/api/i2c?name=rtc'`

#### `PUT /api/i2c`
Обновить конфиг одного I²C‑устройства (частичное обновление).
- Требуется `name` (имя устройства в `config_store`), иначе `400`; неизвестное имя → `404`.
- Изменяемые поля (передайте только то, что хотите поменять):
  - `policy`: `"whitelist"` или `"blacklist"`.
  - `autopoll_enabled`: bool.
  - `autopoll_reg_delay_ms`, `autopoll_cycle_delay_ms`: числа.
  - `autopoll_regs`: массив **десятичных** чисел 0..255 (полностью заменяет список).
  - `whitelist`/`blacklist`: массивы объектов `{reg,val}` (0..255, допускаются `0x..`), полностью заменяют соответствующий список.
- Не меняются через API: `addr_7b`, `file_name` (они берутся из существующего конфига).
- Ответ:
  - `{"ok":true,"saved":true|false}`
- Пример:
  - `curl -X PUT -H 'Content-Type: application/json' --data '{\"name\":\"rtc\",\"policy\":\"whitelist\",\"autopoll_enabled\":true,\"autopoll_regs\":[0,1,2]}' http://<ip>/api/i2c`

#### `PUT /api/reboot`
Инициировать перезагрузку (через watchdog) из отдельной задачи.
- Входной JSON:
  - `confirm:true` — обязательно.
  - `delay_ms` — задержка перед перезагрузкой (по умолчанию `1000`, максимум `5000`).
- Ответ:
  - `{"ok":true,"rebooting":true}`
- Пример:
  - `curl -X PUT -H 'Content-Type: application/json' --data '{\"confirm\":true,\"delay_ms\":1000}' http://<ip>/api/reboot`

#### Диагностика: `PUT /api/diag/*` (опасно)
Эндпоинты прямого доступа для проверки “железа”:
- `PUT /api/diag/i2c/read|write` — прямое чтение/запись I²C регистров (write подчиняется политике).
- `PUT /api/diag/smi/read|write` — прямое чтение/запись MDIO регистров (write подчиняется политике).
- `PUT /api/diag/mem/read|write` — MMIO/BRAM чтение/запись (требует `confirm:true` для write).

Используйте диагностические эндпоинты только в доверенном контуре: они предназначены для проверки “железа” и дают доступ к реальным шинам/памяти.

### Файловый API

Файловый API даёт доступ к томам FatFs по HTTP и используется для:
- загрузки/выгрузки отдельных файлов (`/sd/...`, `/flash/...`);
- пакетной загрузки/выгрузки каталогов через tar (`/tar/...`).

Поддерживаемые маршруты:
- `GET/PUT /sd/<path>` → `sd:/<path>` (SD‑карта)
- `GET/PUT /flash/<path>` → `flash:/<path>` (QSPI FS)
- `GET/PUT /tar/sd/<dir>` → tar‑поток каталога `sd:/<dir>`
- `GET/PUT /tar/flash/<dir>` → tar‑поток каталога `flash:/<dir>`

**Правила путей**
- URL‑путь декодируется (`%xx`, `+` → пробел).
- Запрещены `..` и `:` (чтобы нельзя было выйти из корня тома/указать “диск” вручную).
- Для Web UI дополнительно запрещён `\\`, но в файловом API используйте только `/`.

#### Одиночные файлы: `/sd/...` и `/flash/...`
**GET**: отдать файл как бинарный поток.
- Если путь указывает на каталог — будет `404` (листинга директорий нет).
- `Content-Type: application/octet-stream`, `Content-Length: <size>`.
- Пример (скачать конфиг):
  - `curl -o network.json http://<ip>/flash/config/network.json`

**PUT**: записать/перезаписать файл.
- Требуется `Content-Length` или `Transfer-Encoding: chunked` (иначе `411`).
- Файл создаётся/перезаписывается целиком (`CREATE_ALWAYS`).
- Промежуточные каталоги **не создаются**: если `flash:/config/...` не существует, получите `500 open failed`.
- Пример (залить файл):
  - `curl -T network.json http://<ip>/flash/config/network.json`

#### Каталоги как tar: `/tar/...`
Tar‑режим нужен, когда надо перенести дерево файлов (например, Web UI или набор конфигов) без десятков отдельных запросов.

**GET**: “упаковать каталог в tar” и отдать поток.
- `Content-Type: application/x-tar` (без `Content-Length`, поток до закрытия соединения).
- Примеры:
  - `curl -o www.tar http://<ip>/tar/flash/www`
  - `curl -o cfg.tar http://<ip>/tar/flash/config`

**PUT**: принять tar‑поток и распаковать *в указанный каталог*.
- Требуется `Content-Length` или `chunked` (иначе `411`).
- Каталог назначения создаётся рекурсивно (mkdir -p); внутри создаются подкаталоги и файлы.
- Внутри tar запрещены абсолютные пути, `..` и `:` (защита от “выхода” из каталога назначения).
- Примеры:
  - `curl -T www.tar http://<ip>/tar/flash/www`
  - `curl -T cfg.tar http://<ip>/tar/flash/config`

**Практика**
- Для обновления Web UI обычно удобнее tar‑PUT в `flash:/www/` (см. “Веб‑ресурсы и деплой”).
- Для создания каталогов без tar используйте TCP‑консоль (`mkdir`) — файловый API сам по себе каталоги не создаёт.

### Раздача Web UI

Web UI раздаётся HTTP‑сервером как статика с QSPI‑тома: `flash:/www/` (см. также “Web UI в flash:/www/” и “Веб‑ресурсы и деплой”).

Маршрут:
- `GET /...` (любой путь, не попавший в `/api/*` и файловый API) маппится в файл внутри `flash:/www/`.
- Методы кроме `GET` для Web UI не поддерживаются.

Правила маппинга:
- `/` → `flash:/www/index.html`.
- `/dir/` → `flash:/www/dir/index.html` (если путь заканчивается `/`, подставляется `index.html`).
- `?query` и `#fragment` игнорируются при выборе файла.
- URL‑путь декодируется (`%xx`, `+` → пробел), но запрещены `..`, `:` и `\\` (защита от выхода из `www` и “windows‑путей”).

Content‑Type:
- Определяется по расширению файла:
  - `.html/.htm` → `text/html; charset=utf-8`
  - `.css` → `text/css; charset=utf-8`
  - `.js` → `application/javascript; charset=utf-8`
  - `.json` → `application/json; charset=utf-8`
  - `.png` → `image/png`
  - `.jpg/.jpeg` → `image/jpeg`
  - `.svg` → `image/svg+xml`
  - `.ico` → `image/x-icon`
  - `.txt` → `text/plain; charset=utf-8`
  - прочее → `application/octet-stream`

Примеры:
- Открыть главную страницу:
  - `http://<ip>/`
- Открыть страницу в подкаталоге:
  - `http://<ip>/ui/` → `flash:/www/ui/index.html`
