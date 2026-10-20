# Изменение политики через HTTP

Устройство `device_a` уже настроено в BVSTK под FreeRTOS. Через HTTP
нужно запретить все его сервисные записи. В примере включается пустой белый список,
после чего новая политика сохраняется. Физические регистры не меняются.
Во время редактирования другие интерфейсы не должны менять это устройство.

Команды выполняются в отдельном рабочем каталоге на компьютере.
Потребуются `curl` и Python 3.
Укажите адрес платы, существующую подсистему (`i2c` или `mdio`) и имя
устройства вместо значений в угловых скобках:

```sh
bvstk_url='http://192.168.0.10'
bvstk_bus='<подсистема>'
bvstk_device='device_a'
```

## Получение исходного снимка

```sh
curl -fsS -G "$bvstk_url/api/ui/bus"   --data-urlencode "bus=$bvstk_bus"   --data-urlencode "name=$bvstk_device"   -o snapshot.json
```

Проверьте, что ответ содержит `config` и `revision` выбранного устройства.
Сохраните полную конфигурацию в черновик, изменив только режим
и белый список:

```sh
python3 - <<'PY'
import json
from pathlib import Path

snapshot = json.loads(Path('snapshot.json').read_text())
draft = snapshot['config']
draft['policy'] = 'whitelist'
draft['whitelist'] = []
Path('draft.json').write_text(json.dumps(draft, ensure_ascii=False))
Path('revision.txt').write_text(snapshot['revision'])
PY
```

Остальные поля сохраняются. `config` является проекцией API, поэтому
подмена его содержимым профиля v2 из файловой системы недопустима.

## Применение

```sh
bvstk_revision=$(cat revision.txt)
curl -sS -X PUT -H 'Content-Type: application/json'   --data-binary @draft.json   -o apply-result.json -w '%{http_code}
'   "$bvstk_url/api/ui/bus?bus=$bvstk_bus&name=$bvstk_device&action=apply&revision=$bvstk_revision"
cat apply-result.json
```

Продолжайте только при завершённой передаче, HTTP 200 и `ok:true`.
HTTP 409 означает конфликт; получить актуальные настройки и заново
сформировать черновик нужно до повторного применения. При сетевой ошибке
сначала прочитайте состояние: прежний запрос уже мог выполниться.

После успеха получите новый снимок тем же GET, сохранив его
в `after-apply.json`. Проверьте результат и извлеките новую ревизию:

```sh
curl -fsS -G "$bvstk_url/api/ui/bus"   --data-urlencode "bus=$bvstk_bus"   --data-urlencode "name=$bvstk_device"   -o after-apply.json
python3 - <<'PY'
import json
from pathlib import Path

snapshot = json.loads(Path('after-apply.json').read_text())
assert snapshot['config']['policy'] == 'whitelist'
assert snapshot['config']['whitelist'] == []
Path('revision.txt').write_text(snapshot['revision'])
print('saved:', snapshot['saved'])
PY
```

Теперь сервис отклоняет записи по политике. Значение `saved:false`
означает, что рабочая конфигурация отличается от файла. Если файл уже
содержал такую политику, возможно `saved:true`.

## Сохранение и проверка

```sh
bvstk_revision=$(cat revision.txt)
curl -sS -X PUT -o save-result.json -w '%{http_code}
'   "$bvstk_url/api/ui/bus?bus=$bvstk_bus&name=$bvstk_device&action=save&revision=$bvstk_revision"
cat save-result.json
```

Тело запроса отсутствует. Отправлять формальный `{}` для этого действия
не нужно. После HTTP 200 и `ok:true` повторите GET в `saved.json`:

```sh
curl -fsS -G "$bvstk_url/api/ui/bus"   --data-urlencode "bus=$bvstk_bus"   --data-urlencode "name=$bvstk_device"   -o saved.json
python3 - <<'PY'
import json
from pathlib import Path

snapshot = json.loads(Path('saved.json').read_text())
assert snapshot['config']['policy'] == 'whitelist'
assert snapshot['config']['whitelist'] == []
assert snapshot['saved'] is True
PY
```

Эта проверка сопоставляет рабочие настройки с сохранёнными. Чтение файла
может завершиться ошибкой; тогда `saved:null` требует выяснить состояние носителя.
При ошибке сохранения успешно применённая политика остаётся в RAM.

Механизм разобран в [главе о настройках](../device-settings.md).
Полный контракт запросов и ограничения изменений находятся
в [справочнике устройств](../reference/devices.md#подсистемы).
