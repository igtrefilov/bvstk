"""HTTP guide figures: editing a revision and transferring a file."""
import json
import xml.etree.ElementTree as ET

import generate_pl_subsystem as drawing
from generate_pl_subsystem import Box, NS, Page, el
from transaction_drawing import owned


class HttpFigure(Page):
    def __init__(self, name, title, sources):
        super().__init__('interfaces/http/' + name + '.svg', title, title, sources)
        self.width = 1800
        self.root.set('width', '1800')
        self.background.set('width', '1800')
        self.legend_note = 'Время идёт сверху вниз.'

    def external(self, key, x, y, w, label, file=False):
        h = 120
        g = el(self.blocks, 'g', id=key, data_kind='artifact' if file else 'external-client')
        if file:
            self.legend_note = 'Время идёт сверху вниз. Лист обозначает файл.'
            el(g, 'path', d=f'M{x},{y} H{x+w-24} L{x+w},{y+24} V{y+h} H{x} Z',
               fill='white', stroke='#607487', stroke_width=2)
            el(g, 'path', d=f'M{x+w-24},{y} V{y+24} H{x+w}',
               fill='none', stroke='#607487', stroke_width=2)
        else:
            el(g, 'rect', x=x, y=y, width=w, height=h, rx=12,
               fill='white', stroke='#607487', stroke_width=2)
        self.text(g, x+w/2, y+70, label, 25, bold=True, anchor='middle')
        self.nodes[key] = Box(key, x, y, w, h, g, label)
        self.min_y = min(self.min_y, y)
        self.max_y = max(self.max_y, y+h)

    def lane(self, key, bottom):
        b = self.nodes[key]
        x = b.x+b.w/2
        el(self.under, 'path', d=f'M{x},{b.y+b.h+25} V{bottom}', fill='none',
           stroke='#bdc8d0', stroke_width=2, stroke_dasharray='5 8')
        self.max_y = max(self.max_y, bottom)

    def message(self, a, b, y, label, kind='data'):
        x1 = self.nodes[a].x+self.nodes[a].w/2
        x2 = self.nodes[b].x+self.nodes[b].w/2
        self.path(a, b, ((x1, y), (x2, y)), kind, label,
                  label_xy=((x1+x2)/2, y-22))

    def save(self):
        # Unused marker definitions must not depend on import order.
        defs = self.root.find(f'{{{NS}}}defs')
        used = {f'{kind}-{suffix}' for kind in self.used_arrows
                for suffix in ('arrow', 'source')}
        for marker in list(defs.findall(f'{{{NS}}}marker')):
            if marker.get('id') not in used:
                defs.remove(marker)
        super().save()
        path = drawing.OUT/self.name
        tree = ET.parse(path)
        metadata = tree.getroot().find(f'{{{NS}}}metadata')
        value = json.loads(metadata.text)
        value['generator'] = 'tools/docs/http_guides.py'
        metadata.text = json.dumps(value, ensure_ascii=False, indent=2)
        tree.write(path, encoding='utf-8', xml_declaration=True)


def device_settings():
    p = HttpFigure('device-settings', 'Редактирование настроек через HTTP', (
        'src/apps/freertos/services/http/http_ui_bus.c',
        'src/apps/freertos/config/config_store.c',
    ))
    p.external('client', 60, 130, 320, 'Клиент')
    owned(p, 'freertos', 460, 60, 400, 230, 'FreeRTOS')
    p.node('http', 490, 130, 340, 120, 'HTTP и ConfigStore', layer='L4', parent='freertos')
    p.node('service', 960, 130, 340, 120, 'Сервис устройства', layer='L3')
    p.external('profile', 1420, 130, 320, 'Профиль', file=True)
    for key in ('client', 'http', 'service', 'profile'):
        p.lane(key, 1480)
    p.message('client', 'http', 370, 'GET')
    p.message('http', 'client', 470, 'config + revision')
    p.message('client', 'http', 620, 'APPLY: черновик + revision')
    p.text(p.notes, 700, 695, 'Проверка revision', 23, color='#3f75ad')
    p.message('http', 'service', 790, 'Применить', 'call')
    p.message('http', 'client', 890, 'Результат применения')
    p.message('client', 'http', 1020, 'GET')
    p.message('http', 'client', 1120, 'config + новая revision')
    p.message('client', 'http', 1250, 'SAVE + revision')
    p.message('http', 'profile', 1350, 'Текущие настройки')
    p.message('http', 'client', 1450, 'Результат сохранения')
    p.save()


def file_transfer():
    p = HttpFigure('file-transfer', 'Загрузка и проверочное чтение файла', (
        'src/apps/freertos/services/http/http_ui_files.c',
    ))
    p.external('client', 120, 130, 340, 'Клиент')
    owned(p, 'freertos', 670, 60, 1010, 230, 'FreeRTOS')
    p.node('http', 710, 130, 340, 120, 'HTTP', layer='L4', parent='freertos')
    p.node('fs', 1300, 130, 340, 120, 'FatFs', layer='L3', parent='freertos')
    for key in ('client', 'http', 'fs'):
        p.lane(key, 1510)
    p.message('client', 'http', 370, 'PUT: содержимое файла')
    p.message('http', 'fs', 490, 'Блок данных')
    p.message('http', 'fs', 610, 'Следующий блок')
    p.message('http', 'fs', 750, 'Синхронизация и закрытие', 'call')
    p.message('fs', 'http', 870, 'Результат записи')
    p.message('http', 'client', 990, 'ok + bytes')
    p.message('client', 'http', 1130, 'GET: тот же файл')
    p.message('http', 'fs', 1250, 'Чтение', 'call')
    p.message('fs', 'http', 1370, 'Прочитанные данные')
    p.message('http', 'client', 1490, 'Содержимое файла')
    p.save()


def generate():
    device_settings()
    file_transfer()
    return 2


if __name__ == '__main__':
    drawing.OUT = drawing.ROOT/'docs/figures'
    generate()
