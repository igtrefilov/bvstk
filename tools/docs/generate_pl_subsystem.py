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
import math
import os
import re
import textwrap
import xml.etree.ElementTree as ET

from i2c_protocol import generate_i2c_protocol
from i2c_slave import generate_i2c_slave
from i2c_master import generate_i2c_master
from spi_protocol import generate_spi_protocol
from mdio_protocol import generate_mdio_protocol
from i2c_drivers import generate_i2c_drivers
from article_flows import generate_article_flows
from mdio_details import generate_mdio_details
from spi_details import generate_spi_details
from transaction_examples import generate_transaction_examples

ROOT = Path(__file__).resolve().parents[2]
ARCH = ROOT / "docs/figures/system/architecture"
OUT = ROOT / "docs/figures/system/pl-subsystem"
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
    if target.startswith(("https://", "http://")):
        return target
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
        self.width = 2200
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
        self.min_y = math.inf
        self.article_notes = []
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
        self.min_y = min(self.min_y, y-size)
        self.max_y = max(self.max_y, y+(len(chunks)-1)*lh+size*.3)
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
            self.links.append({"node": key, "target": href(self.name, target)})
        if subtitle:
            self.text(g, x+26, y+70, subtitle, 18, color="#576c7b", width=w-52)
        self.nodes[key] = Box(key, x, y, w, h, g, label)
        self.min_y = min(self.min_y, y)
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
            self.links.append({"node": key, "target": href(self.name, target)})
            g.set("data-target", href(self.name, target))
        title_y = 58
        if not body:
            title_lines = len(lines(label, w-40, 24))
            title_y = max(58, (h+34-(title_lines-1)*24*1.38)/2)
        title_h = self.text(head, x+20, y+title_y, label, 24, bold=True, width=w-40)
        if body:
            used = self.text(g, x+20, y+64+title_h, body, 19, width=w-40)
            if 64+title_h+used > h+15:
                raise ValueError(f"Text height exceeds {self.name}#{key}: {h}")
        self.nodes[key] = Box(key, x, y, w, h, g, label)
        self.used_layers.add(layer)
        self.min_y = min(self.min_y, y)
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
        self.min_y = min(self.min_y, *(point[1] for point in points))
        self.max_y = max(self.max_y, *(point[1] for point in points))
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
        """Retain source context in metadata; explanations belong in Markdown."""
        self.article_notes.append(text)
        return 0

    def section(self, y, label):
        self.text(self.notes, 60, y, label, 27, bold=True)
        self.max_y = max(self.max_y, y+20)

    def save(self):
        top = max(0, math.floor(self.min_y-40)) if math.isfinite(self.min_y) else 0
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
        legend_note = getattr(self, "legend_note", "Цвет обозначает уровень абстракции.")
        if legend_note:
            self.text(self.notes, 60, y+47, legend_note, 18, color="#576c7b")
        el(self.root, "metadata", json.dumps({
            "generator": "tools/docs/generate_pl_subsystem.py",
            "sources": self.sources, "cross_sheet_links": self.links,
            "article_notes": self.article_notes,
        }, ensure_ascii=False, indent=2), id="provenance")
        height = int(y+(90 if legend_note else 38))
        self.root.set("height", str(height-top))
        self.root.set("viewBox", f"0 {top} {self.width} {height-top}")
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


# Sheet definitions follow. Geometry is explicit and differs with the mechanism.

def transaction_symbol(p, key, cx, cy, direction=1, blocked=False, symbol="T"):
    """Draw a data symbol independently of component and layer colors."""
    g = el(p.notes, "g", id=key, data_kind="transaction",
           data_transaction=symbol, data_state="blocked" if blocked else "forwarded")
    title = "Транзакция T" if symbol == "T" else "Значение V"
    el(g, "title", title+" остановлена" if blocked else title)
    points = ((-46, -29), (25, -29), (46, 0), (25, 29), (-46, 29))
    el(g, "polygon", points=" ".join(f"{cx+direction*x},{cy+y}" for x, y in points),
       fill="white", stroke=ARROWS["data"][0], stroke_width=2.5,
       stroke_linejoin="round")
    p.text(g, cx-4*direction, cy+9, symbol, 27, bold=True,
           color=ARROWS["data"][0], anchor="middle")
    if blocked:
        el(g, "path", d=f"M{cx-29},{cy+28} L{cx+29},{cy-28}",
           fill="none", stroke="white", stroke_width=9)
        el(g, "path", d=f"M{cx-29},{cy+28} L{cx+29},{cy-28}",
           fill="none", stroke="#233747", stroke_width=4)


def bridge_bus(p, key, left, right, participants, label, y=1115):
    """Draw a physical bus; data arrows can run in a separate lane above it."""
    color = PALETTE["L0"][1]
    g = el(p.edges, "g", id=key, data_kind="bus", data_layer="L0",
           data_members=" ".join(participants), data_label=label)
    el(g, "title", label)
    el(g, "path", d=f"M{left},{y} H{right}", fill="none",
       stroke=color, stroke_width=5, stroke_linecap="round")
    for participant in participants:
        x, top = p.nodes[participant].point("S")
        el(g, "path", d=f"M{x},{top} V{y}", fill="none",
           stroke=color, stroke_width=3)
        el(g, "circle", cx=x, cy=y, r=5, fill=color)
    p.text(p.notes, (left+right)/2, y+55, label, 20, bold=True,
           anchor="middle", color=color)


def bridge_overview_sheet():
    """Introduce the bridge without expanding its software and bus internals."""
    p = Page("overview/bridge.svg", "BVSTK: мост между устройствами",
             "Два участка шины соединены через BVSTK. Политика записи решает, выполнять ли изменение устройства.", [
                 "../hw_platform/fpga/Burevestnik_21.tcl",
                 "src/services/i2c/bvstk_i2c_slave_service.c",
                 "src/services/mdio/bvstk_mdio_service.c",
             ])
    p.frame("bridge", 420, 280, 1360, 695, "BVSTK", layer=None,
            target="overview/subsystems.svg")
    p.node("policy", 945, 385, 755, 205, "Политика записи в PS",
           "",
           "L3", parent="bridge", target="overview/subsystems.svg#policy")
    p.node("host", 60, 730, 300, 185, "Внешний\nведущий",
           "", "L0", target="overview/subsystems.svg#host")
    p.node("pl_slave", 500, 730, 290, 185, "pl_slave",
           "", parent="bridge",
           target="overview/subsystems.svg#pl_slave")
    p.node("bram", 935, 730, 330, 185, "BRAM",
           "Запросы и ответы", parent="bridge",
           target="overview/memory.svg#exchange-memory")
    p.node("pl_master", 1430, 730, 270, 185, "pl_master",
           "", parent="bridge",
           target="overview/subsystems.svg#pl_master")
    p.node("device", 1840, 730, 300, 185, "Внешнее ведомое\nустройство",
           "", "L0", target="overview/subsystems.svg#device")
    for a, b in (("pl_slave", "bram"), ("bram", "pl_master")):
        p.edge(a, b, "data", start="E", end="W", bidirectional=True)
    p.edge("bram", "policy", "data", "Запрос T", start="N", end="S",
           b_offset=-222.5, label_xy=(1015, 666))
    p.edge("policy", "pl_master", "mmio", "Разрешено:\nзапустить",
           start="S", end="N", a_offset=242.5, label_xy=(1658, 648))

    bridge_bus(p, "host-bus", 110, 730, ("host", "pl_slave"), "Шина ведущего")
    bridge_bus(p, "device-bus", 1470, 2100, ("pl_master", "device"), "Шина ведомого")
    p.path("host", "pl_slave", ((270, 1068), (585, 1068)), "data")
    p.path("pl_master", "device", ((1625, 1068), (1930, 1068)), "data")
    transaction_symbol(p, "transaction-in", 420, 1068)
    transaction_symbol(p, "transaction-out", 1780, 1068)
    p.text(p.notes, 420, 1020, "Запрос записи", 18, anchor="middle",
           color=ARROWS["data"][0])
    p.text(p.notes, 1780, 1020, "Разрешённая запись", 18, anchor="middle",
           color=ARROWS["data"][0])

    p.path("policy", "transaction-denied", ((945, 487.5), (548, 487.5)),
           "data", "Запрещено", label_xy=(727, 432))
    transaction_symbol(p, "transaction-denied", 727, 487.5, direction=-1, blocked=True)
    el(p.notes, "path", d="M532,463.5 V511.5", stroke="#233747",
       stroke_width=6, stroke_linecap="round")
    p.note(1250, "T — запрос записи: адрес и данные. Шина показана серой линией, движение T — синей стрелкой.")
    p.legend_note = "T — запрос записи. Серая линия — шина."


def cache_overview_sheet():
    """Follow cached values using the physical layout of the bridge overview."""
    p = Page("overview/cache.svg", "BVSTK: кэш и подготовка ответов",
             "Сервис обновляет значения в PS и готовит в BRAM ответы для внешнего ведущего.", [
                 "src/services/i2c/bvstk_i2c_cache.c",
                 "src/services/i2c/bvstk_i2c_master_service.c",
                 "src/services/i2c/bvstk_i2c_slave_service.c",
                 "src/services/mdio/bvstk_mdio_service.c",
             ])
    p.frame("bridge", 420, 280, 1360, 935, "BVSTK", layer=None,
            target="overview/bridge.svg#bridge")
    p.frame("service", 500, 370, 1200, 450, "Сервис и кэш в PS", "L3",
            parent="bridge", target="overview/subsystems.svg#services")
    p.node("cache", 540, 485, 530, 235, "Кэш значений",
           "Значение\nДостоверность",
           "L3", parent="service")
    p.node("operation", 1280, 485, 380, 275, "Обработка операций",
           "",
           "L3", parent="service", target="overview/exchange-refresh.svg#service")
    p.edge("operation", "cache", "data", "Значение\nи состояние", start="W", end="E",
           a_offset=-32.5, b_offset=-12.5, label_xy=(1175, 539))

    p.node("host", 60, 970, 300, 185, "Внешний\nведущий",
           "", "L0", target="overview/bridge.svg#host")
    p.node("pl_slave", 500, 970, 290, 185, "pl_slave",
           "", parent="bridge",
           target="overview/bridge.svg#pl_slave")
    p.node("bram", 935, 970, 330, 185, "BRAM",
           "Запросы и ответы", parent="bridge",
           target="overview/memory.svg#exchange-memory")
    p.node("pl_master", 1430, 970, 270, 185, "pl_master",
           "", parent="bridge",
           target="overview/bridge.svg#pl_master")
    p.node("device", 1840, 970, 300, 185, "Внешнее ведомое\nустройство",
           "Регистр устройства", "L0", target="overview/bridge.svg#device")
    p.edge("pl_slave", "bram", "data", "Запрос", start="E", end="W",
           a_offset=-37.5, b_offset=-37.5, label_xy=(862.5, 1006))
    p.edge("bram", "pl_slave", "data", "Ответ", start="W", end="E",
           a_offset=37.5, b_offset=37.5, label_xy=(862.5, 1132))
    p.edge("pl_master", "bram", "data", "Результат", start="W", end="E",
           label_xy=(1347.5, 1045))
    p.edge("cache", "bram", "data", "Подготовить\nответ", start="S", end="N",
           a_offset=170, b_offset=-125, label_xy=(871, 864))
    p.edge("bram", "operation", "data", "Запрос\nили результат", start="N", end="W",
           a_offset=100, b_offset=92.5, via=((1200, 715),), label_xy=(1310, 864))
    p.edge("operation", "pl_master", "mmio", "Чтение\nили запись", start="S", end="N",
           a_offset=95, label_xy=(1658, 864))

    bridge_bus(p, "host-bus", 110, 730, ("host", "pl_slave"), "Шина ведущего", y=1355)
    bridge_bus(p, "device-bus", 1470, 2100, ("pl_master", "device"), "Шина ведомого", y=1355)
    p.path("pl_slave", "host", ((585, 1308), (270, 1308)), "data")
    p.path("device", "pl_master", ((1930, 1308), (1625, 1308)), "data")
    transaction_symbol(p, "value-reply", 420, 1308, direction=-1, symbol="V")
    transaction_symbol(p, "value-read", 1780, 1308, direction=-1, symbol="V")
    p.text(p.notes, 420, 1260, "Подготовленное значение", 18, anchor="middle",
           color=ARROWS["data"][0])
    p.text(p.notes, 1780, 1260, "Прочитанное значение", 18, anchor="middle",
           color=ARROWS["data"][0])
    p.note(1490, "V — значение. Ответ ведущему отражает последнюю публикацию в BRAM; изменения устройства становятся видны после обновления.")
    p.note(1555, "Опрос по расписанию и перечитывание после записи зависят от подсистемы. Начальные значения могут задаваться профилем.")
    p.note(1620, "При ошибке запись кэша становится недостоверной. Дальнейшее поведение на шине определяется подсистемой.")
    p.legend_note = "V — значение. Серая линия — шина."


def overview_sheets():
    """Give each overview a separate role in the explanation."""
    sources = [
        "../hw_platform/fpga/Burevestnik_21.tcl",
        "src/services/i2c/bvstk_i2c_slave_service.c",
        "src/services/i2c/bvstk_i2c_master_service.c",
        "src/services/mdio/bvstk_mdio_service.c",
        "src/shared/config/bvstk_device_profile.c",
    ]
    subsystem_overview_sheet(sources)
    exchange_overview_sheet(sources)
    memory_overview_sheet(sources)


def subsystem_overview_sheet(sources):
    p = Page("overview/subsystems.svg", "Подсистема PL: состав и ответственность",
             "Какие части принимают решения, обращаются к аппаратуре и выполняют операции на шине.",
             sources)
    p.frame("bridge", 420, 280, 1360, 2130, "BVSTK", layer=None,
            target="overview/bridge.svg#bridge")
    p.node("application", 500, 380, 500, 175, "Прикладные команды",
           "", "L4", parent="bridge",
           target="@architecture_bvstk_l4.svg")
    p.node("profile", 1250, 380, 450, 190, "Профиль подсистемы",
           "", "L4", parent="bridge")
    p.frame("services", 500, 690, 1200, 410, "Сервис подсистемы", "L3",
            parent="bridge", target="@architecture_bvstk_l3.svg#subsystem-services")
    p.node("operation", 540, 795, 530, 220, "Обработка операций",
           "", "L3",
           parent="services", target="overview/exchange-refresh.svg#service")
    p.node("policy", 1280, 780, 380, 110, "Политика в PS",
           layer="L3", parent="services", target="overview/bridge.svg#policy")
    p.node("cache", 1280, 940, 380, 110, "Кэш значений",
           layer="L3", parent="services", target="overview/cache.svg#cache")
    p.edge("application", "operation", "call", start="S", end="N",
           b_offset=90, via=((750, 620), (895, 620)))
    p.edge("profile", "services", "data", "Настройки", start="S", end="N",
           b_offset=375, label_xy=(1590, 642))
    p.edge("operation", "policy", "call", "Проверка", start="E", end="W",
           a_offset=-70, label_xy=(1175, 817))
    p.edge("operation", "cache", "data", "Значения", start="E", end="W",
           a_offset=90, label_xy=(1175, 977), bidirectional=True)

    p.node("driver", 600, 1220, 620, 165, "Драйвер подсистемы",
           "", "L3", parent="bridge",
           target="overview/exchange-refresh.svg#completion")
    p.edge("operation", "driver", "call", "Выполнить операцию", start="S", end="N",
           via=((805, 1150), (910, 1150)), label_xy=(1110, 1170))
    p.node("platform", 600, 1470, 620, 150, "Платформенный доступ",
           "MMIO и события", "L1", parent="bridge",
           target="@architecture_bvstk_l1.svg#platform-access")
    p.edge("driver", "platform", "call")
    p.node("ps", 600, 1705, 620, 155, "Аппаратная часть PS",
           "", "L0", parent="bridge",
           target="@architecture_bvstk_ps_pl.svg#ps-axi")
    p.edge("platform", "ps", "mmio")
    p.node("registers", 1400, 1690, 300, 185, "Регистры ядер",
           "Запуск\nСостояние", parent="bridge",
           target="@architecture_bvstk_ps_pl.svg")
    p.edge("ps", "registers", "mmio", start="E", end="W")

    p.frame("pl-subsystem", 480, 2020, 1260, 330, "Ядра и BRAM",
            parent="bridge", target="@architecture_bvstk_ps_pl.svg")
    p.node("pl_slave", 500, 2110, 290, 185, "pl_slave",
           "", parent="pl-subsystem",
           target="overview/exchange-read.svg#pl_slave")
    p.node("exchange-memory", 935, 2110, 330, 185, "BRAM",
           "Области обмена", parent="pl-subsystem",
           target="overview/memory.svg#exchange-memory")
    p.node("pl_master", 1430, 2110, 270, 185, "pl_master",
           "", parent="pl-subsystem",
           target="overview/exchange-refresh.svg#pl_master")
    p.edge("ps", "exchange-memory", "data", start="S", end="N",
           via=((910, 1990), (1100, 1990)), bidirectional=True)
    p.edge("registers", "pl_slave", "mmio", start="S", end="N", a_offset=-80,
           via=((1470, 1940), (745, 1940)), b_offset=100)
    p.edge("registers", "pl_master", "mmio", start="S", end="N", a_offset=80,
           b_offset=65)
    p.edge("pl_slave", "exchange-memory", "data", start="E", end="W", bidirectional=True)
    p.edge("exchange-memory", "pl_master", "data", start="E", end="W", bidirectional=True)
    p.node("host", 60, 2110, 300, 185, "Внешний\nведущий",
           "", "L0", target="overview/bridge.svg#host")
    p.node("device", 1840, 2110, 300, 185, "Внешнее ведомое\nустройство",
           "", "L0", target="overview/bridge.svg#device")
    bridge_bus(p, "host-bus", 110, 730, ("host", "pl_slave"), "Шина ведущего", y=2550)
    bridge_bus(p, "device-bus", 1470, 2100, ("pl_master", "device"), "Шина ведомого", y=2550)
    p.note(2650, "Программная часть раскрывает управление мостом. Политика и кэш подробно показаны на отдельных листах.")
    p.note(2715, "Показаны обе аппаратные роли. Состав ядер и реализация доступа зависят от подсистемы и выбранной ОС.")


def exchange_overview_sheet(sources):
    from i2c_core import CoreDrawing

    actors = {
        "host": ("Внешний\nведущий", "L0", "overview/bridge.svg#host"),
        "pl_slave": ("pl_slave", "PL", "overview/subsystems.svg#pl_slave"),
        "memory": ("BRAM", "PL", "overview/memory.svg#exchange-memory"),
        "service": ("Сервис PS", "L3", "overview/subsystems.svg#services"),
        "pl_master": ("pl_master", "PL", "overview/subsystems.svg#pl_master"),
        "device": ("Ведомое устройство", "L0", "overview/bridge.svg#device"),
    }

    def scenario(name, title, participants, steps):
        p = Page("overview/" + name + ".svg", title,
                 "Порядок обмена между участниками; подробности приведены в статье.", sources)
        d = CoreDrawing(p, el, PALETTE, ARROWS)
        d.canvas(max(1400, 340 * len(participants)))
        p.root.attrib.pop("data-layer", None)
        specs = [(key, actors[key][0], "", actors[key][1], actors[key][2], 1)
                 for key in participants]
        start = d.actors(specs, y=70)
        d.sequence(start, steps)
        p.legend_note = ""
        return p

    scenario("exchange-read", "Выдача подготовленного ответа",
             ("host", "pl_slave", "memory"), [
        ("read-request", "host", "pl_slave", "data", "Чтение"),
        ("reply-address", "pl_slave", "memory", "mmio", "Адрес ответа"),
        ("reply-buffer", "memory", "pl_slave", "data", "Значение"),
        ("send-reply", "pl_slave", "host", "data", "Ответ"),
    ])
    p = scenario("exchange-refresh", "Обновление значения",
                 ("memory", "service", "pl_master", "device"), [
        ("launch-read", "service", "pl_master", "mmio", "Запуск чтения"),
        ("device-read", "pl_master", "device", "data", "Запрос"),
        ("device-reply", "device", "pl_master", "data", "Значение"),
        ("store-result", "pl_master", "memory", "data", "Результат"),
        ("result-event", "pl_master", "service", "irq", "IRQ, если подключён"),
        ("completion", "service", "pl_master", "mmio", "Проверка состояния"),
        ("result-buffer", "memory", "service", "data", "Результат"),
        ("publish-reply", "service", "memory", "data", "Новый ответ"),
    ])
    p.article_notes.append("Доступ сервиса идёт через драйвер и L1. Момент публикации ответа зависит от подсистемы.")
    p = scenario("exchange-write", "Обработка внешней записи",
                 ("host", "pl_slave", "memory", "service", "pl_master", "device"), [
        ("write-request", "host", "pl_slave", "data", "Запись"),
        ("request-buffer", "pl_slave", "memory", "data", "Запрос"),
        ("request-event", "pl_slave", "service", "irq", "IRQ, если подключён"),
        ("take-request", "memory", "service", "data", "Запрос"),
        ("check-policy", "service", "service", "call", "Политика записи"),
        ("launch-write", "service", "pl_master", "mmio", "Разрешено: запуск"),
        ("device-write", "pl_master", "device", "data", "Запись"),
        ("write-completion", "service", "pl_master", "mmio", "Проверка состояния"),
        ("write-result", "service", "service", "call", "Обновление кэша"),
    ])
    p.article_notes.append("При отказе политики обработка заканчивается до запуска pl_master.")


def memory_overview_sheet(sources):
    p = Page("overview/memory.svg", "Память подсистемы: области обмена",
             "Кто записывает запрос, результат и ответ; когда программа может получить или опубликовать данные.",
             sources)
    p.frame("bridge", 420, 280, 1360, 1310, "BVSTK", layer=None,
            target="overview/bridge.svg#bridge")
    p.frame("driver", 500, 370, 1200, 455, "Драйвер подсистемы", "L3",
            parent="bridge", target="overview/subsystems.svg#driver",
            subtitle="")
    p.node("take-request", 540, 485, 330, 240, "Получить запрос",
           "", "L3",
           parent="driver", target="overview/exchange-write.svg#take-request")
    p.node("publish", 980, 485, 280, 270, "Опубликовать ответ",
           "", "L3",
           parent="driver", target="overview/exchange-refresh.svg#publish-reply")
    p.node("take-result", 1400, 485, 260, 285, "Получить результат",
           "", "L3",
           parent="driver", target="overview/exchange-refresh.svg#result-buffer")

    p.frame("exchange-memory", 800, 1080, 620, 370, "BRAM",
            parent="bridge")
    p.node("request-buffer", 830, 1160, 170, 210, "Запрос",
           "", parent="exchange-memory",
           target="overview/exchange-write.svg#request-buffer")
    p.node("reply-buffer", 1025, 1160, 170, 210, "Ответ",
           "", parent="exchange-memory",
           target="overview/exchange-read.svg#reply-buffer")
    p.node("result-buffer", 1220, 1160, 170, 210, "Результат",
           "", parent="exchange-memory",
           target="overview/exchange-refresh.svg#result-buffer")
    p.node("pl_slave", 480, 1160, 250, 210, "pl_slave",
           "", parent="bridge",
           target="overview/subsystems.svg#pl_slave")
    p.node("pl_master", 1500, 1160, 220, 210, "pl_master",
           "", parent="bridge",
           target="overview/subsystems.svg#pl_master")
    p.node("host", 60, 1160, 300, 210, "Внешний\nведущий",
           "", "L0", target="overview/bridge.svg#host")
    p.node("device", 1840, 1160, 300, 210, "Внешнее ведомое\nустройство",
           "", "L0", target="overview/bridge.svg#device")

    p.edge("pl_slave", "request-buffer", "data", start="E", end="W")
    p.edge("pl_master", "result-buffer", "data", start="W", end="E")
    p.edge("request-buffer", "take-request", "data", "Принятые данные", start="N", end="S",
           a_offset=60, via=((975, 890), (705, 890)), label_xy=(827, 870))
    p.edge("publish", "reply-buffer", "data", "Подготовленный\nответ", start="S", end="N",
           a_offset=-10, label_xy=(1215, 985))
    p.edge("result-buffer", "take-result", "data", "Новый результат", start="N", end="S",
           via=((1305, 910), (1530, 910)), label_xy=(1420, 886))
    p.edge("reply-buffer", "pl_slave", "data", "Ответ ведущему", start="S", end="E",
           b_offset=70, via=((1110, 1520), (770, 1520), (770, 1335)),
           label_xy=(954, 1500))

    bridge_bus(p, "host-bus", 110, 730, ("host", "pl_slave"), "Шина ведущего", y=1730)
    bridge_bus(p, "device-bus", 1470, 2100, ("pl_master", "device"), "Шина ведомого", y=1730)
    p.note(1865, "Названия обозначают назначение областей. Их число, размер и способ размещения определяет реализация.")
    p.note(1930, "Готовность и подтверждение могут храниться в регистрах ядра или в памяти. Конкретный порядок задаёт драйвер.")


def bridge_subsystems():
    """Keep the external host on the same side as in the bridge overview."""
    p = Page("mdio/subsystem.svg", "MDIO: аппаратура и программный мост",
             "Внешний ведущий слева, физический PHY справа. Сервис управляет отдельными обменами через драйвер и BRAM.",
             ["../hw_platform/fpga/Burevestnik_21.tcl",
              "src/services/mdio/bvstk_mdio_service.c",
              "src/drivers/pl/mdio/bvstk_mdio_core.c"])
    p.width = 1800
    p.root.set("width", "1800")
    p.background.set("width", "1800")
    p.legend_note = "Серые линии внизу — два независимых участка MDIO."
    p.frame("software", 595, 70, 610, 510, "Программа PS", "L3")
    p.node("service", 650, 155, 500, 140, "bvstk_mdio_service",
           "Политика и кэш", "L3", parent="software",
           target="mdio/services/step.svg#service")
    p.node("driver", 650, 395, 500, 130, "bvstk_mdio_core",
           "Доступ к PL через L1", "L3", parent="software",
           target="mdio/drivers/platform.svg#driver")
    p.edge("service", "driver", "call", start="S", end="N")
    p.frame("subsystem", 305, 690, 1190, 355, "MDIO", "PL",
            target="@architecture_bvstk_ps_pl.svg#smi-fabric")
    p.node("slave", 350, 790, 300, 180, "SMI_slave_0",
           "", parent="subsystem",
           target="mdio/cores/slave/structure.svg#core")
    p.node("memory", 750, 790, 300, 180, "BRAM MDIO",
           "0x42000000\n16 КиБ", parent="subsystem")
    p.node("master", 1150, 790, 300, 180, "SMI_master_0",
           "", parent="subsystem", target="mdio/cores/master/structure.svg#core")
    p.node("external-host", 30, 790, 235, 180, "Внешний\nведущий", layer="L0")
    p.node("external-target", 1535, 790, 235, 180, "Физический\nPHY", layer="L0")
    p.edge("driver", "slave", "mmio", start="W", end="N",
           via=((500, 460),), label="Регистры", label_xy=(410, 620))
    p.edge("driver", "master", "mmio", start="E", end="N",
           via=((1300, 460),), label="Регистры", label_xy=(1390, 620))
    p.edge("driver", "memory", "data", start="S", end="N", bidirectional=True,
           label="BRAM", label_xy=(950, 650))
    p.edge("slave", "memory", "data", start="E", end="W", bidirectional=True)
    p.edge("master", "memory", "data", start="W", end="E")
    bridge_bus(p, "host-bus", 90, 560, ("external-host", "slave"), "Шина ведущего", y=1160)
    bridge_bus(p, "device-bus", 1240, 1710, ("master", "external-target"), "Шина PHY", y=1160)
    p.path("external-host", "slave", ((147.5, 1105), (500, 1105)), "data", bidirectional=True)
    p.path("master", "external-target", ((1300, 1105), (1652.5, 1105)), "data", bidirectional=True)



def spi_sheets():
    p = Page("spi/subsystem.svg", "Подсистема SPI: потоки, DMA и память",
             "Аппаратный состав SPI раскрыт отдельно от его текущего использования для SD.",
             ["../hw_platform/fpga/Burevestnik_21.tcl",
              "src/hardware/boards/ax7020/bvstk_hw_config.h",
              "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.c"])
    p.node("ps", 600, 260, 1000, 170, "PS и AXI к PL",
           "", "L0",
           target="@architecture_bvstk_ps_pl.svg#ps-axi")
    p.frame("subsystem", 60, 590, 2080, 1320, "Подсистема SPI",
            subtitle="",
            target="overview/subsystems.svg#pl-subsystem")
    p.node("master", 150, 710, 680, 220, "SPI_master_0",
           "",
           parent="subsystem", target="spi/cores/master/structure.svg#core")
    p.node("slave", 1370, 710, 680, 190, "SPI_Slave_0",
           "", parent="subsystem",
           target="spi/cores/slave/structure.svg#core")
    p.edge("ps", "master", "mmio", start="S", end="N", a_offset=-240,
           via=((860, 505), (490, 505)))
    p.edge("ps", "slave", "mmio", start="S", end="N", a_offset=240,
           via=((1340, 505), (1710, 505)))
    p.frame("dma0", 120, 1110, 790, 310, "axi_dma_0", parent="subsystem",
            subtitle="", target="@architecture_bvstk_ps_pl.svg#dma0")
    p.node("dma0-tx", 155, 1210, 310, 165, "MM2S", "Память → поток", parent="dma0")
    p.node("dma0-rx", 565, 1210, 310, 165, "S2MM", "Поток → память", parent="dma0")
    p.frame("dma1", 1290, 1110, 790, 310, "axi_dma_1", parent="subsystem",
            subtitle="", target="@architecture_bvstk_ps_pl.svg#dma1")
    p.node("dma1-tx", 1325, 1210, 310, 165, "MM2S", "Память → поток", parent="dma1")
    p.node("dma1-rx", 1735, 1210, 310, 165, "S2MM", "Поток → память", parent="dma1")
    p.edge("dma0-tx", "master", "data", "AXI Stream", start="N", end="S", b_offset=-180)
    p.edge("master", "dma0-rx", "data", "AXI Stream", start="S", end="N", a_offset=230)
    p.edge("dma1-tx", "slave", "data", "AXI Stream", start="N", end="S", b_offset=-230)
    p.edge("slave", "dma1-rx", "data", "AXI Stream", start="S", end="N", a_offset=180)
    p.node("fabric", 620, 1650, 960, 175, "axi_interconnect_3",
           "",
           parent="subsystem", target="@architecture_bvstk_ps_pl.svg#spi-fabric")
    p.edge("dma0", "fabric", "data", start="S", end="N", b_offset=-280,
           via=((515, 1540), (820, 1540)), bidirectional=True)
    p.edge("dma1", "fabric", "data", start="S", end="N", b_offset=280,
           via=((1685, 1540), (1380, 1540)), bidirectional=True)
    p.edge("ps", "fabric", "data", "Доступ PS", start="E", end="E",
           via=((2180, 345), (2180, 1737.5)),
           label_xy=(1910, 1710), bidirectional=True)
    p.edge("master", "fabric", "data", "M_AXI", start="E", end="N",
           via=((1100, 820),), label_xy=(1190, 1460))
    p.node("memory", 100, 2050, 1000, 240, "BRAM обмена",
           "Банк 0\nБанк 1")
    p.node("hp0", 1360, 2050, 740, 180, "S_AXI_HP0",
           "DDR и OCM", "L0",
           target="@architecture_bvstk_ps_pl.svg#hp0")
    p.edge("fabric", "memory", "data", start="S", end="N", a_offset=-220,
           via=((880, 1960), (600, 1960)), bidirectional=True)
    p.edge("fabric", "hp0", "data", start="S", end="N", a_offset=220,
           via=((1320, 1960), (1730, 1960)), bidirectional=True)



def main():
    bridge_overview_sheet()
    cache_overview_sheet()
    overview_sheets()
    bridge_subsystems()
    generate_i2c_protocol(Page, el, PALETTE, ARROWS)
    generate_i2c_slave(Page, el, PALETTE, ARROWS)
    generate_i2c_master(Page, el, PALETTE, ARROWS)
    generate_i2c_drivers(Page, el, PALETTE, ARROWS)
    generate_article_flows(Page, el, PALETTE, ARROWS)
    generate_spi_protocol(Page, el, PALETTE, ARROWS)
    generate_mdio_protocol(Page, el, PALETTE, ARROWS)
    spi_sheets()
    generate_mdio_details(Page, el, PALETTE, ARROWS)
    generate_spi_details(Page, el, PALETTE, ARROWS)
    generate_transaction_examples(Page, el, PALETTE, ARROWS)
    for page in SHEETS.values():
        page.save()
    placeholder = OUT / ".gitkeep"
    if placeholder.exists():
        placeholder.unlink()
    print(f"Generated {len(SHEETS)} SVG sheets in {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
