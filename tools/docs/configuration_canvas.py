"""Small SVG primitives for the configuration article.

The palette and interaction styles are shared with the system figures.
Coordinates are explicit so a diagram's paths remain independent of its text.
"""
from pathlib import Path
import json
import xml.etree.ElementTree as ET

from generate_pl_subsystem import PALETTE, ARROWS

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/figures/configuration"
NS = "http://www.w3.org/2000/svg"
ET.register_namespace("", NS)


def element(parent, tag, text=None, **attrs):
    node = ET.SubElement(parent, f"{{{NS}}}{tag}", {
        key.replace("_", "-"): str(value)
        for key, value in attrs.items() if value is not None
    })
    node.text = text
    return node


class Figure:
    def __init__(self, name, description, height, width=1600):
        self.name, self.height, self.width = name, height, width
        self.styles = {**ARROWS, "ref": ("#788794", "Связь описаний", "4 5")}
        self.layers, self.arrows, self.boxes = set(), set(), {}
        self.root = ET.Element(f"{{{NS}}}svg", width=str(width),
                               height=str(height + 130),
                               viewBox=f"0 0 {width} {height + 130}",
                               role="img", **{"aria-labelledby": "title desc"})
        element(self.root, "title", description, id="title")
        element(self.root, "desc", description, id="desc")
        defs = element(self.root, "defs")
        element(defs, "style", """
text { font-family: "DejaVu Sans", sans-serif; fill: #233747; }
.edge-label { paint-order: stroke; stroke: white; stroke-width: 7px; stroke-linejoin: round; }
""")
        for kind, (color, _, _) in self.styles.items():
            marker = element(defs, "marker", id=kind, viewBox="0 0 10 10",
                             refX=10, refY=5, markerWidth=11, markerHeight=11,
                             markerUnits="userSpaceOnUse", orient="auto")
            element(marker, "path", d="M0 0 L10 5 L0 10 Z", fill=color)
        element(self.root, "rect", width=width, height=height + 130, fill="white")
        self.regions = element(self.root, "g", id="regions")
        self.blocks = element(self.root, "g", id="blocks")
        self.edges = element(self.root, "g", id="interactions")
        self.labels = element(self.root, "g", id="labels")

    def text(self, x, y, content, size=23, bold=False, color="#233747",
             anchor="start", parent=None, edge=False):
        for i, line in enumerate(str(content).split("\n")):
            element(parent if parent is not None else self.labels, "text", line,
                    x=x, y=y+i*(size+10), font_size=size,
                    font_weight="700" if bold else "400", text_anchor=anchor,
                    style=f"fill:{color}", **({"class": "edge-label"} if edge else {}))

    def frame(self, x, y, w, h, title, layer=None, os=False):
        color = "#8b9ca9" if os else PALETTE[layer][1] if layer else "#607487"
        group = element(self.regions, "g", data_kind="os" if os else "group")
        element(group, "rect", x=x, y=y, width=w, height=h, rx=14,
                fill="white", stroke=color, stroke_width=1.8,
                stroke_dasharray="10 8" if os else None)
        self.text(x+22, y+38, title, 25, True, color, parent=group)
        return group

    def box(self, key, x, y, w, h, title, detail="", layer=None, file=False):
        self.boxes[key] = (x, y, w, h)
        group = element(self.blocks, "g", id=key,
                        data_kind="file" if file else "component" if layer else "data",
                        data_layer=layer)
        fill, stroke = PALETTE[layer] if layer else ("white", "#9baab6")
        if file:
            element(group, "path", d=f"M{x} {y} H{x+w-22} L{x+w} {y+22} V{y+h} H{x} Z",
                    fill="white", stroke="#607487", stroke_width=2)
            element(group, "path", d=f"M{x+w-22} {y} V{y+22} H{x+w}",
                    fill="none", stroke="#607487", stroke_width=2)
        else:
            element(group, "rect", x=x, y=y, width=w, height=h, rx=10,
                    fill=fill, stroke=stroke, stroke_width=2)
        title_y = y+43
        if layer:
            self.layers.add(layer)
            self.text(x+20, y+25, layer, 15, True, stroke, parent=group)
            title_y = y+61
        self.text(x+20, title_y, title, 26, True, parent=group)
        if detail:
            self.text(x+20, title_y+37, detail, 22, parent=group)
        return key

    def folder(self, key, x, y, w, label):
        self.boxes[key] = (x, y, w, 76)
        group = element(self.blocks, "g", id=key, data_kind="directory")
        element(group, "path", d=f"M{x} {y+17} h24 l10 8 h27 v34 h-61 Z",
                fill="#f7f9fb", stroke="#607487", stroke_width=2)
        self.text(x+80, y+48, label, 25, True, parent=group)

    def point(self, key, side):
        x, y, w, h = self.boxes[key]
        return {"N": (x+w/2, y), "S": (x+w/2, y+h),
                "W": (x, y+h/2), "E": (x+w, y+h/2)}[side]

    def edge(self, source, target, kind="data", label="", start="E", end="W", at=None):
        a, b = self.point(source, start), self.point(target, end)
        if a[0] != b[0] and a[1] != b[1]:
            raise ValueError(f"{self.name}: align {source} and {target} before connecting")
        self.path([a, b], kind, label, at)

    def path(self, points, kind="data", label="", at=None):
        color, _, dash = self.styles[kind]
        self.arrows.add(kind)
        element(self.edges, "path", d="M"+" L".join(f"{x:g},{y:g}" for x, y in points),
                fill="none", stroke=color, stroke_width=2.8,
                stroke_dasharray=dash, marker_end=f"url(#{kind})",
                stroke_linejoin="round", data_kind=kind)
        if label:
            if at is None:
                a, b = points[0], points[-1]
                at = ((a[0]+b[0])/2, (a[1]+b[1])/2-18)
            self.text(*at, label, 22, color=color, anchor="middle", edge=True)

    def line(self, points, color="#9baab6", width=2, dashed=False):
        element(self.edges, "path", d="M"+" L".join(f"{x:g},{y:g}" for x, y in points),
                fill="none", stroke=color, stroke_width=width,
                stroke_dasharray="5 8" if dashed else None)

    def step(self, x, y, number):
        element(self.labels, "circle", cx=x, cy=y, r=19, fill="white",
                stroke="#607487", stroke_width=2)
        self.text(x, y+7, number, 21, True, anchor="middle")

    def save(self, note=""):
        legend = element(self.root, "g", id="legend")
        x, y = 50, self.height+30
        for kind in ("call", "data", "mmio", "irq", "ref"):
            if kind not in self.arrows:
                continue
            color, label, dash = self.styles[kind]
            element(legend, "path", d=f"M{x},{y-6} h48", fill="none",
                    stroke=color, stroke_width=2.8, stroke_dasharray=dash,
                    marker_end=f"url(#{kind})")
            self.text(x+62, y, label, 17, color=color, parent=legend)
            x += 100+len(label)*11
        for layer in PALETTE:
            if layer not in self.layers:
                continue
            fill, stroke = PALETTE[layer]
            element(legend, "rect", x=x, y=y-18, width=27, height=20, rx=3,
                    fill=fill, stroke=stroke)
            self.text(x+37, y, layer, 17, color=stroke, parent=legend)
            x += 85
        if note:
            self.text(50, y+48, note, 18, color="#607487", parent=legend)
        metadata = element(self.root, "metadata")
        metadata.text = json.dumps({
            "generator": "tools/docs/configuration_diagrams.py",
            "sources": [
                "src/shared/config/bvstk_device_profile.c",
                "src/shared/config/bvstk_config_model.h",
                "src/services/i2c/bvstk_i2c_master_service.c",
                "src/services/i2c/bvstk_i2c_slave_service.c",
                "src/apps/freertos/config/config_store.c",
                "src/apps/neutrino/config/bvstk_i2c_config_store.c",
                "web/assets/js/register-map.js",
                "web/assets/js/device-view.js",
            ],
        }, ensure_ascii=False)
        path = OUT/(self.name+".svg")
        path.parent.mkdir(parents=True, exist_ok=True)
        ET.indent(self.root)
        ET.ElementTree(self.root).write(path, encoding="utf-8", xml_declaration=True)
