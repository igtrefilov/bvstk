"""Configuration figures: shared layout conventions, generic device examples."""
import json
import xml.etree.ElementTree as ET

import generate_pl_subsystem as drawing
from generate_pl_subsystem import ARROWS, NS, PALETTE, Box, Page, el
from transaction_drawing import owned

SOURCES = (
    'src/shared/config/bvstk_device_profile.c',
    'src/shared/config/bvstk_config_model.h',
    'src/apps/freertos/config/config_store.c',
    'src/apps/freertos/runtime/bvstk_runtime.c',
    'src/apps/neutrino/config/bvstk_i2c_config_store.c',
    'src/apps/neutrino/runtime/bvstk_i2c_runtime.c',
    'src/apps/freertos/services/http/http_ui_bus.c',
    'web/assets/js/register-map.js',
    'web/assets/js/device-view.js',
)


class ConfigurationFigure(Page):
    def __init__(self, name, title, width=1800):
        super().__init__('configuration/' + name + '.svg', title, title, SOURCES)
        self.width = width
        self.root.set('width', str(width))
        self.background.set('width', str(width))
        self.legend_note = 'Лист — файл. Белая карточка — данные. Цвет компонента — уровень абстракции.'

    def data(self, key, x, y, w, h, title, detail='', parent=None, file=False):
        owner = self.nodes[parent].group if parent else self.blocks
        g = el(owner, 'g', id=key, data_kind='artifact' if file else 'data', data_parent=parent)
        if file:
            el(g, 'path', d=f'M{x},{y} H{x+w-26} L{x+w},{y+26} V{y+h} H{x} Z',
               fill='white', stroke='#607487', stroke_width=2)
            el(g, 'path', d=f'M{x+w-26},{y} V{y+26} H{x+w}',
               fill='none', stroke='#607487', stroke_width=2)
        else:
            el(g, 'rect', x=x, y=y, width=w, height=h, rx=8,
               fill='white', stroke='#9baab6', stroke_width=1.8)
        self.text(g, x+22, y+43, title, 25, bold=True, width=w-48)
        if detail:
            self.text(g, x+22, y+82, detail, 23, width=w-44, line_height=32)
        self.nodes[key] = Box(key, x, y, w, h, g, title)
        self.min_y = min(self.min_y, y)
        self.max_y = max(self.max_y, y+h)
        return key

    def folder(self, key, x, y, w, h, title, parent=None):
        owner = self.nodes[parent].group if parent else self.blocks
        g = el(owner, 'g', id=key, data_kind='directory', data_parent=parent)
        el(g, 'rect', x=x, y=y, width=w, height=h, rx=8,
           fill='white', stroke='#9baab6', stroke_width=1.8)
        # Keep the folder symbol compact; the card width follows the label.
        cy = y + h / 2
        el(g, 'path', d=f'M{x+18},{cy-20} h22 l8,7 h26 v34 h-56 Z',
           fill='#f7f9fb', stroke='#607487', stroke_width=2)
        self.text(g, x+92, cy+9, title, 26, bold=True, width=w-108)
        self.nodes[key] = Box(key, x, y, w, h, g, title)
        self.min_y = min(self.min_y, y)
        self.max_y = max(self.max_y, y+h)
        return key

    def branch(self, points):
        el(self.edges, 'path',
           d='M' + ' L'.join(f'{x:g},{y:g}' for x, y in points),
           fill='none', stroke='#9baab6', stroke_width=2.5,
           stroke_linejoin='round', data_kind='directory-membership')

    def save(self):
        # Other documentation modules register extra arrow types. Keep this
        # series identical whether generated alone or from the main entry point.
        defs = self.root.find(f'{{{NS}}}defs')
        used_markers = {f'{kind}-{suffix}' for kind in self.used_arrows
                        for suffix in ('arrow', 'source')}
        for marker in list(defs.findall(f'{{{NS}}}marker')):
            if marker.get('id') not in used_markers:
                defs.remove(marker)
        super().save()
        path = drawing.OUT / self.name
        tree = ET.parse(path)
        metadata = tree.getroot().find(f'{{{NS}}}metadata')
        data = json.loads(metadata.text)
        data['generator'] = 'tools/docs/configuration_diagrams.py'
        metadata.text = json.dumps(data, ensure_ascii=False, indent=2)
        tree.write(path, encoding='utf-8', xml_declaration=True)


def devices():
    p = ConfigurationFigure('devices', 'Устройства на шинах и их пакеты', width=1920)
    p.frame('bvstk', 60, 90, 310, 930, 'BVSTK', None)
    p.node('port_a', 105, 340, 220, 120, 'Порт A', layer='L0', parent='bvstk')
    p.node('port_b', 105, 830, 220, 120, 'Порт B', layer='L0', parent='bvstk')
    p.frame('files', 1250, 90, 610, 930, 'Пакеты в BVSTK', None)
    color = PALETTE['L0'][1]
    bus_a = el(p.edges, 'g', id='bus_a', data_kind='bus', data_layer='L0')
    for path in ('M325,400 H520', 'M520,190 V610',
                 'M520,190 H670', 'M520,400 H670', 'M520,610 H670'):
        el(bus_a, 'path', d=path, fill='none', stroke=color, stroke_width=4)
    for cy in (190, 400, 610):
        el(bus_a, 'circle', cx=520, cy=cy, r=5, fill=color)
    p.text(p.notes, 480, 155, 'Шина A', 24, bold=True, color=color)
    el(p.edges, 'path', id='bus_b', d='M325,890 H670', fill='none',
       stroke=color, stroke_width=4, data_kind='bus', data_layer='L0')
    p.text(p.notes, 480, 860, 'Шина B', 24, bold=True, color=color)
    for name, y in (('a', 120), ('b', 330), ('c', 540), ('d', 820)):
        p.node('device_' + name, 670, y, 340, 140, 'device_' + name, layer='L0')
        p.folder('package_' + name, 1310, y+20, 490, 100,
                 'device_' + name, parent='files')
        p.edge('package_' + name, 'device_' + name, 'ref', start='W', end='E')
    p.legend_note = 'Сплошные линии — шины. Пунктир связывает пакет с описываемым устройством.'
    p.save()


def storage():
    p = ConfigurationFigure('storage', 'Каталоги и файлы описания устройств', width=1920)
    p.folder('root', 60, 500, 240, 100, 'config')
    p.folder('subsystem_a', 420, 310, 390, 100, '<подсистема_a>')
    p.folder('subsystem_b', 420, 900, 390, 100, '<подсистема_b>')
    for key, y in (('a', 182.5), ('b', 410), ('c', 630), ('d', 900)):
        p.folder('dir_' + key, 930, y, 310, 100, 'device_' + key)
    # The files are expanded for every device. Optional files produce different
    # branch sizes, reflecting an actual package rather than a fixed template.
    for key, y, name in (
        ('a_profile', 60, 'device_a.json'),
        ('a_map', 190, 'register_map.json'),
        ('a_view', 320, 'view.js'),
        ('b_profile', 417.5, 'device_b.json'),
        ('c_profile', 572.5, 'device_c.json'),
        ('c_map', 702.5, 'register_map.json'),
        ('d_profile', 842.5, 'device_d.json'),
        ('d_map', 972.5, 'register_map.json'),
    ):
        p.data(key, 1390, y, 470, 85, name, file=True)
    p.branch(((300, 550), (355, 550)))
    p.branch(((355, 360), (355, 950)))
    p.branch(((355, 360), (420, 360)))
    p.branch(((355, 950), (420, 950)))
    p.branch(((810, 360), (870, 360)))
    p.branch(((870, 232.5), (870, 680)))
    for cy in (232.5, 460, 680):
        p.branch(((870, cy), (930, cy)))
    p.branch(((810, 950), (930, 950)))
    # Separate vertical lanes prevent one device's branch from merging with
    # the next. These are containment lines, not runtime transfers.
    for key, ys in (('a', (102.5, 232.5, 362.5)), ('b', (460,)),
                    ('c', (615, 745)), ('d', (885, 1015))):
        _, cy = p.nodes['dir_' + key].point('E')
        p.branch(((1240, cy), (1300, cy)))
        p.branch(((1300, min(cy, min(ys))), (1300, max(cy, max(ys)))))
        for fy in ys:
            p.branch(((1300, fy), (1390, fy)))
    p.legend_note = 'Значок папки — каталог, лист — файл. Линии показывают вложенность. Карта и форма необязательны.'
    p.save()


def package():
    p = ConfigurationFigure('package', 'Файлы пакета и их потребители')
    p.frame('folder', 60, 100, 590, 870, 'Пакет device_a', None)
    p.data('profile', 140, 185, 460, 245, 'device_a.json',
           'Подключение и политика\nРегистры и настройки\nregister_map\nview', parent='folder', file=True)
    p.data('map', 140, 525, 460, 145, 'register_map.json', 'Регистры и поля', parent='folder', file=True)
    p.data('view', 140, 790, 460, 135, 'view.js', 'Форма', parent='folder', file=True)
    p.path('profile', 'map', ((140, 331), (98, 331), (98, 597), (140, 597)), 'ref')
    p.path('profile', 'view', ((140, 363), (78, 363), (78, 857), (140, 857)), 'ref')
    p.node('service', 1030, 210, 660, 175, 'Сервис PS', layer='L3')
    p.frame('browser', 950, 460, 800, 510, 'Веб-интерфейс', 'L4')
    p.node('decode', 1030, 525, 660, 145, 'Расшифровка кэша', layer='L4', parent='browser')
    p.node('form', 1030, 790, 660, 135, 'Индивидуальная форма', layer='L4', parent='browser')
    p.edge('profile', 'service', 'data', 'Настройки', start='E', end='W', a_offset=-10, label_xy=(820, 270))
    p.edge('map', 'decode', 'data', start='E', end='W')
    p.edge('view', 'form', 'data', start='E', end='W')
    p.edge('service', 'decode', 'data', 'Снимок кэша', start='S', end='N', label_xy=(1480, 430))
    p.edge('decode', 'form', 'data', 'Показатели', start='S', end='N', label_xy=(1480, 735))
    p.legend_note = 'Листы — файлы пакета. Карта и форма необязательны. Профиль передаётся сервису при загрузке.'
    p.save()


def loading():
    p = ConfigurationFigure('loading', 'Чтение профиля и общий код')
    owned(p, 'freertos', 50, 100, 470, 650, 'FreeRTOS')
    owned(p, 'neutrino', 1280, 100, 470, 650, 'Нейтрино')
    p.node('ft_store', 90, 210, 390, 170, 'config_store', 'Чтение файлов', layer='L4', parent='freertos')
    p.node('nt_store', 1320, 210, 390, 170, 'Хранилище профилей', 'Чтение файлов', layer='L4', parent='neutrino')
    p.node('codec', 665, 210, 470, 170, 'Общий кодек', 'Проверка и разбор', layer='L4')
    p.data('model', 690, 530, 420, 150, 'Модель конфигурации', 'Параметры в памяти')
    p.node('ft_runtime', 90, 530, 390, 150, 'Код запуска', layer='L4', parent='freertos')
    p.node('nt_runtime', 1320, 530, 390, 150, 'Код запуска', layer='L4', parent='neutrino')
    p.node('service', 665, 880, 470, 170, 'Общий сервис', 'Экземпляр устройства', layer='L3')
    p.edge('ft_store', 'codec', 'data', 'JSON', start='E', end='W')
    p.edge('nt_store', 'codec', 'data', 'JSON', start='W', end='E')
    p.edge('codec', 'model', 'data', start='S', end='N')
    p.edge('model', 'ft_runtime', 'data', start='W', end='E')
    p.edge('model', 'nt_runtime', 'data', start='E', end='W')
    p.path('ft_runtime', 'service', ((285, 680), (285, 965), (665, 965)), 'call',
           'Создать', label_xy=(480, 945))
    p.path('nt_runtime', 'service', ((1515, 680), (1515, 965), (1135, 965)), 'call',
           'Создать', label_xy=(1325, 945))
    p.legend_note = 'Пунктир — части ОС. Общий код используется раздельно: каждая программа создаёт свои экземпляры.'
    p.save()


def state():
    p = ConfigurationFigure('state', 'Сохранённый профиль и рабочие настройки', width=1700)
    p.data('file', 70, 230, 420, 220, 'device_a.json', 'Сохранённый профиль', file=True)
    p.node('service', 1000, 230, 600, 220, 'Рабочие настройки', 'Сервис устройства', layer='L3')
    p.node('editor', 1000, 760, 600, 175, 'Черновик', 'Редактор настроек', layer='L4')
    p.edge('file', 'service', 'data', 'Загрузить', start='E', end='W',
           a_offset=-50, b_offset=-50, label_xy=(745, 270))
    p.edge('service', 'file', 'data', 'Сохранить', start='W', end='E',
           a_offset=50, b_offset=50, label_xy=(745, 425))
    p.edge('editor', 'service', 'call', 'Применить', start='N', end='S',
           a_offset=-110, b_offset=-110, label_xy=(1060, 610))
    p.edge('service', 'editor', 'data', 'Получить настройки', start='S', end='N',
           a_offset=110, b_offset=110, label_xy=(1545, 650))
    p.legend_note = 'Лист — файл. Загрузка и сохранение проходят через кодек и слой хранения.'
    p.save()


def cache():
    p = ConfigurationFigure('cache', 'Начальное значение и результат обмена')
    p.data('seed', 60, 160, 450, 165, 'device_a.json', 'cache_seed: reg_01 = 0x00', file=True)
    p.node('device', 60, 600, 450, 145, 'Устройство', 'Чтение вернуло 0x01', layer='L0')
    p.frame('cache', 670, 85, 470, 710, 'Кэш сервиса', 'L3')
    p.data('initial', 710, 160, 390, 165, 'reg_01 = 0x00', 'source: settings', parent='cache')
    p.data('actual', 710, 590, 390, 165, 'reg_01 = 0x01', 'Результат обмена', parent='cache')
    p.frame('ui', 1280, 85, 460, 710, 'Веб-интерфейс', 'L4')
    p.data('unknown', 1320, 160, 380, 165, 'ready', 'Нет данных\nот устройства', parent='ui')
    p.data('known', 1320, 605, 380, 135, 'ready', 'Готово', parent='ui')
    p.edge('seed', 'initial', 'data', 'При загрузке', start='E', end='W')
    p.edge('device', 'actual', 'data', 'После чтения', start='E', end='W')
    p.edge('initial', 'actual', 'data', 'Обновление', start='S', end='N', label_xy=(1010, 465))
    p.edge('initial', 'unknown', 'data', start='E', end='W')
    p.edge('actual', 'known', 'data', start='E', end='W')
    p.legend_note = 'Два состояния записи кэша. Справа — результат расшифровки по карте.'
    p.save()


def profile():
    p = ConfigurationFigure('profile', 'Состав профиля устройства')
    p.data('file', 60, 90, 1680, 1035, 'device_a.json', file=True)
    p.data('identity', 100, 185, 450, 155, 'id и device', 'Экземпляр и описание', parent='file')
    p.data('connection', 100, 415, 450, 220, 'Подключение', 'transport\nprotocol', parent='file')
    p.data('presentation', 100, 745, 450, 175, 'Отображение', 'register_map\nview', parent='file')
    p.data('defaults', 690, 185, 370, 155, 'defaults.register', 'Общие свойства', parent='file')
    p.data('resources', 690, 445, 370, 380, 'resources', parent='file')
    p.data('reg00', 720, 515, 310, 105, 'reg_00', 'address: 0x00', parent='resources')
    p.data('reg01', 720, 660, 310, 125, 'reg_01', 'address: 0x01', parent='resources')
    p.data('policy', 1200, 445, 490, 190, 'policy', 'Правила операций\nresource: reg_01', parent='file')
    p.data('settings', 1200, 690, 490, 165, 'settings.cache_seed', 'resource: reg_01\nvalue: 0x00', parent='file')
    p.data('observe', 690, 960, 1000, 115, 'observability', 'Публикация наблюдений и событий', parent='file')
    p.edge('defaults', 'resources', 'ref', start='S', end='N')
    p.path('policy', 'reg01', ((1200, 570), (1140, 570), (1140, 700), (1030, 700)), 'ref')
    p.path('settings', 'reg01', ((1200, 750), (1030, 750)), 'ref')
    # File and data cards have opaque backgrounds. References must remain visible
    # over their enclosing JSON containers, including the resource array.
    p.root.remove(p.edges)
    p.root.insert(list(p.root).index(p.notes), p.edges)
    p.legend_note = 'Вложенность — состав JSON. Пунктирная стрелка — связь данных. Показана часть полей.'
    p.save()


def defaults():
    p = ConfigurationFigure('defaults', 'Общие свойства ресурса', width=1600)
    p.data('defaults', 60, 185, 490, 200, 'defaults.register',
           'access: rw\ncache.mode: driver\nverify.mode: driver')
    p.data('resource', 60, 455, 490, 200, 'resources: reg_01',
           'id: reg_01\nkind: register\naddress: 0x01')
    p.data('resolved', 920, 190, 610, 400, 'Описание reg_01',
           'access: rw\ncache.mode: driver\nverify.mode: driver\n\nid: reg_01\nkind: register\naddress: 0x01')
    p.path('defaults', 'resolved', ((550, 285), (920, 285)), 'ref')
    p.path('resource', 'resolved', ((550, 555), (920, 555)), 'ref')
    p.legend_note = 'Белые карточки — данные. Показан фрагмент; разрядность и маска задаются отдельно.'
    p.save()


def decode():
    p = ConfigurationFigure('decode', 'Расшифровка битового поля')
    p.data('map', 60, 110, 480, 180, 'register_map.json', 'reg_01.ready\nlsb: 0, width: 1', file=True)
    p.data('cache', 60, 460, 480, 180, 'Кэш: регистр 0x01', '0x01 после чтения')
    p.frame('browser', 660, 70, 1080, 650, 'Веб-интерфейс', 'L4')
    p.node('decode', 710, 160, 430, 420, 'Выделение поля', layer='L4', parent='browser')
    # Replace the centred node title with a top heading to leave room for bits.
    g = p.nodes['decode'].group
    for element in list(g):
        if element.tag == f'{{{NS}}}text' and element.get('font-size') != '14':
            g.remove(element)
    p.text(g, 730, 225, 'Выделение поля', 25, bold=True)
    p.text(g, 740, 285, 'Старшие биты', 19)
    for index, (value, label) in enumerate((('…', ''), ('0', '2'), ('0', '1'), ('1', '0'))):
        x = 740 + index * 90
        p.text(g, x+35, 325, label, 19, anchor='middle')
        el(g, 'rect', x=x, y=345, width=70, height=70, rx=6,
           fill='#d9ebff' if index == 3 else 'white', stroke='#3f75ad' if index == 3 else '#9baab6')
        p.text(g, x+35, 392, value, 29, bold=index == 3, anchor='middle')
    p.text(g, 740, 485, 'ready = 1', 28, bold=True)
    p.data('label', 1300, 300, 380, 140, 'Показатель ready', 'Готово', parent='browser')
    p.edge('map', 'decode', 'data', start='E', end='W', b_offset=-170)
    p.edge('cache', 'decode', 'data', start='E', end='W', b_offset=180)
    p.edge('decode', 'label', 'data', 'values', start='E', end='W', label_xy=(1220, 350))
    p.legend_note = 'Показан младший бит условного регистра. С кэшем карта связывается по адресу.'
    p.save()


def view():
    p = ConfigurationFigure('view', 'Создание формы и обновление снимков', width=1920)
    p.node('ui', 60, 145, 390, 120, 'Веб-интерфейс', layer='L4')
    p.frame('iframe', 670, 65, 1190, 1140, 'Изолированная область iframe', 'L4')
    p.node('bridge', 730, 145, 430, 120, 'Среда формы', layer='L4', parent='iframe')
    p.node('form', 1350, 145, 430, 120, 'view.js', layer='L4', parent='iframe')
    for x in (255, 945, 1565):
        el(p.under, 'path', d=f'M{x},265 V1160', fill='none', stroke='#9baab6', stroke_width=2, stroke_dasharray='4 7')
    p.path('bridge', 'form', ((945, 350), (1565, 350)), 'call', 'mount(root)', label_xy=(1255, 326))
    p.text(p.notes, 1585, 395, 'Один раз', 21)
    for y in (500, 855):
        p.path('ui', 'bridge', ((255, y), (945, y)), 'data', 'Снимок', label_xy=(560, y-22))
        p.path('bridge', 'form', ((945, y+70), (1565, y+70)), 'call', 'update(snapshot)', label_xy=(1255, y+45))
    p.data('unknown', 1380, 620, 370, 115, 'ready', 'Нет данных', parent='iframe')
    p.data('known', 1380, 975, 370, 115, 'ready', 'Готово', parent='iframe')
    p.legend_note = 'Время идёт сверху вниз. Снимки передаются через postMessage; вызовы формы выполняются внутри iframe.'
    p.save()


def generate():
    previous = ARROWS.get('ref')
    ARROWS['ref'] = ('#788794', 'Связь данных', '4 5')
    figures = (devices, package, storage, loading, state, cache, profile, defaults, decode, view)
    try:
        for figure in figures:
            figure()
    finally:
        if previous is None:
            del ARROWS['ref']
        else:
            ARROWS['ref'] = previous
    return len(figures)


if __name__ == '__main__':
    drawing.OUT = drawing.ROOT / 'docs/figures'
    print(f'Generated {generate()} configuration figures')
