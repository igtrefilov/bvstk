"""Drawing primitives for article figures about the I2C hardware cores.

Explanations and reference tables live in docs/system/pl-subsystem/i2c/cores.
These helpers draw participants, messages and word layouts without prose
panels or navigation footers.
"""
from i2c_protocol import ProtocolDrawing


class CoreDrawing(ProtocolDrawing):
    def __init__(self, page, element, palette, arrows):
        super().__init__(page, element, palette, arrows)
        self.palette, self.arrows = palette, arrows
        page.root.set("data-content", "article-figure")
        page.legend_note = ""
        page.used_layers.clear()
        self.canvas(1800)

    def canvas(self, width):
        self.p.width = width
        self.p.root.set("width", str(width))
        self.p.background.set("width", str(width))

    def strip(self, key, y, fields, x=70, gap=12):
        """Fit the transaction to the canvas while keeping the fields distinct."""
        available = self.p.width-x-60-gap*(len(fields)-1)
        scale = min(1, available/sum(item["width"] for item in fields))
        fields = [dict(item, width=item["width"]*scale) for item in fields]
        self.p.used_layers.add("L0")
        return super().strip(key, y, fields, x=x, gap=gap)

    def actors(self, specs, y=470, core=(), core_title=""):
        """Draw actual participants and one boundary around the on-core actors."""
        p = self.p
        gap = 48
        available = p.width-120-gap*(len(specs)-1)
        weight = sum(spec[5] for spec in specs)
        locations = {}
        x = 60
        for key, title, body, layer, target, part in specs:
            w = available*part/weight
            locations[key] = x, w
            x += w+gap
        if core:
            first, last = locations[core[0]], locations[core[-1]]
            p.frame("core", first[0]-18, y-64, last[0]+last[1]-first[0]+36,
                    252, core_title)
        self.centers, self.actor_labels = {}, {}
        for key, title, body, layer, target, part in specs:
            x, w = locations[key]
            p.node(key, x, y, w, 155, title, body, layer,
                   parent="core" if key in core else None, target=target)
            self.centers[key] = x+w/2
            self.actor_labels[key] = title, layer
        return y+250

    def sequence(self, y, rows):
        """Label each message at its arrow; the article explains each step."""
        p = self.p
        row_height = 105
        bottom = y+len(rows)*row_height
        for x in self.centers.values():
            self.el(p.under, "path", d=f"M{x},{y-65} V{bottom-15}",
                    fill="none", stroke="#8495a3", stroke_width=1.5,
                    stroke_dasharray="5 9", opacity=".45")
        for index, (key, source, destination, kind, label) in enumerate(rows):
            yy = y+index*row_height
            pairs = list(zip(source, destination)) if isinstance(source, tuple) else [(source, destination)]
            points_x = [self.centers[actor] for pair in pairs for actor in pair]
            left, right = min(points_x), max(points_x)
            label_width = max(right-left-20, 260)
            group = self.el(p.notes, "g", id=key, data_kind="sequence-step")
            self.el(group, "rect", x=(left+right-label_width)/2, y=yy-24,
                    width=label_width, height=57, rx=4, fill="white")
            p.text(group, (left+right)/2, yy, label, 22,
                   anchor="middle", width=label_width, line_height=26,
                   color=self.arrows[kind][0])
            for a, b in pairs:
                xa, xb = self.centers[a], self.centers[b]
                points = ((xa, yy+53), (xb, yy+53)) if a != b else (
                    (xa, yy+43), (xa+70, yy+43), (xa+70, yy+65), (xa, yy+65))
                p.path(a, b, points, kind)
        if len(rows) >= 7:
            for key, (title, layer) in self.actor_labels.items():
                p.text(p.notes, self.centers[key], bottom+20, title, 20,
                       anchor="middle", width=p.nodes[key].w,
                       color=self.palette[layer][1])
            bottom += 50
        p.max_y = max(p.max_y, bottom)
        return bottom

    def word(self, key, y, fields, x=100):
        """Draw named bit ranges; widths reserve space for readable names."""
        p = self.p
        group = self.el(p.blocks, "g", id=key, data_kind="word-layout",
                        data_layer="L0", data_domain="pl")
        total = sum(width for _, _, width in fields)
        for label, bits, width in fields:
            self.el(group, "rect", x=x, y=y, width=width, height=125,
                    fill="white", stroke=self.palette["PL"][1], stroke_width=2)
            p.text(group, x+width/2, y+48, label, 25, bold=True,
                   anchor="middle", color=self.palette["PL"][1], width=width-20)
            p.text(group, x+width/2, y+92, bits, 21, anchor="middle")
            x += width
        p.used_layers.add("PL")
        p.max_y = max(p.max_y, y+145)
        return total

    def memory_format(self, header_fields):
        """Show the header and the byte order as adjacent memory words."""
        self.text(100, 265, "+0x00 — заголовок", 26, bold=True)
        self.word("header", 315, header_fields)
        self.text(100, 525, "+0x04 — первое слово данных", 26, bold=True)
        self.word("packing", 575, [(f"B{i}", f"{8*i+7}…{8*i}", 400) for i in (3, 2, 1, 0)])
        self.text(100, 780, "+0x08 — следующее слово", 26, bold=True)
        self.word("next-word", 830, [(f"B{i}", f"{8*(i-4)+7}…{8*(i-4)}", 400) for i in (7, 6, 5, 4)])
