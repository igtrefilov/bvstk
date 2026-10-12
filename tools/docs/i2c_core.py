"""Shared layout helpers for the I2C hardware diagrams."""
import textwrap

from i2c_protocol import ProtocolDrawing


class CoreDrawing(ProtocolDrawing):
    def __init__(self, page, element, palette, arrows):
        super().__init__(page, element, palette, arrows)
        self.palette, self.arrows = palette, arrows
        page.root.set("data-content", "hardware-scenario")
        page.legend_note = "Фиолетовые блоки — PL внутри L0. PS показан за границей ядра."

    def finish(self, y, title, target):
        self.link(60, y, title, target)
        self.p.max_y = max(self.p.max_y, y+40)

    def sequence(self, y, rows):
        """Align transfers to participants and repeat names on long sheets."""
        p = self.p
        row_height = 130
        bottom = y+len(rows)*row_height+60*((len(rows)-1)//4)
        for actor, x in self.centers.items():
            layer = self.actor_labels[actor][1]
            self.el(p.under, "path", d=f"M{x},{y-40} V{bottom-25}", fill="none",
                    stroke=self.palette[layer][1], stroke_width=1.5, opacity=".25")
        yy = y
        for index, (key, a, b, kind, title, detail, target) in enumerate(rows, 1):
            if index > 1 and (index-1) % 4 == 0:
                self.el(p.notes, "rect", x=60, y=yy-30, width=2080, height=45, fill="white")
                for actor, (short_title, layer) in self.actor_labels.items():
                    p.text(p.notes, self.centers[actor], yy, short_title, 20, bold=True,
                           anchor="middle", color=self.palette[layer][1])
                yy += 60
            group = self.el(p.notes, "g", id=key, data_kind="sequence-step")
            self.el(group, "rect", x=60, y=yy-29, width=2080, height=64,
                    fill="white", stroke=self.arrows[kind][0], stroke_width=0)
            p.text(group, 66, yy, str(index), 21, bold=True, color=self.arrows[kind][0])
            label = p.link(group, target) if target else group
            p.text(label, 115, yy, title, 22, bold=True, color=self.arrows[kind][0])
            p.text(group, 115, yy+32, detail, 19, color="#576c7b")
            if target:
                p.links.append({"node": key, "target": label.get("{http://www.w3.org/1999/xlink}href")})
            exchanges = zip(a, b) if isinstance(a, tuple) else ((a, b),)
            for source, destination in exchanges:
                xa, xb, line_y = self.centers[source], self.centers[destination], yy+66
                points = ((xa, line_y), (xb, line_y)) if xa != xb else (
                    (xa, line_y), (xa+90, line_y), (xa+90, line_y+18), (xa, line_y+18))
                p.path(source, destination, points, kind)
            yy += row_height
        p.max_y = max(p.max_y, bottom)
        return bottom

    def table(self, key, y, columns, rows):
        """Keep formats and outcome tables separate from component diagrams."""
        p = self.p
        group = self.el(p.blocks, "g", id=key, data_kind="reference-table")
        widths = [column[1] for column in columns]
        x = 70
        for title, width, layer in columns:
            p.text(group, x+14, y, title, 21, bold=True, color=self.palette[layer][1], width=width-28)
            x += width
        yy = y+32
        for row in rows:
            wrapped = []
            for text, width in zip(row, widths):
                lines = []
                for line in text.split("\n"):
                    lines += textwrap.wrap(line, max(8, int((width-28)/(19*.60))),
                                           break_long_words=False, break_on_hyphens=False) or [""]
                wrapped.append(lines)
            height = max(len(lines) for lines in wrapped)*27+38
            self.line(((70, yy), (70+sum(widths), yy)), color="#ced6dd", width=1.2, parent=group)
            x = 70
            for column_index, (lines, width) in enumerate(zip(wrapped, widths)):
                color = self.palette[columns[column_index][2]][1]
                p.text(group, x+14, yy+33, "\n".join(lines), 19, color=color, width=width-28, line_height=27)
                x += width
            yy += height
        self.line(((70, yy), (70+sum(widths), yy)), color="#ced6dd", width=1.2, parent=group)
        p.max_y = max(p.max_y, yy)
        return yy
