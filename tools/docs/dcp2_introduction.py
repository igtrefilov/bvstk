"""Network and byte layouts for the DCP2 frame chapter."""
import json
import math
import xml.etree.ElementTree as ET

import generate_pl_subsystem as drawing
from generate_pl_subsystem import NS, Page, el


MUTED = "#607487"
NETWORK = "#f2f5f8"
HEADER = "#eef5fb"
PAYLOAD = "#cfe3f3"
BLUE = "#3f75ad"
SOURCES = (
    "src/protocols/dcp2/bvstk_dcp2_codec.h",
    "src/protocols/dcp2/bvstk_dcp2_codec.c",
    "src/apps/freertos/services/dcp2/dcp2_server.c",
    "scripts/dcp2/dcp2_client.py",
)


class PacketFigure(Page):
    """Protocol fields have schematic widths and no runtime layer labels."""

    def __init__(self, name, title, description):
        super().__init__("reference/dcp2/" + name + ".svg", title,
                         description, SOURCES)
        self.width = 1800
        self.root.set("width", str(self.width))
        self.background.set("width", str(self.width))
        self.legend_note = ""

    def cell(self, key, x, y, width, height, fill, stroke=MUTED):
        group = el(self.blocks, "g", id=key, data_kind="packet-field")
        el(group, "rect", x=x, y=y, width=width, height=height,
           fill=fill, stroke=stroke, stroke_width=2)
        self.min_y = min(self.min_y, y)
        self.max_y = max(self.max_y, y + height)
        return group

    def bracket(self, key, x, y, width, label, color=MUTED):
        group = el(self.notes, "g", id=key, data_kind="byte-range")
        el(group, "path", d=f"M{x},{y+12} V{y} H{x+width} V{y+12}",
           fill="none", stroke=color, stroke_width=2)
        self.text(group, x + width / 2, y - 16, label, 24,
                  bold=True, color=color, anchor="middle")

    def key(self, x, y, fill, label):
        el(self.notes, "rect", x=x, y=y-20, width=34, height=25,
           fill=fill, stroke=MUTED, stroke_width=1.5)
        self.text(self.notes, x+48, y, label, 21, color=MUTED)

    def save(self):
        # The legend is drawn beside the byte scales. Remove the empty space
        # reserved by Page for runtime arrow and layer legends.
        bottom = math.ceil(self.max_y + 40)
        super().save()
        path = drawing.OUT / self.name
        tree = ET.parse(path)
        root = tree.getroot()
        top = int(root.get("viewBox").split()[1])
        root.set("height", str(bottom-top))
        root.set("viewBox", f"0 {top} {self.width} {bottom-top}")
        root.find(f"{{{NS}}}rect").set("height", str(bottom))
        metadata = root.find(f"{{{NS}}}metadata")
        data = json.loads(metadata.text)
        data["generator"] = "tools/docs/generate_documentation.py"
        metadata.text = json.dumps(data, ensure_ascii=False, indent=2)
        tree.write(path, encoding="utf-8", xml_declaration=True)


def encapsulation():
    p = PacketFigure(
        "encapsulation", "DCP2 внутри Ethernet, IPv4 и TCP",
        "Упрощённая вложенность сетевых форматов: Ethernet содержит пакет "
        "IPv4, IPv4 — сегмент TCP. Данные TCP несут байты потока DCP2. "
        "Размеры полей условные; опции и заполнение Ethernet не показаны.")
    y, height = 300, 140
    p.bracket("ethernet-frame", 60, 85, 1680, "Кадр Ethernet")
    p.bracket("ipv4-packet", 360, 155, 1240, "Пакет IPv4")
    p.bracket("tcp-segment", 680, 225, 920, "Сегмент TCP")
    fields = (
        ("ethernet-header", 60, 300, NETWORK, "Заголовок\nEthernet"),
        ("ipv4-header", 360, 320, NETWORK, "Заголовок\nIPv4"),
        ("tcp-header", 680, 320, NETWORK, "Заголовок\nTCP"),
        ("tcp-data", 1000, 600, PAYLOAD, "Данные TCP\nбайты DCP2"),
        ("ethernet-fcs", 1600, 140, NETWORK, "FCS"),
    )
    for key, x, width, fill, label in fields:
        g = p.cell(key, x, y, width, height, fill)
        lines = label.count("\n") + 1
        baseline = y + height/2 - (lines-1)*27*1.38/2 + 9
        p.text(g, x+width/2, baseline, label, 27,
               anchor="middle", bold=key == "tcp-data")
    p.key(60, 515, NETWORK, "Сетевое оформление")
    p.key(590, 515, PAYLOAD, "Данные приложения")
    p.text(p.notes, 1120, 515, "FCS — контрольная сумма Ethernet", 21, color=MUTED)
    p.save()


def structure():
    p = PacketFigure(
        "structure", "Общая структура кадра DCP2",
        "Заголовок DCP2 занимает 8 байт: magic, ver и dcp_len. "
        "Полезная нагрузка начинается с srv, op и seq, затем идёт body "
        "переменной длины. Смещения полей указаны от начала кадра DCP2; "
        "ширина ячеек условная.")
    y, height = 215, 145
    p.bracket("dcp2-frame", 60, 75, 1680, "Кадр DCP2: 8 + dcp_len байт", BLUE)
    p.bracket("header", 60, 160, 760, "Заголовок: 8 байт", BLUE)
    p.bracket("payload", 820, 160, 920, "Полезная нагрузка: dcp_len байт", BLUE)
    fields = (
        ("magic", 60, 320, 0, "4 байта"),
        ("ver", 380, 220, 4, "2 байта"),
        ("dcp_len", 600, 220, 6, "2 байта"),
        ("srv", 820, 120, 8, "1 байт"),
        ("op", 940, 120, 9, "1 байт"),
        ("seq", 1060, 180, 10, "2 байта"),
        ("body", 1240, 500, 12, "Переменная длина"),
    )
    for name, x, width, offset, length in fields:
        g = p.cell(name, x, y, width, height, HEADER if offset < 8 else PAYLOAD)
        p.text(g, x+width/2, y+57, name, 28, bold=True, anchor="middle")
        p.text(g, x+width/2, y+104, length, 24, anchor="middle")
        el(p.notes, "path", d=f"M{x},{y+height+8} v10", stroke=MUTED)
        p.text(p.notes, x, y+height+49, str(offset), 23,
               color=MUTED, anchor="middle")
    el(p.notes, "path", d=f"M1740,{y+height+8} v10", stroke=MUTED)
    p.text(p.notes, 1740, y+height+49, "8 + dcp_len", 23,
           color=MUTED, anchor="end")
    p.key(60, 490, HEADER, "Заголовок DCP2")
    p.key(590, 490, PAYLOAD, "Полезная нагрузка DCP2")
    p.text(p.notes, 1190, 490, "Внизу — смещения в байтах", 21, color=MUTED)
    p.save()


def generate():
    for draw in (encapsulation, structure):
        draw()
    return 2
