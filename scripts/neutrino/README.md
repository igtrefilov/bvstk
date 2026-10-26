# Сценарии Нейтрино

| Сценарий | Назначение |
|---|---|
| `build.sh` | Шесть программ: `bvstkctl`, `bvstkd`, `i2c`, `bvstk-shell`, `bvstk-qspi-fat`, `bvstk-sd-raw` |
| `build_image.sh` | Программы, базовый состав BSP, OpenSSL с ГОСТ, локальные SSH-материалы и IFS |
| `build_gost.sh` | GOST ENGINE для ARMv7 и файлы OpenSSL для включения в IFS |
| `gost/openssl-gost-selftest.sh` | Проверка хеширования, шифрования и подписи; в образе доступна как `openssl-gost-selftest` |
| `run_jtag.sh` | Загрузка IFS и запись UART-журнала |
| `verify_ssh.sh` | Проверка программ и менеджера I²C через SSH на стенде |
| `generate_root_shadow.py` | Интерактивная подготовка локального парольного файла |

Полный порядок, ключи и состав образа описаны в
[сборке Нейтрино](../../docs/build/neutrino.md).
Загрузка описана в [руководстве JTAG](../../docs/build/jtag.md).

`run.sh neutrino jtag` объединяет загрузку и SSH-проверку. Прямой
`run_jtag.sh` выполняет только загрузочную часть. Проверочная утилита
отключает постоянный контроль ключа сервера и не предназначена для
установления подлинности узла в недоверенной сети.

UART для JTAG выбирается из стабильных имён `/dev/serial/by-id`. Для AX7020
предпочитается адаптер Silicon Labs CP210x. Если адаптеров несколько, задайте
его явно, например `UART_DEVICE=/dev/ttyUSB0 ./run.sh neutrino jtag`.

В Нейтрино I²C принадлежит `bvstkd`, клиенты используют
`/dev/bvstk-i2c`. PS SD и QSPI обслуживаются отдельными менеджерами
ресурсов; их место в файловом стеке описано в
[статье о BVSTK](../../docs/system/storage/storage.md).
