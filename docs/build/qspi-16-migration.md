# Миграция QSPI с 8/24 на 16/16 МиБ

Разметка AX7020: `[0x00000000, 0x01000000)` — загрузочный образ,
`[0x01000000, 0x02000000)` — FAT `flash:/`. Обе прошивки должны быть
пересобраны с одним `bvstk_qspi_layout.h`. Перед переключением исходного
устройства перенесите файлы: старый FAT начинается по адресу `0x00800000`.

Для подготовки нужны `mkfs.fat`, `fsck.fat`, Python и pyfatfs. Установить
зависимости в отдельное окружение можно командами:

```sh
env -u LD_LIBRARY_PATH /usr/bin/python3 -m venv build/qspi-migration/venv
env -u LD_LIBRARY_PATH build/qspi-migration/venv/bin/pip install 'setuptools<81' pyfatfs==1.1.0
```

Работайте с закрытым каталогом: дамп QSPI, резервные файлы и загрузочный
образ могут содержать настройки и ключи. Скопируйте **всё** содержимое
`/flash`, включая скрытые файлы и пустые каталоги, с работающей платы в
закрытый каталог. Сверьте копию с работающей системой:

```sh
python3 scripts/vitis/verify_flash_backup.py \
  --backup /absolute/path/flash --manifest /absolute/path/files.json
env -u LD_LIBRARY_PATH build/qspi-migration/venv/bin/python scripts/vitis/prepare_qspi16_volume.py \
  --backup /absolute/path/flash --manifest /absolute/path/files.json \
  --output /absolute/path/new-fat.img
```

Создание нового образа проверяет каждый файл и FAT до публикации результата.
Пустой том не монтируется поверх действующего, форматирование на плате не
выполняется. Даты файлов при переносе могут измениться; содержимое каждого
файла проверяется SHA-256.

Соберите обе прошивки и комплект Нейтрино с границей 16 МиБ:

```sh
CLEAN=0 ./build.sh freertos
./build.sh neutrino-image
./build_boot.sh freertos --no-build
./build_boot.sh neutrino --medium qspi --no-build
./run.sh neutrino qspi --dry-run
```

Выберите JTAG boot на J13 и включите питание. Сохраните полный дамп QSPI:

```sh
xsct scripts/vitis/qspi_snapshot.tcl \
  artifacts/boot/neutrino-qspi/ps7_init.tcl \
  artifacts/boot/neutrino-qspi/qspi_read.elf \
  /absolute/path/qspi-before.bin
```

Проверьте сценарий без записи; он сверяет файлы старого тома из дампа,
резервную копию, новый FAT-образ, загрузочный комплект и разметку:

```sh
env -u LD_LIBRARY_PATH build/qspi-migration/venv/bin/python scripts/vitis/migrate_qspi16.py \
  --snapshot /absolute/path/qspi-before.bin --volume /absolute/path/new-fat.img \
  --manifest /absolute/path/files.json --output /absolute/path/preflight
```

Повторите с **новым** каталогом `--output` и `--execute`. Сценарий заново
снимает QSPI и сравнивает с резервным дампом, записывает и перечитывает новый
том, затем программирует BOOT.bin и проверяет сохранность тома. При ошибке
после начала записи он пытается восстановить весь исходный дамп и проверить
его чтением. Сохраните оба отчёта и исходную копию. Потеря питания или
отключение JTAG могут прервать автоматический откат; исходный дамп позволяет
восстановить микросхему через JTAG и `program_flash`.

Обычный программатор при обнаружении старого FAT на 8 МиБ откажет в записи,
пока не передан `QSPI_MIGRATED_VOLUME_SHA256`, совпадающий с полным
перечитанным новым томом. Сценарий миграции устанавливает его после проверки.
После успешной записи выберите QSPI boot на J13 и включите питание. Проверьте
`/api/qspi`, все сохранённые файлы, обе шины и холодный запуск. Для резервной
загрузки с PS SD установите новый SD-комплект, собранный с той же границей.
