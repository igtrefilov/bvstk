# Дерево профиля

<div class="bvstk-json-tree" data-json-tree="profile">
<details open><summary><a class="internal-link" data-href="device-profile.md#Структура профиля" href="device-profile.md#Структура%20профиля">Профиль {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Структура профиля" href="device-profile.md#Структура%20профиля">schema_version</a></p>
<p><a class="internal-link" data-href="device-profile.md#Структура профиля" href="device-profile.md#Структура%20профиля">id</a></p>
<details><summary><a href="device-profile.md#Фоновое%20получение%20данных">acquisition {}</a></summary>
<p>enabled</p><p>read_delay_ms</p><p>retry_delay_ms</p>
<details><summary>groups []</summary>
<p>id</p><p>interval_ms</p><p>chunk_size</p>
<details><summary>ranges []</summary><p>first</p><p>count</p></details>
<details><summary>when []</summary><p>resource</p><p>mask</p><p>equals</p></details>
</details></details>
<details><summary><a class="internal-link" data-href="device-profile.md#Описание устройства" href="device-profile.md#Описание%20устройства">device {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Описание устройства" href="device-profile.md#Описание%20устройства">model</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание устройства" href="device-profile.md#Описание%20устройства">vendor</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание устройства" href="device-profile.md#Описание%20устройства">class</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание устройства" href="device-profile.md#Описание%20устройства">label</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">transport {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">bus</a></p>
<p><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">controller</a></p>
<p><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">address</a></p>
<p><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">address_bits</a></p>
<p><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">parent</a></p>
<p><a class="internal-link" data-href="device-profile.md#Подключение" href="device-profile.md#Подключение">channel</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Адресные пространства" href="device-profile.md#Адресные%20пространства">address_spaces []</a></summary>
<details open><summary><a class="internal-link" data-href="device-profile.md#Описание пространства" href="device-profile.md#Описание%20пространства">элемент массива {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Описание пространства" href="device-profile.md#Описание%20пространства">id</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание пространства" href="device-profile.md#Описание%20пространства">address</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание пространства" href="device-profile.md#Описание%20пространства">register_count</a></p>
</details>
</details>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Обработчик обмена" href="device-profile.md#Обработчик%20обмена">protocol {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Обработчик обмена" href="device-profile.md#Обработчик%20обмена">driver</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">options {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">max_write_value</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">capture_cache_on_save</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">presence_check_enabled</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">presence_check_resource</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">autopoll_enabled</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">autopoll_order []</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">autopoll_reg_delay_ms</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры обработчика" href="device-profile.md#Параметры%20обработчика">autopoll_cycle_delay_ms</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">transfer {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">address_bytes</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">address_order</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">address_base</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">read_restart</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Последовательность чтения" href="device-profile.md#Последовательность%20чтения">read_sequence {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Последовательность чтения" href="device-profile.md#Последовательность%20чтения">mode</a></p>
<p><a class="internal-link" data-href="device-profile.md#Последовательность чтения" href="device-profile.md#Последовательность%20чтения">span</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Последовательность записи" href="device-profile.md#Последовательность%20записи">write_sequence {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Последовательность записи" href="device-profile.md#Последовательность%20записи">mode</a></p>
<p><a class="internal-link" data-href="device-profile.md#Последовательность записи" href="device-profile.md#Последовательность%20записи">span</a></p>
</details>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">write_cycle_ms</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">verify_write</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">cache_after_write</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">host_read</a></p>
<p><a class="internal-link" data-href="device-profile.md#Параметры передачи" href="device-profile.md#Параметры%20передачи">host_current_read</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Коммутатор" href="device-profile.md#Коммутатор">switch {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Коммутатор" href="device-profile.md#Коммутатор">resource</a></p>
<p><a class="internal-link" data-href="device-profile.md#Коммутатор" href="device-profile.md#Коммутатор">select_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Коммутатор" href="device-profile.md#Коммутатор">preserve_mask</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Каналы коммутатора" href="device-profile.md#Каналы%20коммутатора">channels []</a></summary>
<details open><summary><a class="internal-link" data-href="device-profile.md#Каналы коммутатора" href="device-profile.md#Каналы%20коммутатора">элемент массива {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Каналы коммутатора" href="device-profile.md#Каналы%20коммутатора">select</a></p>
<p><a class="internal-link" data-href="device-profile.md#Каналы коммутатора" href="device-profile.md#Каналы%20коммутатора">match_mask</a></p>
</details>
</details>
</details>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Ресурсы" href="device-profile.md#Ресурсы">resources []</a></summary>
<details open><summary><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">элемент массива {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">id</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">kind</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">address</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">space</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">width_bits</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">access</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">write_mask</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Кэш ресурса" href="device-profile.md#Кэш%20ресурса">cache {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Кэш ресурса" href="device-profile.md#Кэш%20ресурса">mode</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Проверка записи ресурса" href="device-profile.md#Проверка%20записи%20ресурса">verify {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Проверка записи ресурса" href="device-profile.md#Проверка%20записи%20ресурса">mode</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Обновление ресурса" href="device-profile.md#Обновление%20ресурса">refresh {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Обновление ресурса" href="device-profile.md#Обновление%20ресурса">on []</a></p>
<p><a class="internal-link" data-href="device-profile.md#Обновление ресурса" href="device-profile.md#Обновление%20ресурса">period_ms</a></p>
</details>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">persist_mask</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">read_effect {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">clear_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">latch_high_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">latch_low_mask</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">write_effect {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">one_clear_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">one_set_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">self_clear_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">fixed_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">fixed_value</a></p>
</details>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">fields []</a></p>
</details>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Общие свойства" href="device-profile.md#Общие%20свойства">defaults {}</a></summary>
<details><summary><a class="internal-link" data-href="device-profile.md#Общие свойства регистра" href="device-profile.md#Общие%20свойства%20регистра">register {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">width_bits</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">access</a></p>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">write_mask</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Кэш ресурса" href="device-profile.md#Кэш%20ресурса">cache {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Кэш ресурса" href="device-profile.md#Кэш%20ресурса">mode</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Проверка записи ресурса" href="device-profile.md#Проверка%20записи%20ресурса">verify {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Проверка записи ресурса" href="device-profile.md#Проверка%20записи%20ресурса">mode</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Обновление ресурса" href="device-profile.md#Обновление%20ресурса">refresh {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Обновление ресурса" href="device-profile.md#Обновление%20ресурса">on []</a></p>
<p><a class="internal-link" data-href="device-profile.md#Обновление ресурса" href="device-profile.md#Обновление%20ресурса">period_ms</a></p>
</details>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">persist_mask</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">read_effect {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">clear_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">latch_high_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты чтения" href="device-profile.md#Эффекты%20чтения">latch_low_mask</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">write_effect {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">one_clear_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">one_set_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">self_clear_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">fixed_mask</a></p>
<p><a class="internal-link" data-href="device-profile.md#Эффекты записи" href="device-profile.md#Эффекты%20записи">fixed_value</a></p>
</details>
<p><a class="internal-link" data-href="device-profile.md#Описание регистра" href="device-profile.md#Описание%20регистра">fields []</a></p>
</details>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Политика" href="device-profile.md#Политика">policy {}</a></summary>
<details><summary><a class="internal-link" data-href="device-profile.md#Политика чтения" href="device-profile.md#Политика%20чтения">read {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Политика чтения" href="device-profile.md#Политика%20чтения">mode</a></p>
<p><a class="internal-link" data-href="device-profile.md#Политика чтения" href="device-profile.md#Политика%20чтения">rules []</a></p>
<p><a class="internal-link" data-href="device-profile.md#Политика чтения" href="device-profile.md#Политика%20чтения">inactive_rules []</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Политика записи" href="device-profile.md#Политика%20записи">write {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Политика записи" href="device-profile.md#Политика%20записи">mode</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Активные правила" href="device-profile.md#Активные%20правила">rules []</a></summary>
<details open><summary><a class="internal-link" data-href="device-profile.md#Правило записи" href="device-profile.md#Правило%20записи">элемент массива {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Правило записи" href="device-profile.md#Правило%20записи">resource</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Условие значения" href="device-profile.md#Условие%20значения">match {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Условие значения" href="device-profile.md#Условие%20значения">value</a></p>
<p><a class="internal-link" data-href="device-profile.md#Условие значения" href="device-profile.md#Условие%20значения">mask</a></p>
</details>
</details>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Неактивные правила" href="device-profile.md#Неактивные%20правила">inactive_rules []</a></summary>
<details open><summary><a class="internal-link" data-href="device-profile.md#Правило записи" href="device-profile.md#Правило%20записи">элемент массива {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Правило записи" href="device-profile.md#Правило%20записи">resource</a></p>
<details><summary><a class="internal-link" data-href="device-profile.md#Условие значения" href="device-profile.md#Условие%20значения">match {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Условие значения" href="device-profile.md#Условие%20значения">value</a></p>
<p><a class="internal-link" data-href="device-profile.md#Условие значения" href="device-profile.md#Условие%20значения">mask</a></p>
</details>
</details>
</details>
</details>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Начальные настройки" href="device-profile.md#Начальные%20настройки">settings {}</a></summary>
<details><summary><a class="internal-link" data-href="device-profile.md#Начальное заполнение кэша" href="device-profile.md#Начальное%20заполнение%20кэша">cache_seed []</a></summary>
<details open><summary><a class="internal-link" data-href="device-profile.md#Начальное заполнение кэша" href="device-profile.md#Начальное%20заполнение%20кэша">элемент массива {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Начальное заполнение кэша" href="device-profile.md#Начальное%20заполнение%20кэша">resource</a></p>
<p><a class="internal-link" data-href="device-profile.md#Начальное заполнение кэша" href="device-profile.md#Начальное%20заполнение%20кэша">value</a></p>
</details>
</details>
<p><a class="internal-link" data-href="device-profile.md#Начальные настройки" href="device-profile.md#Начальные%20настройки">desired []</a></p>
<p><a class="internal-link" data-href="device-profile.md#Начальные настройки" href="device-profile.md#Начальные%20настройки">apply_on_startup</a></p>
</details>
<details><summary><a class="internal-link" data-href="device-profile.md#Наблюдение" href="device-profile.md#Наблюдение">observability {}</a></summary>
<p><a class="internal-link" data-href="device-profile.md#Наблюдение" href="device-profile.md#Наблюдение">telemetry</a></p>
<p><a class="internal-link" data-href="device-profile.md#Наблюдение" href="device-profile.md#Наблюдение">events</a></p>
</details>
<p><a class="internal-link" data-href="device-profile.md#Карта регистров" href="device-profile.md#Карта%20регистров">register_map</a></p>
<p><a class="internal-link" data-href="device-profile.md#Индивидуальная форма" href="device-profile.md#Индивидуальная%20форма">view</a></p>
<p><a class="internal-link" data-href="device-profile.md#Пиктограмма" href="device-profile.md#Пиктограмма">icon</a></p>
</details>
</div>

`{}` — объект, `[]` — массив. Стрелка раскрывает ветку; название открывает описание поля.
