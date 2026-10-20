"""Field provenance and worked configuration examples in the shared SVG style."""
from generate_pl_subsystem import el
from transaction_drawing import owned


def front_edges(p):
    """Expose references drawn inside opaque JSON and file containers."""
    p.root.remove(p.edges)
    p.root.insert(list(p.root).index(p.notes), p.edges)


def policy_structure(Figure):
    p = Figure('policy-structure', 'Вложенность политики и настроек профиля', width=1900)
    p.data('file', 60, 80, 1780, 1100, 'device_a.json {}', file=True)
    p.data('policy', 110, 170, 950, 950, 'policy {}', parent='file')
    p.data('read', 150, 255, 870, 150, 'read {}',
           'mode: blacklist\nrules: []', parent='policy')
    p.data('write', 150, 455, 870, 620, 'write {}', 'mode: whitelist', parent='policy')
    p.data('rules', 190, 610, 470, 420, 'rules []', parent='write')
    p.data('rule', 225, 690, 400, 320, 'Элемент {}', 'resource: reg_03', parent='rules')
    p.data('match', 260, 835, 330, 145, 'match {}',
           'value: 0x10\nmask: 0xFF', parent='rule')
    p.data('inactive', 700, 610, 280, 125, 'inactive_rules[]', '…', parent='write')
    p.data('resources', 1230, 170, 560, 300, 'resources []', parent='file')
    p.data('resource', 1270, 255, 480, 170, 'Элемент {}',
           'id: reg_03\naddress: 0x03', parent='resources')
    p.data('settings', 1230, 575, 560, 545, 'settings {}',
           'desired: []\napply_on_startup: false', parent='file')
    p.data('seeds', 1270, 750, 480, 320, 'cache_seed []', parent='settings')
    p.data('seed', 1310, 830, 400, 190, 'Элемент {}',
           'resource: reg_03\nvalue: 0x10', parent='seeds')
    p.path('rule', 'resource', ((425, 690), (425, 565), (1140, 565), (1140, 340), (1270, 340)), 'ref')
    p.path('seed', 'resource', ((1590, 830), (1590, 425)), 'ref')
    front_edges(p)
    p.legend_note = 'Вложенность — состав JSON. {} — объект, [] — массив. Пунктир — ссылка resource на id. Показаны фрагменты.'
    p.save()


def field_origins(Figure):
    p = Figure('field-origins', 'Происхождение полей при разборе профиля', width=1760)
    p.data('file', 60, 80, 590, 850, 'device_a.json', file=True)
    p.data('explicit', 100, 180, 510, 180, 'Задано автором',
           'id: device_a\nresources: 16 регистров', parent='file')
    p.data('common', 100, 440, 510, 165, 'defaults.register',
           'access: rw\ncache.mode: driver', parent='file')
    p.data('device', 100, 695, 510, 150, 'device',
           'model: Условное устройство', parent='file')
    p.frame('codec', 870, 80, 830, 850, 'Общий кодек профиля', 'L4')
    p.data('model', 910, 180, 750, 180, 'Модель конфигурации',
           'name: device_a\nreg_count: 16', parent='codec')
    p.data('resolved', 910, 440, 750, 165, 'Свойства ресурсов при проверке',
           'access: rw\ncache.mode: driver', parent='codec')
    p.data('implicit', 910, 695, 750, 150, 'Класс устройства',
           'generic', parent='codec')
    for a, b, label in [('explicit', 'model', 'Разбор'),
                        ('common', 'resolved', 'Общие свойства'),
                        ('device', 'implicit', 'class отсутствует')]:
        p.edge(a, b, 'data', label, start='E', end='W')
    front_edges(p)
    p.legend_note = 'Лист — файл. Белые карточки — фрагменты данных; справа показан результат обработки.'
    p.save()


def runtime_fields(Figure):
    p = Figure('runtime-fields', 'Профиль, рабочее состояние и поля ответа', width=1800)
    p.data('file', 60, 155, 440, 180, 'device_a.json',
           'Политика\nsettings.cache_seed', file=True)
    p.frame('service', 660, 80, 1080, 330, 'Сервис устройства', 'L3')
    p.data('config', 700, 155, 400, 180, 'Рабочие настройки',
           'Режим и правила\nНачальные значения', parent='service')
    p.data('cache', 1270, 155, 430, 180, 'Кэш',
           'val, valid\nsource, updated_ms', parent='service')
    p.edge('file', 'config', 'data', 'Загрузка', start='E', end='W')
    p.edge('config', 'cache', 'data', 'Начальный кэш', start='E', end='W')
    owned(p, 'freertos', 610, 505, 1130, 350, 'FreeRTOS')
    p.frame('http', 660, 570, 1040, 245, 'Ответ HTTP', 'L4', parent='freertos')
    p.data('projection', 700, 650, 400, 125, 'config, revision, saved',
           'Представление настроек', parent='http')
    p.data('state', 1270, 650, 390, 125, 'cache, presence',
           'Состояние устройства', parent='http')
    p.path('config', 'projection', ((900, 335), (900, 650)), 'data')
    p.path('cache', 'state', ((1485, 335), (1485, 650)), 'data')
    p.path('file', 'projection', ((280, 335), (280, 712.5), (700, 712.5)),
           'data', 'Сверка saved', label_xy=(465, 690))
    p.data('map', 60, 1020, 440, 180, 'register_map.json',
           'Поля и подписи\nПравила сводки', file=True)
    p.frame('browser', 660, 980, 1080, 300, 'Расшифровка в веб-интерфейсе', 'L4')
    p.data('decoded', 700, 1060, 1000, 150, 'fields, summary, identity',
           'Результат применения карты к снимку кэша', parent='browser')
    p.edge('map', 'decoded', 'data', start='E', end='W', b_offset=-25)
    p.path('state', 'decoded', ((1485, 775), (1485, 1060)), 'data',
           'Снимок', label_xy=(1570, 920))
    p.legend_note = 'Листы — файлы. Белые карточки — данные. Пунктирная область — часть FreeRTOS.'
    p.save()


def map_structure(Figure):
    p = Figure('map-structure', 'Вложенные разделы карты регистров', width=1800)
    p.data('file', 60, 80, 1680, 910, 'register_map.json', file=True)
    p.data('format', 110, 175, 790, 145, 'Формат карты',
           'schema_version: 1\nkind: register_map', parent='file')
    p.data('device', 1120, 175, 570, 145, 'Согласование с профилем',
           'bus, width_bits\nregister_count', parent='file')
    p.data('registers', 110, 385, 790, 540, 'registers[]', parent='file')
    p.data('register', 155, 470, 700, 410, 'id: status',
           'address: 0x01', parent='registers')
    p.data('fields', 200, 610, 610, 225, 'fields[]', parent='register')
    p.data('ready', 245, 680, 520, 120, 'id: ready',
           'lsb: 0, width: 1', parent='fields')
    p.data('summary', 1120, 635, 570, 210, 'summary[]',
           'id: ready\nfield: status.ready', parent='file')
    p.path('summary', 'ready', ((1120, 740), (765, 740)), 'ref')
    front_edges(p)
    p.legend_note = 'Вложенность — состав JSON. Пунктир — ссылка на поле внутри карты. Показан фрагмент.'
    p.save()


def package_assembly(Figure):
    p = Figure('examples/package-assembly', 'Добавление карты и формы в пакет', width=1900)
    for key, x, height in [('a', 60, 410), ('b', 670, 735), ('c', 1280, 970)]:
        p.frame(key, x, 80, 560 if key == 'c' else 520, height, 'device_a', None)
    p.data('profile_a', 100, 165, 440, 270, 'device_a.json',
           'id: device_a\ntransport, protocol\nresources, policy', parent='a', file=True)
    p.data('profile_b', 710, 165, 440, 270, 'device_a.json',
           '…\nregister_map:\n  register_map.json', parent='b', file=True)
    p.data('map_b', 710, 595, 440, 160, 'register_map.json',
           'registers\nsummary', parent='b', file=True)
    p.edge('profile_b', 'map_b', 'ref', start='S', end='N')
    p.data('profile_c', 1320, 165, 440, 350, 'device_a.json',
           '…\nregister_map:\n  register_map.json\nview: view.js', parent='c', file=True)
    p.data('map_c', 1320, 620, 440, 160, 'register_map.json',
           'registers\nsummary', parent='c', file=True)
    p.data('view_c', 1320, 890, 440, 110, 'view.js',
           'mount, update', parent='c', file=True)
    p.path('profile_c', 'map_c', ((1480, 515), (1480, 620)), 'ref')
    p.path('profile_c', 'view_c', ((1760, 430), (1800, 430), (1800, 945), (1760, 945)), 'ref')
    p.legend_note = 'Три варианта одного пакета. Листы — файлы; пунктир — ссылки из профиля.'
    p.save()


def rule_binding(Figure):
    p = Figure('examples/rule-binding', 'Связь ресурса с политикой и начальным кэшем', width=1740)
    p.data('file', 60, 80, 1620, 580, 'device_a.json', file=True)
    p.data('resources', 610, 180, 520, 185, 'resources[]',
           'reg_00 → address 0x00\nreg_01 → address 0x01\n… reg_0f → address 0x0F', parent='file')
    p.data('rule', 110, 440, 620, 175, 'policy.write.rules[]',
           'resource: reg_01\nvalue: 0x01, mask: 0xFF', parent='file')
    p.data('seed', 1010, 440, 620, 175, 'settings.cache_seed[]',
           'resource: reg_01\nvalue: 0x00', parent='file')
    p.path('rule', 'resources', ((420, 440), (420, 255), (610, 255)), 'ref')
    p.path('seed', 'resources', ((1320, 440), (1320, 255), (1130, 255)), 'ref')
    p.frame('service', 60, 790, 1620, 300, 'Сервис', 'L3')
    p.data('resolved', 110, 870, 620, 170, 'Правило белого списка',
           'reg: 0x01, val: 0x01\nСравнение по mask', parent='service')
    p.data('cache', 1010, 870, 620, 170, 'Начальное значение кэша',
           'reg: 0x01, val: 0x00\nsource: settings', parent='service')
    p.edge('rule', 'resolved', 'data', 'Разбор ссылки', start='S', end='N', label_xy=(520, 735))
    p.edge('seed', 'cache', 'data', 'Создание кэша', start='S', end='N', label_xy=(1430, 735))
    front_edges(p)
    p.legend_note = 'Пунктир — ссылка по ID. Синие стрелки — получение рабочих данных. Показаны фрагменты профиля.'
    p.save()


def profile_roundtrip(Figure):
    p = Figure('examples/profile-roundtrip', 'Поля профиля до и после сохранения', width=1860)
    p.data('before', 60, 80, 570, 920, 'device_a.json: исходный', file=True)
    p.data('device_before', 100, 180, 490, 165, 'device',
           'model: Условное устройство', parent='before')
    p.data('resource_before', 100, 450, 490, 240, 'resources[]: 16 регистров',
           'id, kind, address\nwidth_bits, access\nwrite_mask, cache, verify', parent='before')
    p.data('policy_before', 100, 810, 490, 120, 'policy.read',
           'blacklist, rules: []', parent='before')
    p.node('parse', 770, 220, 320, 130, 'Разобрать', layer='L4')
    p.data('model', 770, 470, 320, 200, 'Модель в RAM',
           'class: generic\nПустые списки', file=False)
    p.node('format', 770, 820, 320, 130, 'Сформировать JSON', layer='L4')
    p.data('after', 1230, 80, 570, 920, 'device_a.json: сохранённый', file=True)
    p.data('device_after', 1270, 180, 490, 165, 'device',
           'model: Условное устройство\nclass: generic', parent='after')
    p.data('defaults_after', 1270, 405, 490, 240, 'defaults и resources',
           'defaults.register: свойства\nresources: id, kind, address', parent='after')
    p.data('fixed_after', 1270, 710, 490, 240, 'Явно записанные разделы',
           'policy.write: whitelist, []\nsettings: пустые списки\nobservability\n  telemetry: true\n  events: true', parent='after')
    p.path('before', 'parse', ((630, 285), (770, 285)), 'data')
    p.edge('parse', 'model', 'data', start='S', end='N')
    p.edge('model', 'format', 'data', start='S', end='N')
    p.path('format', 'after', ((1090, 885), (1230, 885)), 'data')
    p.legend_note = 'Показаны изменившиеся фрагменты. Сохранение формирует JSON из модели, а не копирует исходный текст.'
    p.save()


def policy_edit(Figure):
    p = Figure('examples/policy-edit', 'Правило в файле, сервисе и черновике', width=1800)
    p.data('file', 325, 80, 310, 125, 'Профиль', 'device_a.json', file=True)
    p.node('service', 875, 80, 310, 125, 'Сервис', layer='L3')
    p.node('editor', 1425, 80, 310, 125, 'Редактор', layer='L4')
    intervals = [('file', 370, 930), ('service', 920, 645), ('editor', 1470, 420)]
    for key, x, change in intervals:
        el(p.edges, 'path', d=f'M{x+110},205 V275', fill='none', stroke='#9baab6', stroke_width=2)
        p.data(key+'_empty', x, 275, 220, change-275, 'Пусто')
        p.data(key+'_rule', x, change, 220, 1220-change, 'reg_01', '0x01')
        el(p.notes, 'path', d=f'M{x},{change} h220', stroke='#3f75ad', stroke_width=4)
    for y, label in [(320, 'Загрузка'), (470, 'Правка'), (700, 'Применение'), (985, 'Сохранение')]:
        p.text(p.notes, 60, y, label, 24, bold=True)
    p.path('file_empty', 'service_empty', ((590, 325), (920, 325)), 'data',
           'Загрузить', label_xy=(755, 305))
    p.path('service_empty', 'editor_empty', ((1140, 325), (1470, 325)), 'data',
           'Получить', label_xy=(1305, 305))
    p.path('editor_rule', 'service_empty', ((1470, 615), (1140, 615)), 'call',
           'Применить', label_xy=(1305, 595))
    p.path('service_rule', 'file_empty', ((920, 900), (590, 900)), 'data',
           'Сохранить', label_xy=(755, 880))
    p.legend_note = 'Время идёт сверху вниз. Показано содержимое белого списка: пустой либо правило reg_01 = 0x01.'
    p.save()


def map_binding(Figure):
    p = Figure('examples/map-binding', 'Связь ресурса профиля и поля карты', width=1800)
    p.data('profile', 60, 80, 460, 360, 'device_a.json',
           'register_map:\n  register_map.json\nresources[]\n  id: reg_01\n  address: 0x01', file=True)
    p.data('map', 700, 80, 540, 650, 'register_map.json', file=True)
    p.data('register', 740, 170, 460, 500, 'registers[]',
           'id: status\naddress: 0x01', parent='map')
    p.data('field', 780, 400, 380, 220, 'fields[]',
           'id: ready\nlsb: 0, width: 1\nvalues: 1 → Готово', parent='register')
    p.path('profile', 'map', ((520, 155), (700, 155)), 'ref',
           'Файл', label_xy=(610, 135))
    p.path('profile', 'register', ((520, 255), (740, 255)), 'ref',
           'Общий адрес', label_xy=(630, 230))
    p.data('cache', 60, 570, 460, 180, 'Снимок кэша',
           'reg: 1, val: 1\nПолучено с устройства')
    p.node('decode', 1430, 190, 310, 240, 'Расшифровка', 'По адресу и битам', layer='L4')
    p.data('result', 1430, 570, 310, 140, 'status.ready', 'Готово')
    p.path('map', 'decode', ((1240, 285), (1430, 285)), 'data')
    p.path('cache', 'decode', ((290, 750), (290, 850), (1330, 850), (1330, 385), (1430, 385)), 'data')
    p.edge('decode', 'result', 'data', start='S', end='N')
    front_edges(p)
    p.legend_note = 'reg_01 — ID ресурса профиля. status.ready — ссылка внутри карты. Регистры сопоставляются по адресу.'
    p.save()


def generate(Figure):
    figures = (policy_structure, field_origins, runtime_fields, map_structure, package_assembly,
               rule_binding, profile_roundtrip, policy_edit, map_binding)
    for figure in figures:
        figure(Figure)
    return len(figures)
