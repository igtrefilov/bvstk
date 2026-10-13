"""Shared visual conventions for the bus-to-service article examples."""


def owned(p, key, x, y, w, h, label):
    """The same OS boundary used by the L0–L4 figures."""
    p.frame(key, x, y, w, h, label, None, ownership=key)
    border = list(p.nodes[key].group)[0]
    border.set("rx", "14")
    border.set("stroke", "#8b9ca9")
    border.set("stroke-width", "1.7")
    border.set("stroke-dasharray", "10 8")


def sequence(d, actors, rows, ownership=(), core=(), core_title=""):
    """Keep participants, message labels and OS boundaries consistent."""
    p = d.p
    y = d.actors(actors, y=140, core=core, core_title=core_title)
    for key, label, members in ownership:
        boxes = [p.nodes[name] for name in members]
        left = min(box.x for box in boxes)-18
        right = max(box.x+box.w for box in boxes)+18
        owned(p, key, left, 68, right-left, 250, label)
        for box in boxes:
            p.blocks.remove(box.group)
            p.nodes[key].group.append(box.group)
            box.group.set("data-parent", key)
            box.group.set("data-ownership", key)
    d.sequence(y, rows)
    p.legend_note = "Время идёт сверху вниз; масштаб условный."
    return p


def wire_row(d, key, y, fields, label):
    """One chronological row of wire fields, never a register reference table."""
    p = d.p
    x = 60
    gap = 12
    available = p.width-120-gap*(len(fields)-1)
    weight = sum(item[2] for item in fields)
    d.text(x, y-28, label, 25, bold=True)
    parent = p.blocks
    p.blocks = d.el(parent, "g", id=key, data_kind="wire-row", data_layer="L0")
    for index, (value, meaning, part) in enumerate(fields):
        width = available*part/weight
        d.cell(x, y, width, value, meaning, height=90)
        x += width+gap
    p.blocks = parent
    p.path(key, key, ((60, y+135), (p.width-60, y+135)), "data")
