# TCP‑консоль (порт 8888)

Полный справочник по текстовой TCP-консоли, её правилам ответов и доступным командам.


### Обзор и правила ответов

TCP‑консоль — интерактивный текстовый интерфейс управления устройством по TCP (`:8888`). Сессия похожа на telnet‑терминал: вы вводите команды строками, устройство отвечает текстом и печатает новый prompt.

**Подключение**
```sh
telnet <device-ip> 8888
# или:
nc <device-ip> 8888
```
При подключении выводится banner и prompt вида `Zynq/<fs_label>[:<path>]>`.

**Формат команд**
- Одна команда = одна строка, разделители — пробел/табуляция.
- Команды регистрозависимы? Нет: сравнения делаются case‑insensitive (например, `HELP` == `help`).
- Встроенные команды верхнего уровня: `help`, `reboot`, `quit|exit`, а также утилиты `fs|tar|ip|i2c|smi|mem`.

**Редактирование строки**
- Поддерживаются Backspace, стрелки влево/вправо, история (↑/↓) и автодополнение по Tab (для команд и некоторых аргументов).
- В telnet‑клиентах выполняется базовая договорённость опций (IAC); в `nc` обычно работает “как есть”.

**Правила ответов**
- Вывод — текст с `CRLF` (`\r\n`).
- Большинство команд используют короткий статус:
  - `OK` — команда выполнена,
  - `ERR` — ошибка (иногда с уточнением, например `ERR DENIED`).
- Некоторые команды печатают многострочный вывод (например, `fs ls`, `ip addr show`, `i2c rules`), а **в конце** сессия возвращается к prompt.
- Unknown command возвращает `ERR`.

**Замечания по автоматизации**
- Для скриптов удобно отправлять команды с переводом строки и ждать `OK/ERR` + prompt.
- Изменение сетевых параметров (`ip ... set`) может оборвать текущую TCP‑сессию — переподключайтесь к новому адресу.

### Команды

Ниже приведён список команд TCP‑консоли, краткое описание и примеры. Для чисел поддерживаются десятичные и `0x...` (hex) формы.

**1) Встроенные команды**
- `help` / `-h` / `--help` / `-help` — список доступных утилит верхнего уровня.
  - Пример: `help`
- `quit` / `exit` — закрыть сессию.
  - Пример: `exit`
- `reboot -y [delay_ms]` — перезагрузить устройство (подтверждение обязательно).
  - Пример: `reboot -y 1000`

**2) Файловые команды (FatFs shell)**
Это основной “shell”: команды доступны напрямую (без префикса `fs`).

- `pwd` — показать текущий каталог (внутри выбранного тома).
  - Пример: `pwd`
- `ls [path]` — список файлов/каталогов.
  - Примеры:
    - `ls`
    - `ls flash:/config/`
- `cd <dir>` — перейти в каталог.
  - Особые формы:
    - `cd flash` — переключить активный том на QSPI (`flash:/`)
    - `cd sd` — переключить активный том на SD (`sd:/`)
  - Примеры:
    - `cd flash`
    - `cd /config`
    - `cd sd:/logs`
- `mkdir <dir>` — создать каталог.
  - Пример: `mkdir flash:/www`
- `touch <file>` — создать пустой файл (или обновить метаданные, если поддерживается).
  - Пример: `touch sd:/test.txt`
- `cat <file>` — вывести содержимое файла.
  - Пример: `cat flash:/config/network.json`
- `rm <file|dir>` — удалить файл/пустой каталог.
  - Пример: `rm sd:/test.txt`
- `rm -r <dir>` — рекурсивно удалить каталог.
  - Пример: `rm -r flash:/tmp`
- `cp <src> <dst>` — копировать файл.
  - Пример: `cp sd:/cfg.json flash:/config/cfg.json`
- `cp -r <src> <dst>` — рекурсивно копировать каталог.
  - Пример: `cp -r sd:/www flash:/www`
- `mv <src> <dst>` — переименовать/переместить.
  - Пример: `mv flash:/config/network.json flash:/config/network.bak.json`

Пути:
- явные тома: `0:/...`, `1:/...`
- псевдонимы: `sd:/...`, `flash:/...`
- относительные пути — относительно текущего `pwd` в активном томе.

Справка:
- `fs -h` — печатает шпаргалку по файловым командам.

**3) Архивы (tar)**
- `tar -h|--help` — справка.
- `tar c <src_dir> <dst_tar>` — создать tar‑архив из каталога.
  - Пример: `tar c flash:/www sd:/www.tar`
- `tar x <src_tar> <dst_dir>` — распаковать tar‑архив в каталог.
  - Пример: `tar x sd:/www.tar flash:/www`
- `tar t <src_tar>` — вывести список содержимого tar‑архива.
  - Пример: `tar t sd:/www.tar`

**4) Сеть (ip)**
Команды упрощённые, но совместимы по форме с `iproute2`:
- `ip addr show` — показать IP/маску.
  - Пример: `ip addr show`
- `ip addr set <IPv4>/<prefix>` — установить IP/маску, применить сразу и сохранить в `flash:/config/network.json`.
  - Пример: `ip addr set 192.168.0.10/24`
- `ip link show` — показать MAC.
  - Пример: `ip link show`
- `ip link set address <mac>` — установить MAC, применить сразу и сохранить.
  - Пример: `ip link set address 00:0a:35:00:01:02`
- `ip route show` — показать default gateway.
  - Пример: `ip route show`
- `ip route set default via <gw>` — установить default gateway, применить сразу и сохранить.
  - Пример: `ip route set default via 192.168.0.1`
- `ip save` — сохранить текущие runtime‑параметры `netif` в конфиг (без изменения адресов).
  - Пример: `ip save`

Примечание: смена IP часто рвёт текущую TCP‑сессию — переподключайтесь к новому адресу.

**5) I²C (i2c)**
- `i2c list` — список устройств из `config_store`.
  - Пример: `i2c list`
- `i2c <name> [info]` — вывести параметры устройства.
  - Пример: `i2c axp15060 info`
- `i2c <name> r <reg>` — прочитать регистр (обновляет кеш).
  - Пример: `i2c axp15060 r 0x13`
- `i2c <name> w <reg> <val>` — записать регистр (подчиняется политике).
  - Пример: `i2c axp15060 w 0x13 0x11`
- `i2c @0x<addr_7b> ...` — выбор устройства по адресу вместо имени.
  - Пример: `i2c @0x36 r 0x13`
- `i2c <name> addr <addr_7b>` — изменить `addr_7b` и сохранить.
  - Пример: `i2c axp15060 addr 0x36`
- `i2c <name> policy <whitelist|blacklist>` — переключить политику и сохранить.
  - Пример: `i2c axp15060 policy whitelist`
- `i2c <name> rules` — вывести правила.
  - Пример: `i2c axp15060 rules`
- `i2c <name> allow <reg> <val>` / `deny <reg> <val>` / `clear <reg> <val>` — редактировать правила и сохранить.
  - Примеры:
    - `i2c axp15060 allow 0x13 0x11`
    - `i2c axp15060 deny 0x13 0x12`
    - `i2c axp15060 clear 0x13 0x11`
- `i2c <name> autopoll` — показать настройки autopoll (изменение — через `PUT /api/i2c` или правкой JSON).
  - Пример: `i2c axp15060 autopoll`
- `i2c <name> save` — сохранить policy/rules и текущие persisted settings (`settings[]`) в `flash:/config/i2c/<...>.json`.
  - Пример: `i2c axp15060 save`

**6) SMI/MDIO (smi)**
- `smi list` — список PHY из `config_store`.
  - Пример: `smi list`
- Быстрые формы:
  - `smi r <phy> <reg>` — прочитать регистр PHY.
    - Пример: `smi r 1 0`
  - `smi w <phy> <reg> <data>` — записать регистр PHY (подчиняется политике, если конфиг найден).
    - Пример: `smi w 1 0 0x8000`
- Выбор устройства и работа через селектор:
  - `smi <name|phy|@phy> [info]`
    - Примеры: `smi lan8720 info`, `smi @1 info`
  - `smi <sel> r <reg>` / `smi <sel> w <reg> <data>`
    - Примеры: `smi @1 r 0`, `smi lan8720 w 0 0x8000`
  - `smi <sel> rules` — показать allow/deny.
    - Пример: `smi lan8720 rules`
  - `smi <sel> phy <phy_addr>` — сменить PHY address и сохранить.
    - Пример: `smi lan8720 phy 1`
  - `smi <sel> policy <whitelist|blacklist>` — переключить политику и сохранить.
    - Пример: `smi lan8720 policy whitelist`
  - `smi <sel> allow|deny|clear <reg>` — редактировать списки разрешённых/запрещённых регистров и сохранить.
    - Примеры: `smi lan8720 allow 0`, `smi lan8720 deny 0`, `smi lan8720 clear 0`
  - `smi <sel> autopoll [on|off|reg_delay <ms>|cycle_delay <ms>|regs <r0> <r1> ...]` — настройка autopoll.
    - Примеры:
      - `smi lan8720 autopoll on`
      - `smi lan8720 autopoll regs 0 1 31`
      - `smi lan8720 autopoll cycle_delay 1000`
  - `smi <sel> settings [clear]` — показать/очистить persisted settings.
    - Примеры: `smi lan8720 settings`, `smi lan8720 settings clear`
  - `smi <sel> save` — сохранить текущий конфиг PHY в `flash:/config/smi/<...>.json`.
    - Пример: `smi lan8720 save`

**6.5) SPI (spi)**
- `spi info` — показать текущий runtime‑конфиг SPI драйвера.
  - Пример: `spi info`
- `spi cfg mode <single|multi|fallthrough>` — выбрать пакетный режим.
  - Пример: `spi cfg mode multi`
- `spi cfg timeout <ticks>` — выставить `TIMEOUT_ADDR` (в тактах PL).
  - Пример: `spi cfg timeout 1`
- `spi cfg div <N>` — выставить `SPI_SIG_ADDR.p_clk_div` (рекомендуется чётное `N>=2`).
  - Пример: `spi cfg div 512`
- `spi cfg read <on|off>` — включить/выключить приём (бит `read_en` в `CSR`).
  - Пример: `spi cfg read on`
- `spi xfer <w0> [w1 ...]` — отправить 32‑битные слова и вывести RX слова из BRAM окна SPI.
  - Пример: `spi xfer 0x40AA5500 0x11223344`

**7) Память/регистры (mem, опасно)**
- `mem -h|--help` — справка.
- `mem r <addr>` — чтение: 32‑бит, если адрес выровнен по 4, иначе 8‑бит.
  - Пример: `mem r 0x40000000`
- `mem w <addr> <value>` — запись: 32‑бит для выровненного адреса; для невыровненного — только 8‑бит (value ≤ 0xFF).
  - Примеры:
    - `mem w 0x40000000 0x00000001`
    - `mem w 0x40000003 0x7F`

Используйте `mem` только для диагностики в доверенном контуре: это прямой доступ к MMIO/BRAM и можно повредить состояние устройства.
