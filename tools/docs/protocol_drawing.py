"""Small vector primitives for the SPI and MDIO protocol illustrations."""


class ProtocolFigure:
    def __init__(self, page, element, palette, arrows):
        self.p, self.el, self.arrows = page, element, arrows
        self.ink, self.fill = palette["L0"][1], palette["L0"][0]
        self.blue = arrows["data"][0]
        page.width = 1600
        page.root.set("width", "1600")
        page.background.set("width", "1600")
        page.root.set("data-content", "protocol")
        page.root.set("data-layer", "L0")
        page.used_layers.add("L0")
        page.legend_note = ""

    def text(self, x, y, value, size=24, **options):
        height = self.p.text(self.p.notes, x, y, value, size, **options)
        self.p.max_y = max(self.p.max_y, y + height)

    def line(self, points, color=None, width=3, dash=None, parent=None):
        return self.el(parent if parent is not None else self.p.edges, "path",
                       d="M" + " L".join(f"{x:g},{y:g}" for x, y in points),
                       fill="none", stroke=color or self.ink,
                       stroke_width=width, stroke_dasharray=dash,
                       stroke_linejoin="round")

    def wire(self, x1, x2, y, label, kind="data", reverse=False):
        color = self.arrows[kind][0] if kind in self.arrows else self.ink
        dash = self.arrows["clock"][2] if kind == "clock" else None
        start, end = (x2, x1) if reverse else (x1, x2)
        path = self.line(((start, y), (end, y)), color, dash=dash)
        marker = kind if kind in self.arrows else "clock"
        path.set("marker-end", f"url(#{marker}-arrow)")
        self.text((x1 + x2) / 2, y - 22, label, 26, bold=True,
                  anchor="middle", color=color)

    def heading(self, y, label):
        self.text(60, y, label, 28, bold=True)

    def wave(self, label, y, points, color=None, height=46, label_x=205):
        self.text(label_x, y + 33, label, 25, bold=True, anchor="end",
                  color=color or self.ink)
        self.line([(x, y + (1 - level) * height) for x, level in points],
                  color=color, width=3.5)
        self.p.max_y = max(self.p.max_y, y + height)

    def cell(self, x, y, w, label, detail=None, muted=False, height=80):
        color = self.ink if muted else self.blue
        self.el(self.p.blocks, "rect", x=x, y=y, width=w, height=height,
                rx=5, fill=self.fill if muted else "#f3f8ff",
                stroke=color, stroke_width=2)
        baseline = y + (34 if detail else height / 2 + 9)
        self.text(x + w / 2, baseline, label, 26, bold=True,
                  anchor="middle", color=color)
        if detail:
            self.text(x + w / 2, y + 64, detail, 21, anchor="middle", color=self.ink)

    def bus(self, x1, x2, y, label, muted=False, height=54):
        color = self.ink if muted else self.blue
        self.el(self.p.blocks, "polygon",
                points=f"{x1},{y+height/2} {x1+10},{y} {x2-10},{y} "
                       f"{x2},{y+height/2} {x2-10},{y+height} {x1+10},{y+height}",
                fill=self.fill if muted else "#f3f8ff", stroke=color, stroke_width=2)
        self.text((x1+x2)/2, y+height/2+9, label, 25, anchor="middle", color=color)

    def high_z(self, x1, x2, y):
        self.line(((x1, y+27), (x2, y+27)), dash="7 6", width=2)
        self.text((x1+x2)/2, y+16, "Z", 24, anchor="middle", color=self.ink)

    def bracket(self, x1, x2, y, label, color=None):
        self.line(((x1, y-10), (x1, y), (x2, y), (x2, y-10)), color=color, width=2)
        self.text((x1+x2)/2, y+37, label, 23, anchor="middle", color=color or self.ink)

    def sample(self, x, y1, y2):
        self.line(((x, y1), (x, y2)), color="#adc6e2", width=1.5,
                  dash="5 6", parent=self.p.under)
        self.el(self.p.notes, "circle", cx=x, cy=y2, r=5.5, fill=self.blue)


def clock_cycles(start, count, period, leading, trailing, idle=0, end=None):
    """Digital clock, with edges offset from the start of each bit cell."""
    points = [(start, idle)]
    for bit in range(count):
        x = start + bit * period
        points.extend(((x+leading, idle), (x+leading, 1-idle),
                       (x+trailing, 1-idle), (x+trailing, idle)))
    points.append((end if end is not None else start+count*period, idle))
    return points


def data_bits(start, bits, period, end=None, change_offset=0):
    points = [(start, bits[0])]
    for index, bit in enumerate(bits[1:], 1):
        x = start + index * period + change_offset
        points.extend(((x, bits[index-1]), (x, bit)))
    points.append((end if end is not None else start+len(bits)*period, bits[-1]))
    return points
