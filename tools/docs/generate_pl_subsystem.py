#!/usr/bin/env python3
"""Generate the editable SVG series about the BVSTK PL subsystems.

Run from any directory with Python 3. Only the standard library is required.
The coordinates are deliberate: separate lanes keep transfers, calls and
interrupts readable. Source references describe the implementation inspected
for each sheet; they do not imply hardware qualification.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import json
import os
import re
import textwrap
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
ARCH = ROOT / "docs/system/figures/architecture"
OUT = ARCH / "pl-subsystem"
NS = "http://www.w3.org/2000/svg"
XLINK = "http://www.w3.org/1999/xlink"
ET.register_namespace("", NS)
ET.register_namespace("xlink", XLINK)
PALETTE = {
    "L0": ("#e9eef3", "#607487"),
    "PL": ("#e6dcf5", "#8760ba"),
    "L1": ("#fff0d6", "#b56b1d"),
    "L2": ("#ffd1c7", "#c65b4e"),
    "L3": ("#d9f0ea", "#287f73"),
    "L4": ("#d9ebff", "#3f75ad"),
}
ARROWS = {
    "call": ("#788794", "Вызов", None),
    "mmio": ("#b56b1d", "MMIO и настройка", None),
    "data": ("#3f75ad", "Данные", None),
    "irq": ("#c65b4e", "IRQ и события", "9 6"),
    "clock": ("#788794", "Тактирование и сброс", "2 6"),
}
SHEETS: dict[str, "Page"] = {}


def el(parent, tag, text=None, **attrs):
    node = ET.SubElement(parent, f"{{{NS}}}{tag}", {
        k.replace("_", "-"): str(v) for k, v in attrs.items() if v is not None
    })
    node.text = text
    return node


def lines(value, width, size):
    # Deliberately conservative for DejaVu Sans, including Cyrillic.
    result = []
    for paragraph in str(value).split("\n"):
        result += textwrap.wrap(paragraph, max(8, int(width / (size * .60))),
                                break_long_words=False, break_on_hyphens=False) or [""]
    return result


def href(source: str, target: str):
    path, sep, fragment = target.partition("#")
    absolute = ARCH / path[1:] if path.startswith("@") else OUT / path
    relative = os.path.relpath(absolute, (OUT / source).parent)
    return relative + (sep + fragment if sep else "")


@dataclass
class Box:
    key: str
    x: float
    y: float
    w: float
    h: float
    group: object
    label: str

    def point(self, side, offset=0):
        if side == "N": return self.x + self.w / 2 + offset, self.y
        if side == "S": return self.x + self.w / 2 + offset, self.y + self.h
        if side == "W": return self.x, self.y + self.h / 2 + offset
        return self.x + self.w, self.y + self.h / 2 + offset


class Page:
    def __init__(self, name, title, subtitle, sources=()):
        self.name, self.title, self.subtitle = name, title, subtitle
        self.sources = list(sources)
        self.root = ET.Element(f"{{{NS}}}svg", {
            "width": "2200", "viewBox": "0 0 2200 1600",
            "role": "img", "aria-labelledby": "title desc",
        })
        el(self.root, "title", title, id="title")
        el(self.root, "desc", subtitle, id="desc")
        defs = el(self.root, "defs")
        el(defs, "style", """
text { font-family: "DejaVu Sans", sans-serif; fill: #233747; }
a { cursor: pointer; } a:hover text { text-decoration: underline; }
g:target > rect { stroke-width: 5; }
.edge-label { paint-order: stroke; stroke: white; stroke-width: 8; stroke-linejoin: round; }
""")
        for kind, (color, _, dash) in ARROWS.items():
            marker = el(defs, "marker", id=kind + "-arrow", viewBox="0 0 10 10",
                        refX=10, refY=5, markerWidth=12, markerHeight=12,
                        markerUnits="userSpaceOnUse", orient="auto-start-reverse")
            el(marker, "path", d="M0 0 L10 5 L0 10 Z", fill=color)
            marker = el(defs, "marker", id=kind + "-source", viewBox="0 0 4 4",
                        refX=2, refY=2, markerWidth=6, markerHeight=6,
                        markerUnits="userSpaceOnUse")
            el(marker, "circle", cx=2, cy=2, r=2, fill=color)
        self.background = el(self.root, "rect", width=2200, height=1600, fill="white")
        self.under = el(self.root, "g", id="backgrounds")
        self.edges = el(self.root, "g", id="interactions")
        self.blocks = el(self.root, "g", id="components")
        self.notes = el(self.root, "g", id="annotations")
        self.nodes: dict[str, Box] = {}
        self.used_layers, self.used_arrows = set(), set()
        self.links, self.max_y = [], 225
        self.text(self.notes, 60, 42, "BVSTK", 16, bold=True, color="#607487")
        self.text(self.notes, 60, 94, title, 36, bold=True)
        nav = [
            ("Оглавление", "README.md"), ("Мост", "overview/bridge.svg"),
            ("Состав", "overview/subsystems.svg"),
            ("Обмен", "overview/exchange.svg"), ("Память", "overview/memory.svg"),
        ]
        if not name.startswith("overview/"):
            nav += [
                ("I²C", "i2c/subsystem.svg"), ("MDIO", "mdio/subsystem.svg"),
                ("SPI", "spi/subsystem.svg"), ("Адреса", "reference/memory-map.svg"),
                ("Программа PS", "ps-interaction/startup.svg"),
            ]
        nav.append(("L0–L4", "@levels.svg"))
        x = 60
        for label, target in nav:
            w = 52 + len(label) * 12
            a = self.link(self.notes, target, navigation="sheet")
            el(a, "rect", x=x, y=123, width=w, height=43, rx=8,
               fill="#f6f8fa", stroke="#ced6dd")
            self.text(a, x+w/2, 151, label, 19, bold=True, anchor="middle")
            x += w + 16
        self.text(self.notes, 60, 207, subtitle, 21, color="#576c7b")
        SHEETS[name] = self

    def text(self, parent, x, y, value, size=21, bold=False,
             color="#233747", anchor="start", width=None, line_height=None, cls=None):
        chunks = lines(value, width, size) if width else str(value).split("\n")
        lh = line_height or size * 1.38
        for i, line in enumerate(chunks):
            # Long source identifiers stay intact, with a bounded font size.
            fitted = min(size, width / (len(line) * .60)) if width and line else size
            label = el(parent, "text", line, x=x, y=y+i*lh, font_size=round(fitted, 2),
                       font_weight=700 if bold else 400, text_anchor=anchor,
                       style=f"fill:{color}", **({"class": cls} if cls else {}))
            if re.search(r"\bL[0-4]\b", line):
                label.text = None
                label.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
                for chunk in re.split(r"(\bL[0-4]\b)", line):
                    if chunk:
                        el(label, "tspan", chunk,
                           style=f"fill:{PALETTE[chunk][1]}" if chunk in PALETTE else None)
        return len(chunks) * lh

    def link(self, parent, target, navigation="detail"):
        a = el(parent, "a", data_navigation=navigation)
        a.set(f"{{{XLINK}}}href", href(self.name, target))
        return a

    def frame(self, key, x, y, w, h, label, layer="PL", parent=None,
              target=None, subtitle=None, ownership=None):
        # A system boundary may contain components from several layers.
        color = PALETTE[layer][1] if layer else "#233747"
        owner = self.nodes[parent].group if parent else self.blocks
        kind = "ownership-region" if ownership else "functional-group" if layer else "system-boundary"
        g = el(owner, "g", id=key, data_kind=kind,
               data_label=label, data_layer="L0" if layer == "PL" else layer,
               data_domain="pl" if layer == "PL" else None,
               data_ownership=ownership or "common")
        el(self.under, "rect", x=x, y=y, width=w, height=h, rx=16, fill="white")
        el(g, "rect", x=x, y=y, width=w, height=h, rx=16, fill="none",
           stroke="#8495a3" if ownership else color, stroke_width=2,
           stroke_dasharray="10 7" if ownership else None)
        header = self.link(g, target) if target else g
        self.text(header, x+26, y+37, label, 25, bold=True, color=color)
        if target:
            self.text(header, x+w-24, y+36, "↗", 22, color=color, anchor="end")
            self.links.append({"node": key, "target": href(self.name, target)})
        if subtitle:
            self.text(g, x+26, y+70, subtitle, 18, color="#576c7b", width=w-52)
        self.nodes[key] = Box(key, x, y, w, h, g, label)
        self.max_y = max(self.max_y, y+h)
        if layer:
            self.used_layers.add(layer)
        return key

    def node(self, key, x, y, w, h, label, body="", layer="PL",
             parent=None, target=None, source=None, ownership="common"):
        fill, color = PALETTE[layer]
        owner = self.nodes[parent].group if parent else self.blocks
        g = el(owner, "g", id=key, data_kind="component",
               data_layer="L0" if layer == "PL" else layer,
               data_domain="pl" if layer == "PL" else None, data_label=label,
               data_ownership=ownership, data_source=source, data_parent=parent)
        el(g, "rect", x=x, y=y, width=w, height=h, rx=11,
           fill=fill, stroke=color, stroke_width=1.8)
        head = self.link(g, target) if target else g
        self.text(head, x+20, y+24, "L0. PL" if layer == "PL" else layer,
                  14, bold=True, color=color)
        if target:
            self.text(head, x+w-18, y+25, "↗", 20, anchor="end", color=color)
            self.links.append({"node": key, "target": href(self.name, target)})
            g.set("data-target", href(self.name, target))
        title_h = self.text(head, x+20, y+58, label, 24, bold=True, width=w-40)
        if body:
            used = self.text(g, x+20, y+64+title_h, body, 19, width=w-40)
            if 64+title_h+used > h+15:
                raise ValueError(f"Text height exceeds {self.name}#{key}: {h}")
        self.nodes[key] = Box(key, x, y, w, h, g, label)
        self.used_layers.add(layer)
        self.max_y = max(self.max_y, y+h)
        return key

    def edge(self, a, b, kind="data", label="", start=None, end=None,
             via=(), a_offset=0, b_offset=0, label_xy=None, bidirectional=False):
        aa, bb = self.nodes[a], self.nodes[b]
        if not start:
            if aa.y + aa.h <= bb.y:
                start, end = "S", "N"
            elif bb.y + bb.h <= aa.y:
                start, end = "N", "S"
            else:
                start, end = ("E", "W") if aa.x < bb.x else ("W", "E")
        pa, pb = aa.point(start, a_offset), bb.point(end, b_offset)
        if not via and pa[0] != pb[0] and pa[1] != pb[1]:
            if start in ("N", "S"):
                mid = (pa[1]+pb[1])/2
                via = ((pa[0], mid), (pb[0], mid))
            else:
                mid = (pa[0]+pb[0])/2
                via = ((mid, pa[1]), (mid, pb[1]))
        self.path(a, b, (pa, *via, pb), kind, label, label_xy, bidirectional)

    def path(self, a, b, points, kind="data", label="", label_xy=None, bidirectional=False):
        color, _, dash = ARROWS[kind]
        g = el(self.edges, "g", data_kind="interaction", data_from=a,
               data_to=b, data_interaction=kind,
               data_direction="both" if bidirectional else "forward")
        d = "M" + " L".join(f"{x:g},{y:g}" for x, y in points)
        # A small white gap distinguishes crossings from connected junctions.
        el(g, "path", d=d, fill="none", stroke="white", stroke_width=11,
           stroke_linejoin="round", data_kind="crossing-clearance")
        el(g, "path", d=d, fill="none", stroke=color,
           stroke_width=3.4 if kind == "data" else 2.7,
           stroke_dasharray=dash, stroke_linejoin="round",
           marker_start=f"url(#{kind}-{'arrow' if bidirectional else 'source'})",
           marker_end=f"url(#{kind}-arrow)")
        self.used_arrows.add(kind)
        if label:
            if label_xy:
                x, y = label_xy
            else:
                p, q = points[len(points)//2-1], points[len(points)//2]
                x, y = (p[0]+q[0])/2, (p[1]+q[1])/2-14
            self.text(self.notes, x, y, label, 18, color=color,
                      anchor="middle", cls="edge-label")

    def note(self, y, text, x=60, width=2080):
        h = self.text(self.notes, x, y, text, 20, color="#576c7b", width=width)
        self.max_y = max(self.max_y, y+h)
        return h

    def section(self, y, label):
        self.text(self.notes, 60, y, label, 27, bold=True)
        self.max_y = max(self.max_y, y+20)

    def save(self):
        y = self.max_y + 52
        x = 60
        for kind in ARROWS:
            if kind not in self.used_arrows: continue
            color, label, dash = ARROWS[kind]
            el(self.notes, "path", d=f"M{x},{y} h65", fill="none", stroke=color,
               stroke_width=3, stroke_dasharray=dash, marker_end=f"url(#{kind}-arrow)")
            self.text(self.notes, x+82, y+6, label, 18, color="#576c7b")
            x += 130 + len(label) * 11
        y += 56
        x = 60
        for layer in PALETTE:
            if layer not in self.used_layers: continue
            fill, color = PALETTE[layer]
            el(self.notes, "rect", x=x, y=y-19, width=32, height=24, rx=4,
               fill=fill, stroke=color)
            label = "PL внутри L0" if layer == "PL" else layer
            self.text(self.notes, x+44, y, label, 18, color="#576c7b")
            x += 200 if layer == "PL" else 115
        self.text(self.notes, 60, y+47,
                  "Заголовки со знаком ↗ открывают продолжение. Цвет обозначает уровень абстракции.",
                  18, color="#576c7b")
        el(self.root, "metadata", json.dumps({
            "generator": "tools/docs/generate_pl_subsystem.py",
            "sources": self.sources, "cross_sheet_links": self.links,
        }, ensure_ascii=False, indent=2), id="provenance")
        height = int(y+90)
        self.root.set("height", str(height))
        self.root.set("viewBox", f"0 0 2200 {height}")
        self.background.set("height", str(height))
        path = OUT / self.name
        path.parent.mkdir(parents=True, exist_ok=True)
        ET.indent(self.root, space="  ")
        # Pretty-printing must not add spaces between inline layer references.
        for label in self.root.iter(f"{{{NS}}}text"):
            if len(label):
                label.text = None
                for span in label:
                    span.tail = None
        ET.ElementTree(self.root).write(path, encoding="utf-8", xml_declaration=True)


def sequence(name, title, subtitle, actors, steps, sources=(), notes=()):
    """Horizontal exchanges between fixed actors, numbered from top to bottom."""
    p = Page(name, title, subtitle, sources)
    n = len(actors)
    gap = 42
    w = (2020-(n-1)*gap)/n
    top, row, start = 305, 126, 530
    last = start + row*len(steps)
    centers = {}
    ownership_extents = {}
    for i, actor in enumerate(actors):
        if actor[5]:
            x = 100+i*(w+gap)
            extent = ownership_extents.setdefault(actor[5], [x, x+w])
            extent[1] = x+w
    for ownership, (left, right) in ownership_extents.items():
        p.frame(ownership+"-os", left-14, 245, right-left+28, 245,
                "FreeRTOS" if ownership == "freertos" else "Нейтрино",
                "L1", ownership=ownership)
    for i, actor in enumerate(actors):
        key, label, body, layer, target, ownership = actor
        x = 100+i*(w+gap)
        p.node(key, x, top, w, 165, label, body, layer=layer,
               target=target, ownership=ownership or "common",
               parent=ownership+"-os" if ownership else None)
        centers[key] = x+w/2
        el(p.under, "path", d=f"M{x+w/2},{top+165} V{last-20}",
           stroke="#d3dce3", stroke_width=1.5, fill="none")
    for i, (a, b, kind, label, detail) in enumerate(steps):
        y = start + i*row
        el(p.notes, "rect", x=63, y=y-31, width=2074, height=63, rx=5,
           fill="white", opacity=".96")
        p.text(p.notes, 76, y, f"{i+1:02}", 21, bold=True, color=ARROWS[kind][0])
        p.text(p.notes, 130, y, label, 22, bold=True, width=1970)
        if detail:
            p.text(p.notes, 130, y+31, detail, 18, color="#576c7b", width=1950)
        xa, xb = centers[a], centers[b]
        yy = y+61
        pts = ((xa, yy), (xb, yy)) if a != b else (
            (xa, yy), (xa+86, yy), (xa+86, yy+24), (xa, yy+24))
        p.path(a, b, pts, kind)
    p.max_y = max(p.max_y, last+20)
    y = last+68
    for note in notes:
        y += p.note(y, note)+20
    return p


def source(bus, role, filename):
    directory = {
        ("i2c", "master"): "i2c_master/src",
        ("i2c", "slave"): "i2c_slave/i2c/slave/rtl",
        ("mdio", "master"): "SMI_master/src",
        ("mdio", "slave"): "SMI_slave/src",
        ("spi", "master"): "SPI_master/src",
        ("spi", "slave"): "SPI_Slave",
    }[bus, role]
    return f"../hw_platform/fpga/ip_repo/{directory}/{filename}.sv"


# Sheet definitions follow. Geometry is explicit and differs with the mechanism.

def bridge_overview_sheet():
    """Introduce the bridge without expanding its software and bus internals."""
    p = Page("overview/bridge.svg", "BVSTK: мост между устройствами",
             "Два участка шины соединены через BVSTK. Политика решает, передавать ли транзакцию дальше.", [
                 "../hw_platform/fpga/Burevestnik_21.tcl",
                 "src/services/i2c/bvstk_i2c_slave_service.c",
                 "src/services/mdio/bvstk_mdio_service.c",
             ])
    p.frame("bridge", 420, 280, 1360, 695, "BVSTK", layer=None,
            target="overview/subsystems.svg")
    p.node("policy", 945, 385, 755, 205, "Политика в PS",
           "Проверка транзакции T",
           "L3", parent="bridge", target="overview/subsystems.svg#policy")
    p.node("host", 60, 730, 300, 185, "Внешний\nведущий",
           "Начинает транзакцию", "L0", target="overview/subsystems.svg#host")
    p.node("pl_slave", 500, 730, 290, 185, "pl_slave",
           "Приём запроса\nВыдача ответа", parent="bridge",
           target="overview/subsystems.svg#pl_slave")
    p.node("bram", 935, 730, 330, 185, "BRAM",
           "Промежуточная память\nЗапросы и ответы", parent="bridge",
           target="overview/memory.svg#exchange-memory")
    p.node("pl_master", 1430, 730, 270, 185, "pl_master",
           "Обмен с устройством", parent="bridge",
           target="overview/subsystems.svg#pl_master")
    p.node("device", 1840, 730, 300, 185, "Внешнее ведомое\nустройство",
           "Выполняет операцию", "L0", target="overview/subsystems.svg#device")
    for a, b in (("pl_slave", "bram"), ("bram", "pl_master")):
        p.edge(a, b, "data", start="E", end="W", bidirectional=True)
    p.edge("bram", "policy", "data", "Запрос T", start="N", end="S",
           b_offset=-222.5, label_xy=(1015, 666))
    p.edge("policy", "pl_master", "mmio", "Разрешено:\nзапустить",
           start="S", end="N", a_offset=242.5, label_xy=(1658, 648))

    def transaction(key, cx, cy, direction=1, blocked=False):
        # A white packet symbol represents data, not another software layer.
        g = el(p.notes, "g", id=key, data_kind="transaction",
               data_transaction="T", data_state="blocked" if blocked else "forwarded")
        el(g, "title", "Транзакция T остановлена" if blocked else "Транзакция T")
        points = ((-46, -29), (25, -29), (46, 0), (25, 29), (-46, 29))
        el(g, "polygon", points=" ".join(f"{cx+direction*x},{cy+y}" for x, y in points),
           fill="white", stroke=ARROWS["data"][0], stroke_width=2.5,
           stroke_linejoin="round")
        p.text(g, cx-4*direction, cy+9, "T", 27, bold=True,
               color=ARROWS["data"][0], anchor="middle")
        if blocked:
            el(g, "path", d=f"M{cx-29},{cy+28} L{cx+29},{cy-28}",
               fill="none", stroke="white", stroke_width=9)
            el(g, "path", d=f"M{cx-29},{cy+28} L{cx+29},{cy-28}",
               fill="none", stroke="#233747", stroke_width=4)

    def bus(key, left, right, participants, label):
        # Separate physical bus segments; the packet arrow runs above the wire.
        color = PALETTE["L0"][1]
        g = el(p.edges, "g", id=key, data_kind="bus", data_layer="L0",
               data_members=" ".join(participants), data_label=label)
        el(g, "title", label)
        el(g, "path", d=f"M{left},1115 H{right}", fill="none",
           stroke=color, stroke_width=5, stroke_linecap="round")
        for participant in participants:
            x, y = p.nodes[participant].point("S")
            el(g, "path", d=f"M{x},{y} V1115", fill="none",
               stroke=color, stroke_width=3)
            el(g, "circle", cx=x, cy=1115, r=5, fill=color)
        p.text(p.notes, (left+right)/2, 1170, label, 20, bold=True,
               anchor="middle", color=color)

    bus("host-bus", 110, 730, ("host", "pl_slave"), "Шина ведущего")
    bus("device-bus", 1470, 2100, ("pl_master", "device"), "Шина ведомого")
    p.path("host", "pl_slave", ((270, 1068), (585, 1068)), "data")
    p.path("pl_master", "device", ((1625, 1068), (1930, 1068)), "data")
    transaction("transaction-in", 420, 1068)
    transaction("transaction-out", 1780, 1068)
    p.text(p.notes, 420, 1020, "Входящая транзакция", 18, anchor="middle",
           color=ARROWS["data"][0])
    p.text(p.notes, 1780, 1020, "Транзакция пропущена", 18, anchor="middle",
           color=ARROWS["data"][0])

    p.path("policy", "transaction-denied", ((945, 487.5), (548, 487.5)),
           "data", "Запрещено", label_xy=(727, 432))
    transaction("transaction-denied", 727, 487.5, direction=-1, blocked=True)
    el(p.notes, "path", d="M532,463.5 V511.5", stroke="#233747",
       stroke_width=6, stroke_linecap="round")
    p.text(p.notes, 727, 560, "Транзакция остановлена", 18,
           color="#233747", anchor="middle")
    p.note(1250, "T — условная транзакция: адрес, операция и данные. Шина показана серой линией, движение T — синей стрелкой.")


def overview_sheets():
    """Describe functional roles before introducing any bus implementation."""
    sources = [
        "../hw_platform/fpga/Burevestnik_21.tcl",
        "src/services/i2c/bvstk_i2c_slave_service.c",
        "src/services/mdio/bvstk_mdio_service.c",
        "src/shared/config/bvstk_device_profile.c",
        "src/drivers/pl/sd/bvstk_sd_master.c",
    ]
    p = Page("overview/subsystems.svg", "Подсистема PL: общее устройство",
             "pl_slave принимает внешние обращения. pl_master выполняет операции на внешней шине.",
             sources)
    p.node("application", 80, 275, 620, 170, "Прикладные команды",
           "Операции с устройствами и настройками", "L4",
           target="@architecture_bvstk_l4.svg")
    p.node("profile", 1320, 275, 800, 215, "Профиль подсистемы",
           "Устройства и ресурсы\nПравила доступа и начальные настройки", "L4",
           target="overview/memory.svg#profile")
    p.frame("services", 300, 660, 1600, 600, "Сервис подсистемы", "L3",
            target="@architecture_bvstk_l3.svg#subsystem-services")
    p.node("operation", 340, 785, 700, 205, "Обработка операций",
           "Принять запрос\nВыбрать действие и вернуть результат", "L3",
           parent="services", target="overview/exchange.svg#service")
    p.node("policy", 1260, 785, 600, 205, "Политика",
           "Проверить разрешение операции\nПрименить правила устройства", "L3",
           parent="services", target="ps-interaction/configuration.svg")
    p.node("cache", 1240, 1070, 620, 145, "Программный кэш",
           "Значения и их достоверность", "L3", parent="services",
           target="overview/memory.svg#cache")
    p.edge("application", "operation", "call", start="S", end="N",
           via=((390, 555), (690, 555)))
    p.edge("profile", "services", "data", "Конфигурация", start="S", end="N",
           b_offset=550, via=((1720, 550), (1650, 550)), label_xy=(1780, 605))
    p.edge("operation", "policy", "call", "Проверка", start="E", end="W",
           a_offset=-45, b_offset=-45, label_xy=(1150, 821))
    p.edge("policy", "operation", "data", "Решение", start="W", end="E",
           a_offset=45, b_offset=45, label_xy=(1150, 970))
    p.edge("operation", "cache", "data", start="S", end="N", a_offset=200,
           via=((890, 1025), (1550, 1025)), bidirectional=True)
    p.node("driver", 450, 1410, 700, 175, "Драйвер подсистемы",
           "Команды ядер, буферы и условия завершения", "L3",
           target="overview/exchange.svg#completion")
    p.edge("operation", "driver", "call", "Разрешённая операция", start="S", end="N",
           a_offset=-150, via=((540, 1320), (800, 1320)), label_xy=(800, 1370))
    p.node("platform", 450, 1710, 700, 170, "Платформенный доступ",
           "MMIO, память и обработка событий\nРеализация в порте выбранной ОС", "L1",
           target="@architecture_bvstk_l1.svg#platform-access")
    p.edge("driver", "platform", "call")
    p.node("ps", 450, 2010, 700, 170, "Аппаратная часть PS",
           "AXI к PL и доступ к памяти обмена", "L0",
           target="@architecture_bvstk_ps_pl.svg#ps-axi")
    p.edge("platform", "ps", "mmio")
    p.node("registers", 1430, 2010, 690, 200, "Управление ядрами",
           "Настройка, запуск и состояние\nАдресация через AXI", "PL",
           target="overview/memory.svg#registers")
    p.edge("ps", "registers", "mmio", start="E", end="W", b_offset=-15)
    p.frame("pl-subsystem", 60, 2470, 2080, 600, "Аппаратная часть подсистемы",
            target="@architecture_bvstk_ps_pl.svg")
    p.frame("pl_slave", 110, 2580, 600, 360, "pl_slave", parent="pl-subsystem",
            target="overview/exchange.svg#pl_slave")
    p.node("slave-rx", 210, 2670, 450, 105, "Приём внешнего запроса", parent="pl_slave")
    p.node("slave-tx", 210, 2820, 450, 90, "Выдача ответа", parent="pl_slave")
    p.node("exchange-memory", 820, 2670, 570, 220, "Память обмена",
           "BRAM и аппаратные буферы\nЗапросы, результаты и ответы", parent="pl-subsystem",
           target="overview/memory.svg#exchange-memory")
    p.frame("pl_master", 1500, 2580, 600, 400, "pl_master", parent="pl-subsystem",
            target="overview/exchange.svg#pl_master")
    p.node("master-engine", 1540, 2670, 520, 255, "Выполнение операции",
           "Формирование сигналов шины\nПередача и приём данных\nЗавершение и ошибки", parent="pl_master")
    p.edge("registers", "pl_slave", "mmio", start="S", end="N", a_offset=-150, b_offset=230,
           via=((1625, 2300), (640, 2300)))
    p.edge("registers", "master-engine", "mmio", start="S", end="N", a_offset=180,
           via=((1955, 2400), (1800, 2400)))
    p.edge("ps", "exchange-memory", "data", start="S", end="N",
           via=((800, 2380), (1105, 2380)), bidirectional=True)
    p.edge("slave-rx", "exchange-memory", "data", start="E", end="W", b_offset=-57.5)
    p.edge("exchange-memory", "slave-tx", "data", start="W", end="E", a_offset=85)
    p.edge("exchange-memory", "master-engine", "data", start="E", end="W",
           b_offset=-17.5, bidirectional=True)
    p.node("host", 110, 3260, 600, 170, "Внешний ведущий",
           "Обращается к подсистеме", "L0", target="overview/exchange.svg#host")
    p.node("device", 1500, 3260, 600, 170, "Внешнее ведомое устройство",
           "Выполняет операции ведущего ядра", "L0", target="overview/exchange.svg#device")
    p.edge("host", "slave-rx", "data", "Запрос", start="N", end="W", a_offset=-250,
           via=((160, 2722.5),), label_xy=(235, 3180))
    p.edge("slave-tx", "host", "data", "Ответ", start="S", end="N", b_offset=100,
           via=((435, 3130), (510, 3130)), label_xy=(550, 3180))
    p.edge("master-engine", "device", "data", start="S", end="N", bidirectional=True)
    p.note(3530, "pl_slave и pl_master обозначают роли на внешней шине. Их роли на AXI задаются отдельно.")
    p.note(3590, "Показаны обе аппаратные роли. Реализация может использовать одну из них; состав политики и кэша определяется задачами сервиса.")

    p = Page("overview/exchange.svg", "Подсистема PL: ход операций",
             "Операция PS и входящий запрос проходят через разные аппаратные роли.", sources)
    p.section(285, "Операцию начинает PS")
    p.node("service", 80, 360, 560, 210, "Сервис подсистемы",
           "Проверить правила\nПодготовить ведущую операцию", "L3",
           target="overview/subsystems.svg#operation")
    p.node("pl_master", 900, 360, 520, 225, "pl_master",
           "Выполнить команду\nЗафиксировать результат и состояние", target="overview/subsystems.svg#pl_master")
    p.node("device", 1690, 360, 440, 180, "Внешнее ведомое\nустройство",
           "Обмен с ведущим ядром", "L0", target="overview/subsystems.svg#device")
    p.edge("service", "pl_master", "mmio", "Через драйвер L3 и L1",
           start="E", end="W", b_offset=-7.5, label_xy=(770, 437))
    p.edge("pl_master", "device", "data", start="E", end="W", a_offset=-22.5,
           bidirectional=True)
    p.node("result-buffer", 900, 850, 620, 185, "Результат операции",
           "Принятые данные в памяти обмена", target="overview/memory.svg#result-buffer")
    p.node("completion", 80, 850, 640, 230, "Обработка завершения",
           "Драйвер проверяет состояние и ошибки\nПолучает данные\nВозвращает результат сервису", "L3",
           target="overview/subsystems.svg#driver")
    p.edge("pl_master", "result-buffer", "data", start="S", end="N",
           a_offset=100, b_offset=50)
    p.edge("pl_master", "completion", "irq", "IRQ, если подключён", start="S", end="N",
           a_offset=-140, via=((1020, 730), (400, 730)), label_xy=(690, 708))
    p.edge("completion", "pl_master", "mmio", "Опрос состояния", start="E", end="W",
           a_offset=-45, b_offset=37.5, via=((800, 920), (800, 510)),
           label_xy=(800, 650))
    p.edge("result-buffer", "completion", "data", "Данные", start="W", end="E",
           a_offset=47.5, b_offset=25, label_xy=(810, 974))
    p.edge("completion", "service", "data", "Результат", start="N", end="S",
           a_offset=-220, b_offset=-180, label_xy=(180, 690))
    p.note(1170, "Опрос состояния и доставка IRQ — способы обнаружить завершение. Достоверность данных и результат операции проверяются драйвером.")

    p.section(1310, "Входящий запрос, требующий обработки PS")
    p.node("host", 80, 1380, 520, 200, "Внешний ведущий",
           "Передаёт запрос и получает ответ", "L0", target="overview/subsystems.svg#host")
    p.node("pl_slave", 820, 1380, 600, 210, "pl_slave",
           "Принять обращение\nПередать запрос в память обмена", target="overview/subsystems.svg#pl_slave")
    p.node("request-buffer", 1610, 1380, 520, 210, "Принятый запрос",
           "Адрес, операция и данные", target="overview/memory.svg#request-buffer")
    p.edge("host", "pl_slave", "data", "Запрос", start="E", end="W",
           a_offset=-45, b_offset=-50, label_xy=(710, 1416))
    p.edge("pl_slave", "host", "data", "Ответ", start="W", end="E",
           a_offset=45, b_offset=50, label_xy=(710, 1558))
    p.edge("pl_slave", "request-buffer", "data")
    p.node("input-handler", 1610, 1830, 520, 215, "Приём запроса в PS",
           "Получить событие или опросить состояние\nПередать данные сервису", "L1",
           target="@architecture_bvstk_l1.svg")
    p.node("request-service", 820, 1820, 600, 260, "Сервис подсистемы",
           "Разобрать запрос и проверить политику\nОбработать данные\nПодготовить ответ", "L3",
           target="overview/subsystems.svg#services")
    p.node("reply-buffer", 80, 1830, 520, 220, "Подготовленный ответ",
           "Данные для внешнего ведущего", target="overview/memory.svg#reply-buffer")
    p.edge("request-buffer", "input-handler", "data", start="S", end="N",
           a_offset=100, b_offset=100)
    p.edge("pl_slave", "input-handler", "irq", "IRQ, если подключён", start="S", end="N",
           a_offset=200, b_offset=-150, via=((1320, 1720), (1720, 1720)),
           label_xy=(1530, 1696))
    p.edge("input-handler", "request-service", "call", start="W", end="E",
           b_offset=-12.5)
    p.edge("request-service", "reply-buffer", "data", start="W", end="E",
           a_offset=-10)
    p.edge("reply-buffer", "pl_slave", "data", start="N", end="S", b_offset=-100,
           via=((340, 1710), (1020, 1710)))
    p.node("forward-master", 820, 2290, 600, 210, "pl_master",
           "Ведущая операция по запросу сервиса\nЕсли нужен обмен с устройством",
           target="overview/exchange.svg#pl_master")
    p.edge("request-service", "forward-master", "mmio", "Через драйвер L3 и L1",
           start="S", end="N", a_offset=-170, b_offset=-170, label_xy=(890, 2195))
    p.edge("forward-master", "request-service", "data", "Результат",
           start="N", end="S", a_offset=170, b_offset=170, label_xy=(1320, 2195))
    p.note(2610, "Заранее подготовленный ответ pl_slave может выдать самостоятельно. Обновление такого ответа выполняется отдельной операцией PS.")
    p.note(2670, "Порядок ожидания, подтверждения и обработки ошибок определяется ядром и драйвером. Профиль и политика применяются сервисом.")

    p = Page("overview/memory.svg", "Память подсистемы: буферы и состояние",
             "Кто записывает запросы, результаты и ответы; как они связаны с состоянием программы.", sources)
    p.frame("exchange-memory", 760, 390, 760, 1040, "Память обмена",
            subtitle="BRAM и буферы обмена. Структура задаётся ядром.",
            target="overview/subsystems.svg#exchange-memory")
    p.node("request-buffer", 800, 535, 680, 185, "Принятый запрос",
           "Данные внешнего обращения\nДля последующей обработки PS", parent="exchange-memory",
           target="overview/exchange.svg#request-buffer")
    p.node("result-buffer", 800, 840, 680, 190, "Результат операции",
           "Данные, полученные ведущим ядром", parent="exchange-memory",
           target="overview/exchange.svg#result-buffer")
    p.node("reply-buffer", 800, 1155, 680, 210, "Подготовленный ответ",
           "Данные для выдачи внешнему ведущему\nОбновляются программой PS", parent="exchange-memory",
           target="overview/exchange.svg#reply-buffer")
    p.node("pl_slave", 100, 530, 500, 220, "pl_slave",
           "Записывает принятый запрос\nЧитает подготовленный ответ",
           target="overview/subsystems.svg#pl_slave")
    p.node("pl_master", 100, 825, 500, 235, "pl_master",
           "Передаёт принятые данные в память\nЗавершает ведущую операцию",
           target="overview/subsystems.svg#pl_master")
    p.frame("driver", 1650, 390, 500, 1040, "Драйвер подсистемы", "L3",
            target="overview/subsystems.svg#driver")
    p.node("take-request", 1690, 520, 420, 200, "Получить запрос",
           "Считать содержимое буфера\nПередать сервису", "L3", parent="driver")
    p.node("take-result", 1690, 845, 420, 180, "Получить результат",
           "Считать данные завершённой операции", "L3", parent="driver")
    p.node("publish", 1690, 1160, 420, 210, "Опубликовать ответ",
           "Записать данные\nСообщить ядру о готовности", "L3", parent="driver")
    p.edge("pl_slave", "request-buffer", "data", start="E", end="W", a_offset=-12.5)
    p.edge("pl_master", "result-buffer", "data", start="E", end="W", a_offset=-7.5)
    p.edge("request-buffer", "take-request", "data", start="E", end="W", b_offset=7.5)
    p.edge("result-buffer", "take-result", "data")
    p.edge("publish", "reply-buffer", "data", start="W", end="E", a_offset=-5)
    p.edge("reply-buffer", "pl_slave", "data", start="W", end="E", b_offset=40,
           via=((660, 1260), (660, 680)))
    p.section(1565, "Готовность и подтверждение")
    p.node("registers", 90, 1660, 600, 240, "Состояние pl_slave и pl_master",
           "Готовность и состояние обмена\nОшибки и параметры результата\nПодтверждение приёма",
           target="overview/subsystems.svg#registers")
    p.node("confirmation", 920, 1660, 490, 240, "Проверка и подтверждение",
           "Проверить готовность\nПолучить данные\nПодтвердить приём", "L3",
           target="overview/exchange.svg#completion")
    p.edge("driver", "confirmation", "call", start="S", end="N", a_offset=-100,
           via=((1800, 1510), (1165, 1510)))
    p.edge("confirmation", "registers", "mmio", "Через L1", start="W", end="E",
           bidirectional=True)
    p.section(2085, "Состояние в программе PS")
    p.node("profile", 80, 2160, 600, 220, "Профиль подсистемы",
           "Конфигурация и правила\nНачальные настройки", "L4",
           target="overview/subsystems.svg#profile")
    p.node("service", 1480, 2160, 650, 235, "Сервис подсистемы",
           "Применяет настройки\nОбрабатывает данные драйвера\nГотовит ответы и обновляет кэш", "L3",
           target="overview/subsystems.svg#services")
    p.node("cache", 800, 2520, 650, 240, "Программный кэш",
           "Значение, достоверность и источник\nВремя обновления\nХранится в памяти PS", "L3",
           target="overview/subsystems.svg#cache")
    p.edge("profile", "service", "data", "Конфигурация", start="E", end="W", b_offset=-7.5)
    p.edge("driver", "service", "data", "Запросы и результаты", start="S", end="N",
           a_offset=50, via=((1950, 2010), (1805, 2010)), label_xy=(1970, 1980))
    p.edge("service", "cache", "data", "Чтение и обновление", start="S", end="N",
           via=((1805, 2450), (1125, 2450)), bidirectional=True, label_xy=(1470, 2428))
    p.edge("service", "publish", "data", "Публикация", start="N", end="E",
           a_offset=180, b_offset=50,
           via=((1985, 2050), (2180, 2050), (2180, 1315)), label_xy=(2090, 1550))
    p.note(2870, "Программа обращается к аппаратной памяти через L1. Для потоковых ядер перенос данных может выполнять DMA.")
    p.note(2930, "Число буферов, формат данных и порядок подтверждения задаются реализацией. На схеме показано назначение областей, без фиксированных адресов.")
    p.note(2990, "Профиль задаёт настройки сервиса. Кэш и подготовленный ответ обновляются отдельными действиями; публикация переносит данные в аппаратный буфер.")


def memory_map_sheet():
    p = Page("reference/memory-map.svg", "BRAM: карта памяти реализации",
             "Адреса относятся к аппаратному проекту Burevestnik_21. Смещения внутри окна указаны в байтах.",
             ["src/hardware/pl/i2c/bvstk_i2c_regs.h", "src/hardware/pl/mdio/bvstk_mdio_regs.h",
              "src/hardware/pl/sd/bvstk_sd_master_regs.h", "../hw_platform/fpga/Burevestnik_21.tcl"])
    def bank(key, y, title, subtitle, rows, target):
        h = 115 + len(rows)*160
        p.frame(key, 80, y, 2040, h, title, subtitle=subtitle, target=target)
        for i, (off, name, writer, reader, detail) in enumerate(rows):
            yy = y+100+i*160
            p.node(key+f"-w{i}", 110, yy, 390, 120, writer, layer="L0" if writer == "CPU PS" else "PL",
                   parent=key)
            p.node(key+f"-m{i}", 690, yy, 820, 132, off+"  "+name, detail, parent=key)
            p.node(key+f"-r{i}", 1690, yy, 400, 120, reader, layer="L0" if reader == "CPU PS" else "PL", parent=key)
            p.edge(key+f"-w{i}", key+f"-m{i}", "data", "Запись")
            p.edge(key+f"-m{i}", key+f"-r{i}", "data", "Чтение")
        return y+h
    end = bank("i2c-bank", 270, "BRAM I²C", "0x40000000, окно 16 КиБ; пакетные области по 4 КиБ", [
        ("+0x0000", "Результат master", "I2C_master_0", "CPU PS",
         "Заголовок 4 байта, затем принятые данные"),
        ("+0x1000", "Запрос к slave", "I2C_slave_0", "CPU PS",
         "Заголовок с адресом и длиной, затем полезные данные"),
        ("+0x2000", "Ответ slave", "CPU PS", "I2C_slave_0",
         "PS заполняет окно и устанавливает rd_valid"),
    ], "i2c/subsystem.svg#memory")
    end = bank("mdio-bank", end+110, "BRAM MDIO", "0x42000000, окно 16 КиБ; индекс слова = ((PHY << 5) | REG) × 4", [
        ("+0x0000", "Результаты master", "SMI_master_0", "CPU PS",
         "16-битное значение в 32-битном слове"),
        ("+0x1000", "Последние записи", "SMI_slave_0", "CPU PS",
         "Ячейки по адресу регистра; потреблённая ячейка = 0x00010000"),
        ("+0x2000", "Ответы slave", "CPU PS", "SMI_slave_0",
         "Подготовленное значение; недостоверный ответ = 0xFFFF"),
    ], "mdio/subsystem.svg#memory")
    y = end+110
    p.frame("spi-bank", 80, y, 2040, 510, "BRAM SPI",
            subtitle="Два аппаратных банка. Действующий драйвер SD использует банк 0.",
            target="spi/subsystem.svg#memory")
    p.node("spi-bank0", 110, y+100, 960, 350, "Банк 0 — 0x44000000",
           "Окно 8 КиБ, axi_bram_ctrl_2\nОтветы команд: +0x34, +0x38, +0x3C, +0x44\nПередача: +0x0400\nПриём CMD17: +0x1000, 514 байт\nCPU готовит TX; MM2S читает TX.\nS2MM пишет RX; SPI master пишет ответы команд.",
           parent="spi-bank", target="spi/sd-transfer.svg#memory")
    p.node("spi-bank1", 1260, y+100, 820, 220, "Банк 1 — 0x46000000",
           "Окно 8 КиБ, axi_bram_ctrl_3\nОба AXI DMA имеют адресный доступ к обоим банкам.",
           parent="spi-bank", target="@architecture_bvstk_ps_pl.svg#spi-bank1")
    p.note(y+570, "Программный кэш хранится в памяти PS на L3. BRAM содержит аппаратные буферы и подготовленные ответы; их обновление выполняется отдельными операциями.")


def bridge_subsystems():
    for bus, label, hardware, fabric, bram_ctrl, bram, base in [
        ("i2c", "I²C", "I2C", "axi_interconnect_1", "axi_bram_ctrl_0", "blk_mem_gen_1", "0x40000000"),
        ("mdio", "MDIO", "SMI", "axi_interconnect_0", "axi_bram_ctrl_1", "blk_mem_gen_0", "0x42000000"),
    ]:
        p = Page(f"{bus}/subsystem.svg", f"Подсистема {label}: аппаратура и программный мост",
                 "Роли внешней шины и AXI подписаны отдельно. Оба ядра обращаются к общей BRAM.",
                 ["../hw_platform/fpga/Burevestnik_21.tcl",
                  source(bus, "master", f"top_{'i2c' if bus=='i2c' else 'mdio'}_master"),
                  source(bus, "slave", f"top_{'i2c' if bus=='i2c' else 'mdio'}_slave")])
        p.node("software", 80, 280, 680, 165, "Сервис и драйверы",
               "Политика, кэш и операции с устройством", "L3",
               target=f"ps-interaction/{'i2c-irq' if bus=='i2c' else 'mdio-write'}.svg#service")
        p.node("ps", 1410, 280, 710, 165, "PS и AXI к PL",
               "MMIO к регистрам и окну BRAM", "L0",
               target="@architecture_bvstk_ps_pl.svg#ps-axi")
        p.edge("software", "ps", "mmio", "Через L1: bvstk_mmio")
        p.frame("subsystem", 60, 590, 2080, 1100, f"Подсистема {label}",
                subtitle=f"{hardware}_master_0 и {hardware}_slave_0", target="overview/subsystems.svg#pl-subsystem")
        p.node("master", 140, 705, 760, 205, f"{hardware}_master_0",
               f"Ведущий {label}\nS_AXI: регистры; M_AXI: запись результатов", parent="subsystem",
               target=f"{bus}/master.svg#core")
        p.node("slave", 1300, 705, 760, 205, f"{hardware}_slave_0",
               f"Ведомый {label}\nS_AXI: регистры; M_AXI: запросы и ответы", parent="subsystem",
               target=f"{bus}/slave.svg#core")
        p.edge("ps", "master", "mmio", start="S", end="N", a_offset=-180,
               via=((1585, 535), (520, 535)))
        p.edge("ps", "slave", "mmio", start="S", end="N",
               via=((1765, 535), (1680, 535)))
        p.node("fabric", 640, 1110, 920, 155, fabric,
               "Входы: master, slave и PS", parent="subsystem",
               target="@architecture_bvstk_ps_pl.svg#"+("i2c-fabric" if bus=="i2c" else "smi-fabric"))
        p.edge("master", "fabric", "data", "Результат", start="S", end="N", b_offset=-230,
               via=((520, 1010), (870, 1010)))
        p.edge("slave", "fabric", "data", "Запрос", start="S", end="N", b_offset=230,
               via=((1680, 1030), (1330, 1030)))
        p.edge("fabric", "slave", "data", "Ответ", start="E", end="S", b_offset=235,
               via=((1970, 1187.5), (1970, 975), (1915, 975)))
        p.edge("ps", "fabric", "data", "Доступ CPU к BRAM", start="E", end="E", b_offset=50,
               via=((2180, 362.5), (2180, 1320), (1640, 1320), (1640, 1237.5)),
               label_xy=(1920, 1360), bidirectional=True)
        p.frame("memory", 440, 1380, 1320, 240, f"BRAM {label}", parent="subsystem",
                subtitle=f"{base}, 16 КиБ", target=f"reference/memory-map.svg#{bus}-bank")
        p.node("bram-controller", 485, 1490, 560, 92, bram_ctrl, parent="memory")
        p.node("bram-array", 1175, 1490, 540, 92, bram, parent="memory")
        p.edge("fabric", "memory", "data", start="S", end="N", bidirectional=True)
        p.edge("bram-controller", "bram-array", "data", bidirectional=True)
        p.node("external-target", 140, 1830, 760, 150, "Внешнее ведомое устройство",
               "Физические регистры устройства", "L0")
        p.node("external-host", 1300, 1830, 760, 150, "Внешний ведущий",
               "Инициатор обращения к мосту", "L0")
        p.edge("master", "external-target", "data", label=label, start="W", end="W",
               via=((95, 807.5), (95, 1905)), label_xy=(150, 1750), bidirectional=True)
        p.edge("external-host", "slave", "data", label=label, start="E", end="E",
               via=((2110, 1905), (2110, 807.5)), label_xy=(2030, 1750), bidirectional=True)
        if bus == "mdio":
            p.note(2070, "Программный мост работает в FreeRTOS: входные записи проверяются задачей mdio, ответы внешнему ведущему заранее подготовлены в BRAM.")
        else:
            p.note(2070, "Ведомый запрос обслуживается портом ОС через IRQ. Сервис связывает принятое обращение, политику и кэш с ведущими операциями.")


def i2c_cores():
    p = Page("i2c/master.svg", "I²C master: от команды к результату",
             "Передача через FIFO, управление шиной и запись принятого пакета в BRAM.",
             [source("i2c", "master", n) for n in
              ("top_i2c_master", "regs", "tx_data_ctrl", "i2c_master_engine", "rx_data_ctrl", "rx_fifo_mux")])
    p.node("driver", 600, 275, 1000, 165, "bvstk_i2c_master",
           "Заполнить FIFO, установить START, дождаться завершения", "L3",
           target="@architecture_bvstk_l3.svg#driver-bvstk_i2c_master")
    p.frame("core", 400, 565, 1420, 1590, "I2C_master_0",
            subtitle="top_i2c_master", target="i2c/subsystem.svg#master")
    p.frame("registers", 450, 670, 1320, 220, "Доступ PS к ядру", parent="core")
    p.node("axil-s", 490, 745, 525, 100, "axi_lite_slave_native", parent="registers")
    p.node("regs", 1215, 745, 510, 100, "regs", parent="registers")
    p.edge("driver", "axil-s", "mmio", "Через L1", start="S", end="N",
           via=((1100, 495), (752.5, 495)))
    p.edge("axil-s", "regs", "mmio", "CSR, IRQ, FIFO")
    p.frame("transmit", 450, 1000, 1320, 245, "Подготовка транзакции", parent="core")
    p.node("tx-fifo", 490, 1080, 525, 125, "TX FIFO",
           "xpm_fifo_sync", parent="transmit")
    p.node("tx-ctrl", 1215, 1080, 510, 125, "tx_data_ctrl",
           "Заголовок, адрес, байты", parent="transmit")
    p.edge("regs", "tx-fifo", "data", start="S", end="N",
           via=((1470, 945), (752.5, 945)))
    p.edge("tx-fifo", "tx-ctrl", "data")
    p.node("engine", 1215, 1370, 510, 225, "i2c_master_engine",
           "START и адрес\nЧтение, запись, ACK\nПовторный START и STOP", parent="core")
    p.edge("tx-ctrl", "engine", "data", "Пакеты")
    p.node("device", 1860, 1370, 310, 225, "Устройство",
           "SCL и SDA", "L0")
    p.edge("engine", "device", "data", bidirectional=True)
    p.frame("receive", 450, 1715, 1320, 390, "Сборка принятого пакета", parent="core")
    p.node("rx-ctrl", 1225, 1790, 500, 120, "rx_data_ctrl",
           "Пакет приёма", parent="receive")
    p.node("rx-fifo", 490, 1790, 525, 120, "RX FIFO",
           "xpm_fifo_sync", parent="receive")
    p.node("rx-mux", 490, 1970, 525, 100, "rx_fifo_mux", parent="receive")
    p.node("axil-m", 1225, 1970, 500, 100, "axi_lite_master_native", parent="receive")
    p.edge("engine", "rx-ctrl", "data")
    p.edge("rx-ctrl", "rx-fifo", "data")
    p.edge("rx-fifo", "rx-mux", "data")
    p.edge("rx-mux", "axil-m", "data")
    p.node("memory", 1215, 2310, 730, 165, "BRAM I²C",
           "+0x0000: заголовок и результат", target="reference/memory-map.svg#i2c-bank")
    p.edge("axil-m", "memory", "data", "M_AXI", start="S", end="N")
    p.node("completion", 140, 2310, 730, 195, "Завершение в драйвере",
           "CSR свободен и новый DATA_VALID\nПроверить ERR_ACK, затем читать BRAM", "L3",
           target="@architecture_bvstk_l3.svg#driver-bvstk_i2c_master")
    p.edge("memory", "completion", "data", "Результат")
    p.edge("core", "completion", "mmio", "CSR и IRQ", start="W", end="N", a_offset=480,
           via=((340, 1840), (340, 2200), (505, 2200)), label_xy=(335, 2170))
    p.note(2600, "Готовность FIFO, завершение обмена на шине и наличие нового результата — отдельные условия. Ведущий драйвер проверяет их опросом.")

    p = Page("i2c/slave.svg", "I²C slave: запрос и подготовка ответа",
             "Входной пакет записывается в BRAM. Чтение продолжится после подготовки окна ответа программой PS.",
             [source("i2c", "slave", n) for n in
              ("top_i2c_slave", "i2c_slave_engine", "data_ctrl", "regs")])
    p.node("host", 70, 280, 610, 165, "Внешний ведущий I²C",
           "Адрес, запись, чтение", "L0")
    p.frame("core", 60, 570, 2080, 1440, "I2C_slave_0",
            subtitle="top_i2c_slave", target="i2c/subsystem.svg#slave")
    p.node("engine", 110, 690, 620, 260, "i2c_slave_engine",
           "Распознать адрес из phy_addr_list\nПринять или выдать байт\nУдерживать SCL при ожидании ответа",
           parent="core")
    p.edge("host", "engine", "data", "SCL и SDA", start="S", end="N",
           via=((375, 505), (420, 505)), bidirectional=True, label_xy=(430, 475))
    p.frame("data-ctrl", 1040, 680, 1050, 1240, "data_ctrl",
            parent="core", subtitle="Пакет, указатель памяти и согласование с PS")
    p.node("write-packet", 1090, 800, 950, 165, "Сборка входного пакета",
           "Слова данных, затем заголовок\nWR_MEM_DATA и WR_MEM_HEADER", parent="data-ctrl")
    p.edge("engine", "write-packet", "data", "Приём")
    p.node("write-memory", 1090, 1070, 950, 155, "AXI: записать запрос",
           "BRAM +0x1000, затем DATA_VALID", parent="data-ctrl",
           target="reference/memory-map.svg#i2c-bank")
    p.edge("write-packet", "write-memory", "data")
    p.node("read-request", 1090, 1350, 950, 170, "Запросить ответ",
           "RD_REQ: событие RD_REQUEST\nОжидать control_s.rd_valid", parent="data-ctrl")
    p.edge("engine", "read-request", "data", "Чтение", start="E", end="W",
           a_offset=90, b_offset=-50, via=((980, 910), (980, 1385)),
           label_xy=(990, 1310))
    p.node("read-send", 1090, 1640, 950, 200, "Прочитать и выдать окно",
           "RD_MEM_HEADER, RD_MEM_DATA, RD_SEND_BYTE\nBRAM +0x2000; новое окно при исчерпании данных",
           parent="data-ctrl", target="ps-interaction/reads.svg#i2c")
    p.edge("read-request", "read-send", "data", "rd_valid")
    p.node("regs", 110, 1130, 620, 195, "regs",
           "Маска адресов, CSR и IRQ\nРегистр запроса REQ", parent="core")
    p.node("service", 110, 2190, 620, 240, "Обработчик ведомого запроса",
           "Порт ОС вызывает сервис L3\nСервис готовит содержимое ответа", "L1",
           target="ps-interaction/i2c-irq.svg")
    p.edge("read-request", "regs", "irq", "RD_REQUEST", start="W", end="E", b_offset=45,
           via=((855, 1435), (855, 1272.5)), label_xy=(785, 1400))
    p.edge("service", "regs", "mmio", "rd_valid", start="N", end="S",
           a_offset=-210, b_offset=-240, via=((210, 2080), (180, 2080)), label_xy=(165, 1960))
    p.edge("read-send", "engine", "data", "Байты ответа", start="N", end="S", a_offset=-220, b_offset=230,
           via=((1345, 1570), (915, 1570), (915, 1020), (650, 1020)),
           label_xy=(805, 995))
    p.edge("write-memory", "regs", "irq", "DATA_VALID", start="W", end="E", b_offset=-45,
           via=((795, 1147.5), (795, 1182.5)), label_xy=(815, 1105))
    p.edge("regs", "service", "irq", "IRQ через GIC", start="S", end="N",
           a_offset=180, b_offset=180)
    p.node("bram", 850, 2190, 1280, 165, "BRAM I²C",
           "Окна запроса и ответа раскрыты на карте памяти",
           target="reference/memory-map.svg#i2c-bank")
    p.edge("write-memory", "bram", "data", start="E", end="E",
           via=((2180, 1147.5), (2180, 2272.5)))
    p.edge("service", "bram", "data", start="E", end="W")
    p.edge("bram", "read-send", "data", "Чтение окна", start="N", end="S",
           a_offset=220, b_offset=145, label_xy=(1710, 2040))
    p.note(2540, "Порт ОС находится за границей аппаратного ядра. Он получает событие, вызывает сервис и публикует подготовленное окно ответа.")
    p.note(2600, "Бюджет clock stretching — 100 мкс в штатном XSA. STOP после установки указателя регистра допускается; подготовка отменяется по условиям завершения и ошибки.")


def mdio_cores():
    p = Page("mdio/master.svg", "MDIO master: команда, кадр и результат",
             "Имя аппаратного экземпляра — SMI_master_0. Программная подсистема называется MDIO.",
             [source("mdio", "master", n) for n in
              ("top_mdio_master", "regs", "mdio_data_ctrl", "mdio_master_engine", "rx_fifo_mux")])
    p.node("driver", 630, 270, 940, 175, "bvstk_mdio_core",
           "Записать команду в TX_FIFO\nПроверять CSR и результат", "L3",
           target="@architecture_bvstk_l3.svg#driver-bvstk_mdio_core")
    p.frame("core", 100, 570, 2000, 1550, "SMI_master_0",
            subtitle="top_mdio_master", target="mdio/subsystem.svg#master")
    p.node("regs", 240, 690, 700, 175, "Регистры управления",
           "axi_lite_slave_native и regs\nCSR, TIMEOUT, IRQ, TX_FIFO, MEM_ADDR", parent="core")
    p.edge("driver", "regs", "mmio", "Через L1", start="S", end="N",
           via=((1100, 505), (590, 505)))
    p.node("tx-fifo", 240, 1010, 480, 130, "TX FIFO", "xpm_fifo_sync", parent="core")
    p.edge("regs", "tx-fifo", "data")
    p.frame("data-ctrl", 900, 980, 600, 590, "mdio_data_ctrl", parent="core")
    p.node("dispatch", 940, 1070, 520, 160, "Выдача команды",
           "transaction_view_t\nНаправление, PHY, REG, DATA", parent="data-ctrl")
    p.node("capture", 940, 1350, 520, 150, "Приём результата",
           "Адрес и 16-битное значение", parent="data-ctrl")
    p.edge("tx-fifo", "dispatch", "data")
    p.node("engine", 1660, 1070, 390, 450, "mdio_master_engine",
           "Clause 22\nПреамбула и заголовок\nАдрес PHY и регистра\nTurnaround\n16 бит данных\nФормирование MDC",
           parent="core")
    p.edge("dispatch", "engine", "data", start="E", end="W", b_offset=-145)
    p.edge("engine", "capture", "data", start="W", end="E", a_offset=130)
    p.node("rx-fifo", 240, 1360, 480, 130, "RX FIFO", "xpm_fifo_sync", parent="core")
    p.edge("capture", "rx-fifo", "data")
    p.node("rx-mux", 240, 1770, 660, 200, "rx_fifo_mux",
           "Индекс по PHY и REG\nЗапись результата, DATA_VALID", parent="core")
    p.edge("rx-fifo", "rx-mux", "data")
    p.node("axil-m", 1120, 1770, 800, 200, "axi_lite_master_native",
           "M_AXI к памяти результата", parent="core")
    p.edge("rx-mux", "axil-m", "data")
    p.node("phy", 1600, 2310, 500, 165, "Физический PHY",
           "MDC и MDIO", "L0")
    p.edge("engine", "phy", "data", start="E", end="E",
           via=((2160, 1295), (2160, 2392.5)), label_xy=(2070, 2180), label="Шина",
           bidirectional=True)
    p.node("memory", 180, 2310, 1120, 195, "BRAM MDIO",
           "+0x0000 + ((PHY << 5) | REG) × 4\nРезультат чтения, младшие 16 бит",
           target="reference/memory-map.svg#mdio-bank")
    p.edge("axil-m", "memory", "data", start="S", end="N",
           via=((1520, 2210), (740, 2210)))
    p.note(2590, "Подтверждение результата в PS: прочитать MEM_ADDR, затем записать 1 в IRQ. Драйвер сверяет адрес результата и проверяет DATA_VALID.")
    p.note(2650, "Успех записи означает завершение отправки кадра. Сервис получает фактическое значение отдельным чтением PHY.")

    p = Page("mdio/slave.svg", "MDIO slave: два независимых пути BRAM",
             "Внешнее чтение использует подготовленный ответ. Входящая запись сохраняется для последующей обработки PS.",
             [source("mdio", "slave", n) for n in
              ("top_mdio_slave", "mdio_slave_engine", "mdio_data_ctrl")])
    p.node("host", 760, 260, 680, 165, "Внешний ведущий MDIO",
           "Формирует MDC и кадр Clause 22", "L0")
    p.frame("core", 130, 550, 1940, 1270, "SMI_slave_0",
            subtitle="top_mdio_slave", target="mdio/subsystem.svg#slave")
    p.node("engine", 640, 670, 920, 190, "mdio_slave_engine",
           "Разбор OP, PHY и REG\nПриём записи или выдача 16-битного ответа", parent="core")
    p.edge("host", "engine", "data", "MDC и MDIO", bidirectional=True)
    p.frame("data-ctrl", 180, 1020, 1840, 735, "mdio_data_ctrl",
            parent="core", subtitle="Адресация BRAM, состояние и регистры для PS")
    p.node("write", 250, 1140, 690, 215, "Входящая запись",
           "BRAM +0x1000 + индекс\nПоследнее значение регистра\nПосле записи устанавливается IRQ", parent="data-ctrl")
    p.node("read", 1260, 1140, 690, 185, "Внешнее чтение",
           "BRAM +0x2000 + индекс\nВыдать ранее подготовленное значение", parent="data-ctrl")
    p.edge("engine", "write", "data", "OP = write", start="S", end="N", a_offset=-250,
           via=((850, 940), (595, 940)))
    p.edge("read", "engine", "data", "Ответ", start="N", end="S", b_offset=250,
           via=((1605, 920), (1350, 920)))
    p.edge("engine", "read", "data", "OP = read", start="S", end="N", a_offset=350, b_offset=240,
           via=((1450, 985), (1845, 985)), label_xy=(1780, 969))
    p.node("axi", 640, 1520, 920, 170, "axi_lite_master_native",
           "Запись принятого значения и чтение ответа", parent="data-ctrl")
    p.edge("write", "axi", "data", start="S", end="N", b_offset=-220,
           via=((595, 1430), (880, 1430)))
    p.edge("axi", "read", "data", start="N", end="S", a_offset=220,
           via=((1320, 1430), (1605, 1430)))
    p.edge("read", "axi", "mmio", start="S", end="N", a_offset=250, b_offset=350,
           via=((1855, 1470), (1450, 1470)))
    p.node("memory", 670, 1950, 860, 165, "BRAM MDIO",
           "Отдельные банки принятых записей и ответов",
           target="reference/memory-map.svg#mdio-bank")
    p.edge("axi", "memory", "data", "M_AXI", start="S", end="N", a_offset=-150, b_offset=-150)
    p.edge("memory", "axi", "data", start="N", end="S", a_offset=150, b_offset=150)
    p.node("consume", 110, 2270, 800, 195, "Обработка принятой записи",
           "Задача PS, профиль и политика", "L3",
           target="ps-interaction/mdio-write.svg#service")
    p.node("publish", 1290, 2270, 800, 195, "Подготовка ответов",
           "Обновление кэша и публикация в BRAM", "L3",
           target="ps-interaction/cache.svg#cache")
    p.edge("memory", "consume", "data", start="S", end="N", a_offset=-200,
           via=((900, 2190), (510, 2190)))
    p.edge("publish", "memory", "data", start="N", end="S", b_offset=200,
           via=((1690, 2190), (1300, 2190)))
    p.note(2550, "Ячейка хранит последнее значение. Несколько записей до обработки PS могут слиться; порядок между регистрами из этой памяти восстановить нельзя.")
    p.note(2610, "После потребления PS ставит метку 0x00010000. Запрет политики учитывается программой; отдельного ответа об отказе внешней MDIO-записи нет.")


def spi_sheets():
    p = Page("spi/subsystem.svg", "Подсистема SPI: потоки, DMA и память",
             "Аппаратный состав SPI раскрыт отдельно от его текущего использования для SD.",
             ["../hw_platform/fpga/Burevestnik_21.tcl",
              "src/hardware/boards/ax7020/bvstk_hw_config.h",
              "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.c"])
    p.node("ps", 600, 260, 1000, 170, "PS и AXI к PL",
           "MMIO к регистрам SPI и DMA, доступ к BRAM", "L0",
           target="@architecture_bvstk_ps_pl.svg#ps-axi")
    p.frame("subsystem", 60, 590, 2080, 1320, "Подсистема SPI",
            subtitle="Два ядра, два DMA и общий адресный interconnect",
            target="overview/subsystems.svg#pl-subsystem")
    p.node("master", 150, 710, 680, 220, "SPI_master_0",
           "Ведущий SPI\nAXI Stream в обе стороны\nДополнительный M_AXI для ответов SD",
           parent="subsystem", target="spi/master.svg#core")
    p.node("slave", 1370, 710, 680, 190, "SPI_Slave_0",
           "Ведомый SPI\nAXI Stream в обе стороны", parent="subsystem",
           target="spi/slave.svg#core")
    p.edge("ps", "master", "mmio", start="S", end="N", a_offset=-240,
           via=((860, 505), (490, 505)))
    p.edge("ps", "slave", "mmio", start="S", end="N", a_offset=240,
           via=((1340, 505), (1710, 505)))
    p.frame("dma0", 120, 1110, 790, 310, "axi_dma_0", parent="subsystem",
            subtitle="Simple DMA, SG выключен", target="@architecture_bvstk_ps_pl.svg#dma0")
    p.node("dma0-tx", 155, 1210, 310, 165, "MM2S", "Память → поток", parent="dma0")
    p.node("dma0-rx", 565, 1210, 310, 165, "S2MM", "Поток → память", parent="dma0")
    p.frame("dma1", 1290, 1110, 790, 310, "axi_dma_1", parent="subsystem",
            subtitle="Simple DMA, SG выключен", target="@architecture_bvstk_ps_pl.svg#dma1")
    p.node("dma1-tx", 1325, 1210, 310, 165, "MM2S", "Память → поток", parent="dma1")
    p.node("dma1-rx", 1735, 1210, 310, 165, "S2MM", "Поток → память", parent="dma1")
    p.edge("dma0-tx", "master", "data", "MOSI", start="N", end="S", b_offset=-180)
    p.edge("master", "dma0-rx", "data", "MISO", start="S", end="N", a_offset=230)
    p.edge("dma1-tx", "slave", "data", "MISO", start="N", end="S", b_offset=-230)
    p.edge("slave", "dma1-rx", "data", "MOSI", start="S", end="N", a_offset=180)
    p.node("fabric", 620, 1650, 960, 175, "axi_interconnect_3",
           "Инициаторы: PS, четыре канала DMA, SPI master",
           parent="subsystem", target="@architecture_bvstk_ps_pl.svg#spi-fabric")
    p.edge("dma0", "fabric", "data", start="S", end="N", b_offset=-280,
           via=((515, 1540), (820, 1540)), bidirectional=True)
    p.edge("dma1", "fabric", "data", start="S", end="N", b_offset=280,
           via=((1685, 1540), (1380, 1540)), bidirectional=True)
    p.edge("ps", "fabric", "data", "Доступ CPU к BRAM", start="E", end="E",
           via=((2180, 345), (2180, 1880), (1660, 1880), (1660, 1737.5)),
           label_xy=(1910, 1800), bidirectional=True)
    p.edge("master", "fabric", "data", "M_AXI", start="E", end="N",
           via=((1050, 820), (1050, 1580), (1100, 1580)), label_xy=(1120, 1460))
    p.node("memory", 100, 2050, 1000, 240, "BRAM обмена",
           "Банк 0: 0x44000000, окно 8 КиБ\nБанк 1: 0x46000000, окно 8 КиБ\nДоступны PS и обоим DMA",
           target="reference/memory-map.svg#spi-bank")
    p.node("hp0", 1360, 2050, 740, 180, "S_AXI_HP0",
           "Аппаратный путь к DDR и нижней OCM", "L0",
           target="@architecture_bvstk_ps_pl.svg#hp0")
    p.edge("fabric", "memory", "data", start="S", end="N", a_offset=-220,
           via=((880, 1960), (600, 1960)), bidirectional=True)
    p.edge("fabric", "hp0", "data", start="S", end="N", a_offset=220,
           via=((1320, 1960), (1730, 1960)), bidirectional=True)
    p.frame("freertos", 350, 2460, 1500, 300, "FreeRTOS", "L3", ownership="freertos")
    p.node("sd", 410, 2540, 1380, 165, "Действующий путь SD",
           "bvstk_sd_service: чтение и запись блоков карты", "L3",
           parent="freertos", target="spi/sd-transfer.svg", ownership="freertos")
    p.edge("memory", "sd", "data", start="S", end="N", b_offset=-260,
           via=((600, 2360), (840, 2360)))
    p.note(2850, "Обычный SPI-сервис выключен в профиле AX7020. Универсальный профиль устройств и политика SPI сейчас не подключены.")

    p = Page("spi/master.svg", "SPI master: поток и обработка команд SD",
             "Вложенные блоки показывают состав top_spi_master из ip_repo.",
             [source("spi", "master", n) for n in
              ("top_spi_master", "spi_regs", "spi_master_ctrl", "spi_dma_2words_parser",
               "sd_ini_tx_cmds", "spi_master_engine", "spi_sd_ini")])
    p.node("dma-in", 80, 270, 720, 165, "axi_dma_0: MM2S",
           "Поток команд и данных из памяти", target="spi/subsystem.svg#dma0-tx")
    p.node("ps", 1400, 270, 720, 165, "PS: настройка SPI",
           "Режим, делитель, запуск", "L0", target="@architecture_bvstk_ps_pl.svg#ps-axi")
    p.frame("core", 60, 580, 2080, 1800, "SPI_master_0",
            subtitle="top_spi_master", target="spi/subsystem.svg#master")
    p.node("axis-fifo", 110, 705, 660, 160, "xpm_fifo_axis",
           "Приём AXI Stream, tvalid и tready", parent="core")
    p.node("regs", 1410, 705, 650, 190, "spi_regs",
           "Через axi_lite_slave_native\nCSR, режим, задержка, SD и делитель", parent="core")
    p.edge("dma-in", "axis-fifo", "data")
    p.edge("ps", "regs", "mmio", start="S", end="N",
           via=((1760, 505), (1735, 505)))
    p.frame("ctrl", 100, 1010, 1000, 530, "spi_master_ctrl", parent="core")
    p.node("pack", 145, 1100, 910, 155, "Управление потоком",
           "Разбор слов, выдача байтов, сборка RX", parent="ctrl")
    p.node("parser", 145, 1350, 910, 150, "spi_dma_2words_parser",
           "Два слова: argument, затем opcode и CRC7", parent="ctrl")
    p.edge("axis-fifo", "pack", "data")
    p.edge("pack", "parser", "data")
    p.node("sd-command", 1350, 1090, 710, 230, "sd_ini_tx_cmds",
           "Последовательность команды SD\nОтвет, полезные данные и CRC\nВыбор передачи в engine", parent="core")
    p.edge("parser", "sd-command", "data", start="E", end="W", b_offset=40,
           via=((1180, 1425), (1180, 1245)))
    p.edge("regs", "sd-command", "mmio")
    p.node("engine", 1350, 1650, 710, 195, "spi_master_engine",
           "SCK и CS\nВыдача MOSI, захват MISO", parent="core")
    p.edge("sd-command", "engine", "data", "Передача", start="S", end="N",
           a_offset=-150, b_offset=-150)
    p.edge("engine", "sd-command", "data", "Ответ", start="N", end="S",
           a_offset=150, b_offset=150)
    p.node("init", 130, 1700, 820, 175, "spi_sd_ini",
           "Отдельный режим начальной последовательности SD", parent="core")
    p.edge("regs", "init", "mmio", "sd_ini_en", start="W", end="N",
           via=((1210, 800), (1210, 1600), (540, 1600)), label_xy=(1110, 1620))
    p.node("rx-fifo", 130, 2040, 820, 155, "RX FIFO и axi_stream_master",
           "xpm_fifo_sync, упаковка принятых данных", parent="core")
    p.node("replies", 1330, 2040, 730, 225, "Запись ответов SD",
           "spi_regs и axi_lite_master_native\nОтвет команды в BRAM через M_AXI",
           parent="core", target="reference/memory-map.svg#spi-bank0")
    p.edge("engine", "rx-fifo", "data", "Через spi_master_ctrl", start="S", end="N",
           via=((1705, 1950), (540, 1950)))
    p.edge("sd-command", "replies", "data", "Ответ", start="E", end="E",
           via=((2110, 1205), (2110, 2152.5)), label_xy=(1990, 1940))
    p.node("pins", 1360, 2510, 750, 185, "Внешняя шина SPI",
           "MOSI, MISO, SCK и CS\nПри sd_ini_en выбирается выход spi_sd_ini.", "L0")
    p.edge("engine", "pins", "data", start="E", end="E",
           via=((2180, 1747.5), (2180, 2602.5)), bidirectional=True)
    p.edge("init", "pins", "data", start="W", end="W",
           via=((80, 1787.5), (80, 2440), (1270, 2440), (1270, 2602.5)),
           label="Начальные такты", label_xy=(920, 2420))
    p.node("dma-out", 130, 2510, 820, 155, "axi_dma_0: S2MM",
           "Принятый поток записывается в память", target="spi/subsystem.svg#dma0-rx")
    p.edge("rx-fifo", "dma-out", "data")
    p.note(2800, "Для SD программный драйвер использует отдельный контракт команды и DMA. Порядок запуска и проверка результата раскрыты на листе SD.")

    p = Page("spi/slave.svg", "SPI slave: обмен между шиной и AXI Stream",
             "Внешний ведущий задаёт SCK и CS. Два FIFO развязывают последовательную шину и потоки.",
             [source("spi", "slave", n) for n in
              ("top_spi_slave", "spi_slave_ctrl", "spi_slave_engine", "axi_stream_master", "axi_stream_slave")])
    p.node("host", 660, 270, 880, 165, "Внешний ведущий SPI",
           "SCK, CS и MOSI; принимает MISO", "L0")
    p.frame("core", 80, 590, 2040, 1390, "SPI_Slave_0",
            subtitle="top_spi_slave", target="spi/subsystem.svg#slave")
    p.node("engine", 650, 710, 900, 185, "spi_slave_engine",
           "Сдвиг данных по внешнему SCK\nГраницы байтов и транзакции по CS", parent="core")
    p.edge("host", "engine", "data", "MOSI", start="S", end="N", a_offset=-180, b_offset=-180)
    p.edge("engine", "host", "data", "MISO", start="N", end="S", a_offset=180, b_offset=180)
    p.node("ctrl", 650, 1050, 900, 225, "spi_slave_ctrl",
           "Разбор и сборка 32-битных слов\nCSR, состояние FIFO и IRQ\nДоступ PS через axi_lite_slave_native", parent="core")
    p.edge("engine", "ctrl", "data", "Принятые байты", start="S", end="N", a_offset=-180, b_offset=-180)
    p.edge("ctrl", "engine", "data", "Байты ответа", start="N", end="S", a_offset=180, b_offset=180)
    p.node("rx-from-axis", 180, 1470, 760, 165, "FIFO ответа внешнему ведущему",
           "rx_xpm_fifo_sync_inst в RTL", parent="core")
    p.node("tx-to-axis", 1260, 1470, 760, 165, "FIFO принятого MOSI",
           "tx_xpm_fifo_sync_inst в RTL", parent="core")
    p.edge("rx-from-axis", "ctrl", "data", start="N", end="S", b_offset=-220,
           via=((560, 1370), (880, 1370)))
    p.edge("ctrl", "tx-to-axis", "data", start="S", end="N", a_offset=220,
           via=((1320, 1370), (1640, 1370)))
    p.node("axis-in", 180, 1770, 760, 145, "axi_stream_slave",
           "axi_str_s: данные из DMA", parent="core")
    p.node("axis-out", 1260, 1770, 760, 145, "axi_stream_master",
           "axi_str_m: данные в DMA", parent="core")
    p.edge("axis-in", "rx-from-axis", "data")
    p.edge("tx-to-axis", "axis-out", "data")
    p.node("dma-tx", 180, 2150, 760, 150, "axi_dma_1: MM2S",
           "Читает подготовленные данные из памяти", target="spi/subsystem.svg#dma1-tx")
    p.node("dma-rx", 1260, 2150, 760, 150, "axi_dma_1: S2MM",
           "Записывает принятые данные в память", target="spi/subsystem.svg#dma1-rx")
    p.edge("dma-tx", "axis-in", "data")
    p.edge("axis-out", "dma-rx", "data")
    p.note(2400, "Названия RX и TX в RTL относятся к стороне потоков. Функциональные подписи показывают направление относительно внешней шины.")
    p.note(2460, "На этом листе показана аппаратная возможность. Обслуживание SPI slave отдельной службой текущего BVSTK не подключено.")

    sequence("spi/sd-transfer.svg", "SD через SPI master: чтение блока",
             "Сценарий FreeRTOS, CMD17. Общие драйверы исполняются в этой сборке.",
             [
                 ("service", "Порт SD FreeRTOS", "bvstk_sd_pl и обработчик IRQ", "L1",
                  "@architecture_bvstk_l1.svg#f-media-sd-pl", "freertos"),
                 ("driver", "bvstk_sd_master", "Через bvstk_sd_service", "L3",
                  "@architecture_bvstk_l3.svg#driver-bvstk_sd_master", None),
                 ("memory", "BRAM банка 0", "TX +0x400, RX +0x1000", "PL",
                  "reference/memory-map.svg#spi-bank0", None),
                 ("hardware", "SPI и axi_dma_0", "Потоки и аппаратная команда", "PL",
                  "spi/subsystem.svg#master", None),
             ], [
                 ("service", "driver", "call", "Запросить блок карты", "Сервис выполняет операцию с картой под своей синхронизацией."),
                 ("driver", "hardware", "mmio", "Подготовить SPI и сбросить DMA", "Порт ОС подтверждает старые события и разрешает учёт нового IRQ."),
                 ("driver", "memory", "data", "Записать дескриптор CMD17", "Аргумент, код команды и CRC7; очистить RX и установить защитные слова."),
                 ("driver", "hardware", "mmio", "Сначала запустить приём S2MM", "Адрес BRAM +0x1000, ожидаемая длина 514 байт."),
                 ("driver", "hardware", "mmio", "Запустить MM2S и команду SPI", "TX из BRAM +0x400; CSR получает SDM_START_COMMAND."),
                 ("memory", "hardware", "data", "DMA передаёт команду в SPI master", "Ядро обменивается с картой по внешней шине."),
                 ("hardware", "memory", "data", "Сохранить ответ и принятый блок", "S2MM записывает 512 байт данных и два байта CRC16."),
                 ("hardware", "service", "irq", "Зафиксировать новое аппаратное прерывание", "ISR порта увеличивает счётчик; логика команды остаётся в вызывающем контексте."),
                 ("driver", "hardware", "mmio", "Проверить завершение", "Нужны новый IRQ, обновлённый ответ, завершение MM2S и S2MM."),
                 ("memory", "driver", "data", "Забрать и проверить блок", "Проверить длину, защитные слова, CRC16 и допустимость ответа карты."),
                 ("driver", "service", "data", "Вернуть блок или причину ошибки", "Образование сигнала IRQ само по себе ещё не завершает проверку данных."),
             ], ["src/drivers/pl/sd/bvstk_sd_master.c",
                 "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.c"],
             notes=["На ошибке драйвер выполняет восстановление DMA и аппаратного состояния. Буферы этого пути размещены в BRAM."])


def ps_structural_sheets():
    p = Page("ps-interaction/startup.svg", "Профиль устройства: подготовка подсистемы",
             "Два варианта сборки используют общий формат и разные точки запуска.",
             ["src/shared/config/bvstk_device_profile.c", "src/shared/config/bvstk_config_model.h",
              "src/apps/freertos/runtime/bvstk_runtime.c",
              "src/apps/neutrino/runtime/bvstk_i2c_runtime.c",
              "docs/reference/device-profile-v2.md"])
    p.frame("freertos", 60, 270, 640, 1080, "FreeRTOS", "L4", ownership="freertos")
    p.frame("neutrino", 1500, 270, 640, 1200, "Нейтрино", "L4", ownership="neutrino")
    p.node("f-files", 100, 365, 560, 190, "Профили I²C и MDIO",
           "Файлы устройств в flash:/config\nВстроенный набор при первом запуске", "L4",
           parent="freertos", target="@architecture_bvstk_l4.svg#f-configuration")
    p.node("n-files", 1540, 365, 560, 190, "Профили I²C",
           "Файлы в /flash/config/i2c\nЗагрузчик в bvstkd", "L4",
           parent="neutrino", target="@architecture_bvstk_l4.svg#n-configuration")
    p.node("f-loader", 100, 720, 560, 165, "config_store",
           "Загрузить и проверить файлы", "L4", parent="freertos",
           target="@architecture_bvstk_l4.svg#f-config-store")
    p.node("n-loader", 1540, 720, 560, 180, "bvstk_i2c_config_store",
           "Загрузить и проверить файлы", "L4", parent="neutrino",
           target="@architecture_bvstk_l4.svg#n-config-store")
    p.node("profile", 820, 720, 560, 215, "bvstk_device_profile",
           "Кодек v2\nУстройство, ресурсы, политика\nsettings и параметры опроса", "L4",
           target="ps-interaction/configuration.svg#profile")
    p.edge("f-files", "f-loader", "data")
    p.edge("n-files", "n-loader", "data")
    p.edge("f-loader", "profile", "call")
    p.edge("n-loader", "profile", "call")
    p.node("model", 820, 1080, 560, 205, "Рабочая модель",
           "i2c_device_config_t\nmdio_phy_config_t\nСтруктуры в памяти PS", "L4",
           target="@architecture_bvstk_l4.svg#configuration-model")
    p.edge("profile", "model", "data")
    p.node("f-runtime", 100, 1130, 560, 165, "bvstk_runtime",
           "Ждёт готовность config_store", "L4", parent="freertos",
           target="@architecture_bvstk_l4.svg#f-runtime")
    p.edge("f-loader", "f-runtime", "data", "Готовность")
    p.node("n-runtime", 1540, 1230, 560, 165, "bvstk_i2c_runtime",
           "Инициализация среды I²C", "L4",
           target="@architecture_bvstk_l4.svg#n-runtime", ownership="neutrino", parent="neutrino")
    p.edge("n-loader", "n-runtime", "data", "Модель I²C")
    p.frame("services", 400, 1520, 1400, 600, "Сервисы подсистем", "L3",
            target="@architecture_bvstk_l3.svg#subsystem-services")
    p.node("i2c", 450, 1650, 600, 250, "Среда I²C",
           "bvstk_i2c_devices\nbvstk_i2c_policy и bvstk_i2c_cache\nВедущий и ведомый сервисы", "L3",
           parent="services", target="ps-interaction/i2c-irq.svg#service")
    p.node("mdio", 1220, 1650, 530, 250, "bvstk_mdio_service",
           "Модель PHY и политика\nКэш, settings и список опроса\nЗадача запускается в FreeRTOS", "L3",
           parent="services", target="ps-interaction/mdio-write.svg#service")
    p.node("prepare", 450, 1980, 1300, 100, "Инициализация драйверов и аппаратных окон",
           layer="L3", parent="services", target="overview/memory.svg")
    p.edge("model", "services", "data", "Конфигурации", start="S", end="N")
    p.edge("f-runtime", "services", "call", start="S", end="W",
           via=((380, 1440), (310, 1440), (310, 1820)))
    p.edge("n-runtime", "i2c", "call", "I²C", start="S", end="N",
           via=((1820, 1450), (750, 1450)), label_xy=(1640, 1430))
    p.edge("i2c", "prepare", "call", start="S", end="N", b_offset=-350)
    p.edge("mdio", "prepare", "call", start="S", end="N", b_offset=350)
    p.node("hardware", 610, 2270, 980, 195, "Регистры и BRAM",
           "MMIO через L1\nПодготовка буферов, ответов и источников событий", "PL",
           target="overview/memory.svg")
    p.edge("prepare", "hardware", "mmio")
    p.note(2560, "settings задают начальные значения кэша. Рабочие обмены используют модель в памяти; JSON перед каждой транзакцией не перечитывается.")
    p.note(2620, "Ошибка сохранённого профиля не подменяется разрешающим профилем. В Нейтрино служба MDIO пока не запускается.")

    p = Page("ps-interaction/i2c-irq.svg", "I²C slave: от IRQ к сервису и ответу",
             "Слева FreeRTOS, справа Нейтрино. Общая обработка запроса расположена в центре.",
             ["src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c",
              "src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c",
              "src/services/i2c/bvstk_i2c_slave_service.c",
              "Neutrino 2024: InterruptWait, physical PDF page 6144"])
    p.node("hardware", 800, 270, 600, 185, "I2C_slave_0 и GIC",
           "DATA_VALID или RD_REQUEST\nPL IRQ → IRQ_F2P → GIC", "L0",
           target="i2c/slave.svg#core")
    p.frame("freertos", 60, 580, 620, 1420, "FreeRTOS", "L1", ownership="freertos")
    p.frame("neutrino", 1520, 580, 620, 1420, "Нейтрино", "L1", ownership="neutrino")
    p.node("f-isr", 100, 700, 540, 205, "slave_isr",
           "Захватить и подтвердить IRQ\nСохранить признаки события", "L1",
           parent="freertos", target="@architecture_bvstk_l1.svg#f-interrupt-processing--attach")
    p.node("f-queue", 100, 1070, 540, 195, "Очередь событий",
           "xQueueSendFromISR\nportYIELD_FROM_ISR", "L2", parent="freertos",
           target="@architecture_bvstk_l1.svg#f-interrupt-processing--event")
    p.node("f-worker", 100, 1500, 540, 250, "slave_task",
           "xQueueReceive\nРазобрать флаги\nОбработать запись или запрос чтения", "L1",
           parent="freertos", target="@architecture_bvstk_l1.svg#f-interrupt-processing--worker")
    p.edge("hardware", "f-isr", "irq", start="S", end="N", a_offset=-190,
           via=((910, 520), (370, 520)))
    p.edge("f-isr", "f-queue", "irq", "Событие")
    p.edge("f-queue", "f-worker", "irq", "Пробуждение")
    p.node("n-attach", 1560, 700, 540, 205, "Подключение IRQ",
           "InterruptAttachEvent\nSIGEV_INTR, до начала ожидания", "L1",
           parent="neutrino", target="@architecture_bvstk_l1.svg#n-interrupt-processing--attach")
    p.node("n-kernel", 1560, 1070, 540, 195, "Доставка IRQ",
           "Ядро отмечает событие\nInterruptWait возвращает управление", "L2",
           parent="neutrino", target="@architecture_bvstk_l1.svg#n-interrupt-processing--event")
    p.node("n-worker", 1560, 1500, 540, 270, "Поток обслуживания",
           "Захватить аппаратное состояние\nВызвать сервис под блокировкой\nЗатем InterruptUnmask", "L1",
           parent="neutrino", target="@architecture_bvstk_l1.svg#n-interrupt-processing--worker")
    p.edge("n-attach", "n-kernel", "call", "Подключение события")
    p.edge("hardware", "n-kernel", "irq", start="E", end="W",
           via=((1460, 362.5), (1460, 1167.5)), label_xy=(1430, 985), label="IRQ")
    p.edge("n-kernel", "n-worker", "irq", "Пробуждение")
    p.frame("service", 790, 1480, 620, 515, "Обработка ведомого запроса", "L3",
            target="@architecture_bvstk_l3.svg#service-bvstk_i2c_slave_service")
    p.node("slave-service", 830, 1580, 540, 185, "bvstk_i2c_slave_service",
           "Указатель регистра, запись, окно ответа", "L3", parent="service")
    p.node("master-service", 830, 1840, 540, 115, "Кэш и ведущий сервис",
           layer="L3", parent="service", target="@architecture_bvstk_l3.svg#service-bvstk_i2c_master_service")
    p.edge("f-worker", "slave-service", "call")
    p.edge("n-worker", "slave-service", "call")
    p.edge("slave-service", "master-service", "call")
    p.node("window", 810, 2160, 580, 180, "Окно ответа I²C",
           "Запись BRAM и установка rd_valid\nЧерез драйвер L3 и MMIO L1", "PL",
           target="reference/memory-map.svg#i2c-bank")
    p.edge("master-service", "window", "data", "Подготовленные байты")
    p.node("read", 810, 2490, 580, 165, "I2C_slave_0",
           "Продолжить чтение и выдать байты", target="i2c/slave.svg#read-send")
    p.edge("window", "read", "data")
    p.note(2740, "При входящей записи сервис проверяет политику и вызывает ведущую операцию. В IRQ-контексте FreeRTOS выполняется захват события.")

    p = Page("ps-interaction/mdio-write.svg", "MDIO: входящая запись и решение политики",
             "Сценарий работающего моста FreeRTOS. Запрет и разрешение расходятся до обращения к PHY.",
             ["src/drivers/pl/mdio/bvstk_mdio_core.c", "src/services/mdio/bvstk_mdio_service.c",
              "src/shared/config/bvstk_mdio_config_codec.c", "src/ports/freertos-xilinx/os/mdio/bvstk_mdio_freertos.c"])
    p.node("slave", 830, 265, 640, 165, "SMI_slave_0",
           "Принять запись внешнего ведущего", target="mdio/slave.svg#core")
    p.node("memory", 830, 570, 640, 195, "BRAM MDIO",
           "+0x1000: последнее значение\nЯчейка определяется PHY и REG", target="reference/memory-map.svg#mdio-bank")
    p.edge("slave", "memory", "data")
    p.frame("freertos", 60, 830, 610, 760, "FreeRTOS", "L1", ownership="freertos")
    p.node("worker", 100, 930, 530, 185, "Задача mdio",
           "Циклический вызов service_step", "L1", parent="freertos")
    p.node("kernel", 100, 1280, 530, 190, "Планировщик",
           "vTaskDelay(1) между проходами\nРабота продолжится после ожидания", "L2",
           parent="freertos", target="@architecture_bvstk_l1.svg#f-kernel")
    p.edge("worker", "kernel", "call")
    p.frame("service", 760, 895, 1380, 1185, "bvstk_mdio_service", "L3",
            target="@architecture_bvstk_l3.svg#service-bvstk_mdio_service")
    p.node("take", 830, 1010, 640, 210, "Забрать входную запись",
           "bvstk_mdio_core_take_write\nПрочитать значение, поставить метку\nНайти устройство по PHY", "L3", parent="service")
    p.edge("memory", "take", "data", "Через драйвер и L1")
    p.edge("worker", "take", "call", "service_step")
    p.node("policy", 830, 1370, 640, 210, "Проверить политику",
           "bvstk_mdio_policy_allows\nREG и совпадение VALUE по MASK\nWhitelist или blacklist", "L3",
           parent="service", target="ps-interaction/configuration.svg#policy")
    p.edge("take", "policy", "call")
    p.node("deny", 1670, 1330, 410, 310, "Запрет",
           "BVSTK_ERR_DENIED\nСчётчик denied\nСобытие REG_DENIED\nПередача PHY не запускается",
           "L3", parent="service", target="ps-interaction/interfaces.svg#events")
    p.edge("policy", "deny", "call", "Запрещено")
    p.node("allow", 830, 1760, 640, 220, "Разрешённая запись",
           "Пометить кэш недостоверным\nПодготовить команду для master\nbvstk_mdio_core_write", "L3", parent="service")
    p.edge("policy", "allow", "call", "Разрешено")
    p.node("master", 830, 2230, 640, 180, "SMI_master_0",
           "Передать кадр физическому PHY\nДождаться завершения отправки", target="mdio/master.svg#core")
    p.edge("allow", "master", "mmio", "Через L1", label_xy=(1150, 2160))
    p.node("refresh", 830, 2560, 640, 190, "Прочитать фактическое значение",
           "Обновить кэш и ответ slave\nОтразить результат в наблюдении", "L3",
           target="ps-interaction/cache.svg#cache")
    p.edge("master", "refresh", "data", "Отдельное чтение")
    p.note(2840, "При отсутствии настроенного PHY запрос также отклоняется. У внешней записи MDIO нет отдельного ответа об отказе политики.")
    p.note(2900, "За шаг обрабатывается ограниченное число ячеек. BRAM хранит последние значения; повторные записи до потребления могут быть потеряны.")


def ps_remaining_sheets():
    p = Page("ps-interaction/reads.svg", "Внешнее чтение: откуда появляется ответ",
             "MDIO использует подготовленные значения; I²C может запросить заполнение окна у программы PS.",
             [source("mdio", "slave", "mdio_data_ctrl"),
              source("i2c", "slave", "data_ctrl"),
              "src/services/i2c/bvstk_i2c_slave_service.c"])
    p.frame("mdio", 60, 270, 2080, 600, "MDIO: ответ из BRAM",
            target="mdio/slave.svg#read")
    p.node("m-host", 110, 400, 470, 165, "Внешний ведущий", "Запрос PHY и REG", "L0", parent="mdio")
    p.node("m-slave", 865, 400, 470, 165, "SMI_slave_0", "Адресовать ячейку", parent="mdio",
           target="mdio/slave.svg#core")
    p.node("m-memory", 1620, 400, 470, 195, "BRAM MDIO",
           "+0x2000 + индекс\nПодготовленное значение", parent="mdio",
           target="reference/memory-map.svg#mdio-bank")
    p.edge("m-host", "m-slave", "data", "1. Запрос")
    p.edge("m-slave", "m-memory", "mmio", "2. Адрес ячейки")
    p.edge("m-memory", "m-slave", "data", "3. Значение", start="S", end="S",
           a_offset=-80, b_offset=80, via=((1775, 735), (1180, 735)), label_xy=(1490, 718))
    p.edge("m-slave", "m-host", "data", "4. Ответ", start="S", end="S",
           a_offset=-80, b_offset=80, via=((1020, 765), (425, 765)), label_xy=(720, 748))
    p.node("m-prepare", 1520, 1010, 600, 190, "Обновление подготовленных ответов",
           "PS публикует значения отдельно от внешнего чтения", "L3",
           target="ps-interaction/cache.svg#cache")
    p.edge("m-prepare", "m-memory", "data", start="E", end="E",
           via=((2180, 1105), (2180, 497.5)), label_xy=(2020, 950), label="Публикация")
    p.section(1330, "I²C: программная подготовка окна ответа")
    p.node("i-host", 110, 1410, 490, 165, "Внешний ведущий",
           "Адрес и чтение", "L0")
    p.node("i2c", 850, 1410, 510, 165, "I2C_slave_0",
           "RD_REQUEST и ожидание rd_valid", target="i2c/slave.svg#read-request")
    p.node("i-port", 1600, 1410, 510, 165, "Доставка IRQ",
           "Порт выбранной ОС", "L1", target="ps-interaction/i2c-irq.svg#hardware")
    p.edge("i-host", "i2c", "data", "1. Запрос")
    p.edge("i2c", "i-port", "irq", "2. RD_REQUEST")
    p.node("i-service", 1600, 1750, 510, 205, "bvstk_i2c_slave_service",
           "Указатель регистра\nЗаполнение окна через ведущий сервис", "L3",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_slave_service")
    p.edge("i-port", "i-service", "call", "3. Обработка")
    p.node("i-cache", 850, 1750, 510, 205, "Кэш I²C",
           "Достоверное значение\nПри необходимости — физическое чтение", "L3",
           target="@architecture_bvstk_l3.svg#state-bvstk_i2c_cache")
    p.edge("i-service", "i-cache", "call", "4. Получить значения")
    p.node("i-memory", 110, 1750, 490, 205, "BRAM I²C",
           "+0x2000: заголовок и байты\nПорт устанавливает rd_valid",
           target="reference/memory-map.svg#i2c-bank")
    p.edge("i-cache", "i-memory", "data", "5. Подготовить окно")
    p.node("i-send", 110, 2120, 490, 165, "I2C_slave_0",
           "Прочитать окно ответа", target="i2c/slave.svg#read-send")
    p.node("i-result", 850, 2120, 510, 165, "Внешний ведущий",
           "Получает байты по SDA", "L0")
    p.edge("i-memory", "i-send", "data", "6. Данные")
    p.edge("i-send", "i-result", "data", "7. Ответ")
    p.note(2400, "Для I²C предварительно установленный указатель регистра связывает запись адреса и последующее чтение, включая повторный START.")
    p.note(2460, "Значение из BRAM MDIO отражает последнюю публикацию PS. В I²C доставка IRQ и подготовка ответа ограничены временем clock stretching.")

    sequence("ps-interaction/cache.svg", "MDIO: обновление кэша и ответов slave",
             "Последовательность одного успешного обновления. Фоновый опрос выполняет задача FreeRTOS.",
             [
                 ("worker", "Задача mdio", "Периодический service_step", "L1",
                  "ps-interaction/mdio-write.svg#worker", "freertos"),
                 ("cache", "bvstk_mdio_service", "Кэш и расписание опроса", "L3",
                  "@architecture_bvstk_l3.svg#service-bvstk_mdio_service", None),
                 ("master", "SMI_master_0", "Через bvstk_mdio_core", "PL",
                  "mdio/master.svg#core", None),
                 ("memory", "BRAM MDIO", "Результаты и ответы slave", "PL",
                  "reference/memory-map.svg#mdio-bank", None),
                 ("registry", "bvstk_cache", "Каталог значений для клиентов", "L3",
                  "ps-interaction/interfaces.svg#cache", None),
             ], [
                 ("worker", "cache", "call", "Выбрать очередной шаг фоновой работы", "Учитываются проверка присутствия PHY, список регистров и интервалы профиля."),
                 ("cache", "master", "mmio", "Запустить физическое чтение регистра", "Сервис вызывает драйвер; доступ к аппаратуре выполняется через L1."),
                 ("master", "memory", "data", "Сохранить результат физического чтения", "Банк +0x0000, адрес по PHY и REG."),
                 ("cache", "master", "mmio", "Дождаться нового результата", "Проверить DATA_VALID и адрес результата MEM_ADDR."),
                 ("memory", "cache", "data", "Получить значение из банка результатов", "Это значение прочитано у PHY."),
                 ("cache", "master", "mmio", "Подтвердить полученный результат", "Прочитать MEM_ADDR, затем записать 1 в IRQ."),
                 ("cache", "memory", "data", "Опубликовать ответ для внешнего ведущего", "Записать значение в банк slave +0x2000."),
                 ("cache", "cache", "call", "Обновить программный кэш", "value, valid = 1, seeded = 0, updated_ms."),
                 ("cache", "registry", "data", "Передать изменение в общий каталог", "bvstk_cache_sink и bvstk_cache_bridges."),
                 ("registry", "registry", "data", "Подготовить снимок и изменения для клиентов", "Продолжение — на схеме программных интерфейсов."),
             ], ["src/services/mdio/bvstk_mdio_service.c", "src/services/cache/bvstk_cache_bridges.c"],
             notes=[
                 "При ошибке чтения значение становится недостоверным; в ответ slave публикуется 0xFFFF. Проверка присутствия PHY имеет отдельные правила учёта.",
                 "Начальные settings имеют источник settings. После физического чтения источником становится устройство.",
                 "Кэш I²C также обновляется при операциях сервиса. Отдельный фоновый опрос всех устройств I²C здесь не подразумевается.",
             ])

    p = Page("ps-interaction/interfaces.svg", "Программные входы и наблюдение",
             "Команды обращаются к сервисам. Значения кэша и события имеют свои пути к клиенту.",
             ["src/apps/freertos/console/i2c_shell.c", "src/apps/freertos/console/mdio_shell.c",
              "src/apps/freertos/services/http/http_ui_bus.c", "src/services/control/bvstk_control_api.c",
              "src/apps/neutrino/i2c/bvstk_i2c_resmgr.c", "src/services/cache/bvstk_cache_bridges.c"])
    p.frame("freertos", 60, 270, 670, 2290, "FreeRTOS", "L4", ownership="freertos")
    p.frame("neutrino", 1470, 270, 670, 1130, "Нейтрино", "L4", ownership="neutrino")
    p.node("shell", 100, 380, 590, 190, "Команды устройства",
           "i2c_shell и mdio_shell\nUART, TCP или SSH", "L4", parent="freertos",
           target="@architecture_bvstk_l4.svg#f-application-operations")
    p.node("http", 100, 730, 590, 180, "http_ui_bus",
           "Операции Web UI и снимок устройства", "L4", parent="freertos",
           target="@architecture_bvstk_l4.svg#f-http-commands")
    p.node("dcp2", 100, 1080, 590, 225, "DCP2",
           "Разбор запроса\nbvstk_dcp2_control\nbvstk_control_api", "L3", parent="freertos",
           target="@architecture_bvstk_l3.svg#control-api")
    p.node("n-client", 1510, 380, 590, 175, "Командный клиент I²C",
           "Открытие устройства и devctl", "L4", parent="neutrino",
           target="@architecture_bvstk_l4.svg#n-ipc-client")
    p.node("n-ipc", 1510, 715, 590, 165, "IPC ядра",
           "Передача запроса менеджеру", "L2", parent="neutrino",
           target="@architecture_bvstk_l3.svg#n-kernel")
    p.node("n-manager", 1510, 1040, 590, 215, "bvstk_i2c_resmgr",
           "Менеджер ресурса в bvstkd\nОбращение к общей среде I²C", "L3",
           parent="neutrino", target="@architecture_bvstk_l3.svg#n-resmgr")
    p.edge("n-client", "n-ipc", "call")
    p.edge("n-ipc", "n-manager", "data", "Запрос")
    p.frame("services", 820, 1030, 560, 570, "Сервисы подсистем", "L3",
            target="@architecture_bvstk_l3.svg#subsystem-services")
    p.node("i2c-service", 860, 1140, 480, 180, "Сервис I²C",
           "Модель, политика и кэш\nbvstk_i2c_master_service", "L3", parent="services",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_master_service")
    p.node("mdio-service", 860, 1410, 480, 145, "bvstk_mdio_service",
           "Политика и кэш PHY", "L3", parent="services",
           target="ps-interaction/mdio-write.svg#service")
    p.edge("shell", "services", "call", start="E", end="W", b_offset=-185,
           via=((770, 475), (770, 1130)))
    p.edge("http", "services", "call", start="E", end="W", b_offset=-40,
           via=((745, 820), (745, 1275)))
    p.edge("dcp2", "services", "call", start="E", end="W", b_offset=150,
           via=((795, 1192.5), (795, 1465)))
    p.edge("n-manager", "i2c-service", "call")
    p.node("cache", 820, 1770, 560, 210, "bvstk_cache",
           "Каталог, снимки и изменения\nvalue, valid, source, updated_ms", "L3",
           target="@architecture_bvstk_l3.svg#cache-module")
    p.edge("services", "cache", "data", "Изменения кэша")
    p.node("values", 100, 1780, 590, 225, "Значения для клиента",
           "HTTP action=cache\nDCP2: снимок и изменения\nКарта регистров и view.js в Web UI", "L4",
           parent="freertos", target="ps-interaction/cache.svg#registry")
    p.edge("cache", "values", "data")
    p.node("events", 820, 2190, 560, 210, "События операций",
           "Попытка, отказ, завершение, ошибка\nbvstk_journal", "L3",
           target="@architecture_bvstk_l3.svg#journal-module")
    p.node("notify", 100, 2200, 590, 205, "Наблюдение и уведомления",
           "bvstk_observer и dcp2_notify\nКлиент получает результат операции", "L4",
           parent="freertos", target="@architecture_bvstk_l4.svg#f-observation")
    p.edge("services", "events", "irq", start="E", end="E",
           via=((1430, 1315), (1430, 2295)), label_xy=(1480, 2080), label="События")
    p.edge("events", "notify", "irq")
    p.note(2660, "Общий каталог кэша на этом листе подключён в runtime FreeRTOS. В Нейтрино показан действующий путь I²C через менеджер ресурсов.")
    p.note(2720, "Команды прямого MMIO находятся на L1 и обходят политику устройства. Они раскрыты в общем архитектурном ряду отдельно.")

    sequence("ps-interaction/configuration.svg", "Изменение настроек и сохранение профиля",
             "Сценарий явного изменения и save в FreeRTOS. Применение в RAM и запись файла — отдельные этапы.",
             [
                 ("command", "Обработчик команды", "i2c_shell или mdio_shell", "L4",
                  "@architecture_bvstk_l4.svg#f-application-operations", "freertos"),
                 ("store", "config_store", "Модель и сохранение файла", "L4",
                  "@architecture_bvstk_l4.svg#f-config-store", "freertos"),
                 ("profile", "Кодек профиля v2", "bvstk_device_profile", "L4",
                  "ps-interaction/startup.svg#profile", None),
                 ("policy", "Политика в сервисе", "Рабочие правила и settings", "L3",
                  "ps-interaction/mdio-write.svg#policy", None),
             ], [
                 ("store", "profile", "data", "При загрузке прочитать и разобрать профиль", "Проверить формат, диапазоны, режим политики и поддерживаемый обработчик."),
                 ("profile", "store", "data", "Получить модель конфигурации", "Создание среды исполнения раскрыто на схеме подготовки подсистемы."),
                 ("command", "policy", "call", "Изменить правило или настройку в работающем сервисе", "Проверка и изменение выполняются под синхронизацией сервиса."),
                 ("policy", "command", "data", "Вернуть результат применения в памяти", "Последующие операции используют актуальные рабочие правила."),
                 ("command", "policy", "call", "По команде save получить снимок конфигурации", "Сервис возвращает текущие значения политики и settings."),
                 ("policy", "command", "data", "Передать согласованный снимок", "Кэш физически прочитанных значений не становится автоматически settings."),
                 ("command", "store", "call", "Передать снимок на сохранение", "config_store_save_i2c_device или сохранение MDIO."),
                 ("store", "profile", "call", "Сформировать JSON профиля v2", "Сериализовать выбранную конфигурацию."),
                 ("profile", "store", "data", "Вернуть JSON профиля", "Сериализованные данные передаются в config_store."),
                 ("store", "store", "call", "Сохранить файл через файловый слой L3", "Сохранить предыдущую версию и заменить основной файл."),
                 ("store", "command", "data", "Сообщить результат сохранения", "Ошибка файла не отменяет уже применённое изменение в RAM."),
             ], ["src/apps/freertos/console/i2c_shell.c", "src/apps/freertos/console/mdio_shell.c",
                 "src/apps/freertos/config/config_store.c", "src/shared/config/bvstk_device_profile.c"],
             notes=[
                 "Прямая замена JSON через файловый интерфейс не является командой перезагрузки работающего сервиса.",
                 "Карта register_map.json объясняет физические регистры, а view.js задаёт представление показателей. Решение политики выполняет сервис.",
             ])


SECTIONS = [
    ("Общее устройство", [
        ("overview/bridge.svg", "Участки шины, BRAM и передача либо остановка транзакции"),
        ("overview/subsystems.svg", "Общие роли pl_slave и pl_master, драйвер и сервис"),
        ("overview/exchange.svg", "Ведущая операция PS и обработка внешнего запроса"),
        ("overview/memory.svg", "Память обмена, готовность, профиль и программный кэш"),
    ]),
    ("I²C", [
        ("i2c/subsystem.svg", "Аппаратный состав и программный мост"),
        ("i2c/master.svg", "Командный FIFO, протокол и результат"),
        ("i2c/slave.svg", "Запрос, clock stretching и окно ответа"),
    ]),
    ("MDIO", [
        ("mdio/subsystem.svg", "Master, slave и общая BRAM"),
        ("mdio/master.svg", "Формирование кадра и получение результата"),
        ("mdio/slave.svg", "Последние записи и подготовленные ответы"),
    ]),
    ("SPI и SD", [
        ("spi/subsystem.svg", "Потоки, DMA и адресные пути памяти"),
        ("spi/master.svg", "Потоковый обмен и обработка команд SD"),
        ("spi/slave.svg", "Внешний SCK и два направления AXI Stream"),
        ("spi/sd-transfer.svg", "Действующее чтение блока SD в FreeRTOS"),
    ]),
    ("Карта памяти реализации", [
        ("reference/memory-map.svg", "Адреса BRAM и назначение областей конкретных подсистем"),
    ]),
    ("Взаимодействие с программой PS", [
        ("ps-interaction/startup.svg", "Загрузка профиля и подготовка подсистемы"),
        ("ps-interaction/i2c-irq.svg", "Доставка IRQ I²C в FreeRTOS и Нейтрино"),
        ("ps-interaction/mdio-write.svg", "Входящая запись MDIO и решение политики"),
        ("ps-interaction/reads.svg", "Происхождение ответа на внешнее чтение"),
        ("ps-interaction/cache.svg", "Обновление кэша и публикация в BRAM"),
        ("ps-interaction/interfaces.svg", "Команды, значения и события для клиентов"),
        ("ps-interaction/configuration.svg", "Рабочие настройки и сохранённый профиль"),
    ]),
]


def link_existing_series():
    """Append navigation without changing the existing node and edge geometry."""
    targets = {
        "architecture_bvstk_ps_pl.svg": [
            ("I2C_master_0", "i2c/master.svg#core"), ("I2C_slave_0", "i2c/slave.svg#core"),
            ("SMI_master_0", "mdio/master.svg#core"), ("SMI_slave_0", "mdio/slave.svg#core"),
            ("SPI_master_0", "spi/master.svg#core"), ("SPI_Slave_0", "spi/slave.svg#core"),
        ],
        "architecture_bvstk_l0_l1.svg": [
            ("Области BRAM", "reference/memory-map.svg"), ("Доставка IRQ I²C", "ps-interaction/i2c-irq.svg"),
            ("Чтение блока SD", "spi/sd-transfer.svg"),
        ],
        "architecture_bvstk_l1.svg": [
            ("Доставка IRQ I²C", "ps-interaction/i2c-irq.svg"),
            ("Работа задачи MDIO", "ps-interaction/mdio-write.svg#worker"),
        ],
        "architecture_bvstk_l3.svg": [
            ("bvstk_mdio_service", "ps-interaction/mdio-write.svg#service"),
            ("Кэш и публикация", "ps-interaction/cache.svg#cache"),
            ("Внешнее чтение", "ps-interaction/reads.svg"),
        ],
        "architecture_bvstk_l4.svg": [
            ("Подготовка по профилю", "ps-interaction/startup.svg"),
            ("Программные интерфейсы", "ps-interaction/interfaces.svg"),
            ("Изменение и сохранение", "ps-interaction/configuration.svg"),
        ],
        "levels.svg": [], "architecture_bvstk_os.svg": [],
    }
    for name, entries in targets.items():
        path = ARCH / name
        root = ET.parse(path).getroot()
        for child in list(root):
            if child.get("id") == "pl-subsystem-navigation":
                root.remove(child)
        vb = root.get("viewBox").split()
        width = float(vb[2])
        old_height = float(root.get("data-pl-original-height", vb[3]))
        root.set("data-pl-original-height", str(int(old_height)))
        rows = (len(entries)+2)//3
        height = old_height+90+rows*52
        group = el(root, "g", id="pl-subsystem-navigation", data_kind="navigation")
        el(group, "rect", x=0, y=old_height, width=width, height=height-old_height, fill="white")
        a = el(group, "a", data_navigation="pl-catalog")
        a.set(f"{{{XLINK}}}href", "pl-subsystem/README.md")
        el(a, "text", "Подсистемы PL: устройство и сценарии ↗", x=60, y=old_height+40,
           font_size=22, font_weight=700, style="fill:#8760ba")
        for i, (label, target) in enumerate(entries):
            a = el(group, "a", data_navigation="pl-detail")
            a.set(f"{{{XLINK}}}href", "pl-subsystem/"+target)
            el(a, "text", label+" ↗", x=60+(i % 3)*(width-120)/3,
               y=old_height+92+(i//3)*52, font_size=20, style="fill:#3f75ad")
        root.set("height", str(int(height)))
        root.set("viewBox", f"0 0 {vb[2]} {int(height)}")
        ET.indent(root, space="  ")
        ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def write_readme():
    parts = [
        "# Подсистемы PL\n",
        "Начните со [схемы моста](overview/bridge.svg): внешний ведущий, "
        "BVSTK, ведомое устройство, промежуточная BRAM и политика транзакций. "
        "Затем рассмотрите общие роли, память и состав конкретной шины. "
        "Сценарии PS связывают аппаратуру с профилями устройств, "
        "политикой, кэшем и пользовательскими входами.\n",
        "В `overview` показана функциональная модель без привязки к шине. "
        "`pl_slave` принимает внешние обращения, `pl_master` выполняет ведущие "
        "операции. Эти имена обозначают роли, а не конкретные модули RTL. "
        "Реализация может использовать одну из ролей; состав буферов, политики "
        "и кэша определяется назначением подсистемы.\n",
        "Адреса и форматы областей конкретных подсистем вынесены в "
        "[карту памяти реализации](reference/memory-map.svg).\n",
    ]
    for heading, entries in SECTIONS:
        parts.append("## "+heading+"\n")
        parts.append("\n".join(f"- [{SHEETS[name].title}]({name}) — {description[:1].lower() + description[1:]}."
                               for name, description in entries)+"\n")
    parts += [
        "## Обозначения\n",
        "Цвета L0–L4 совпадают с общим архитектурным рядом. Фиолетовый цвет "
        "обозначает PL внутри L0. При детализации ядра слой остаётся L0. "
        "Роль master или slave относится к явно названному интерфейсу.\n",
        "Серые стрелки обозначают вызовы, коричневые — MMIO и настройку, "
        "синие — данные, красные пунктирные — IRQ и события. Пунктирные "
        "рамки выделяют части FreeRTOS и Нейтрино. Общие исходники включаются "
        "в выбранную сборку отдельно.\n",
        "На последовательностях шаги читаются сверху вниз. Линии под "
        "заголовками обозначают участников; стрелки между ними — отдельные "
        "операции. На структурных схемах направление стрелки обозначает "
        "указанный обмен. Ссылки в заголовках ведут к подробным листам и "
        "схемам общего архитектурного ряда.\n",
        "Стрелка с двумя наконечниками показывает обмен в обоих направлениях. "
        "Небольшой разрыв в месте пересечения отделяет независимые линии.\n",
        "## Состояние реализации\n",
        "Аппаратные схемы основаны на `Burevestnik_21.tcl` и RTL из `ip_repo` "
        "соседнего проекта `hw_platform/fpga`. Программные сценарии основаны "
        "на исходниках BVSTK. В метаданных каждого SVG указан список источников.\n",
        "В FreeRTOS действуют мосты I²C и MDIO, а также путь SD через SPI master "
        "и DMA. В Нейтрино показан действующий путь I²C. Аппаратный SPI slave "
        "раскрыт как часть PL; отдельная служба BVSTK для него не подключена. "
        "Профили и политики устройств SPI не показаны как работающая функция.\n",
        "Адресные окна SPI BRAM доступны обоим DMA. В сценарии SD используется "
        "банк 0; подключение HP0 к памяти PS показано на аппаратных листах.\n",
        "## Основные исходники\n",
        "| Часть описания | Исходник |\n"
        "|---|---|\n"
        "| Состав PL и адресные пути | "
        "[Burevestnik_21.tcl](../../../../../../hw_platform/fpga/Burevestnik_21.tcl) |\n"
        "| Обработка ведомых запросов I²C | "
        "[bvstk_i2c_slave_service.c](../../../../../src/services/i2c/bvstk_i2c_slave_service.c) |\n"
        "| Политика MDIO, опрос и кэш | "
        "[bvstk_mdio_service.c](../../../../../src/services/mdio/bvstk_mdio_service.c) |\n"
        "| Аппаратная команда SD и DMA | "
        "[bvstk_sd_master.c](../../../../../src/drivers/pl/sd/bvstk_sd_master.c) |\n"
        "| Формат профиля устройства | "
        "[bvstk_device_profile.c](../../../../../src/shared/config/bvstk_device_profile.c) |\n",
        "Аппаратный исходник находится в соседнем репозитории. "
        "Полные списки модулей каждого листа сохранены в его метаданных.\n",
        "## Обновление SVG\n",
        "Содержание и геометрия этого комплекта задаются в "
        "[generate_pl_subsystem.py](../../../../../tools/docs/generate_pl_subsystem.py).\n",
        "Из корня репозитория:\n\n```sh\npython3 tools/docs/generate_pl_subsystem.py\n```\n",
        "Скрипт использует стандартную библиотеку Python. Он создаёт SVG и README, "
        "затем добавляет переходы к новым листам в общий архитектурный "
        "ряд. Источники, тексты и координаты редактируются в скрипте.\n",
        "Векторные SVG можно открывать отдельно и включать в статью. "
        "Для работы переходов сохраняйте относительное расположение файлов.\n",
    ]
    (OUT / "README.md").write_text("\n".join(parts), encoding="utf-8")


def main():
    bridge_overview_sheet()
    overview_sheets()
    memory_map_sheet()
    bridge_subsystems()
    i2c_cores()
    mdio_cores()
    spi_sheets()
    ps_structural_sheets()
    ps_remaining_sheets()
    for page in SHEETS.values():
        page.save()
    write_readme()
    link_existing_series()
    placeholder = OUT / ".gitkeep"
    if placeholder.exists():
        placeholder.unlink()
    print(f"Generated {len(SHEETS)} SVG sheets in {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
