"""Focused article figures: complete exchanges, deadlines and retained state."""


def generate_article_flows(Page, el, palette, arrows):
    def page(name, title, description, sources, width=1400):
        p = Page(name, title, description, sources)
        p.width = width
        p.root.set("width", str(width))
        p.background.set("width", str(width))
        p.legend_note = ""
        return p

    def text(p, x, y, label, size=24, **options):
        return p.text(p.notes, x, y, label, size, **options)

    def line(p, points, color="#a6b1bb", dash=None, width=2, parent=None):
        return el(parent if parent is not None else p.under, "path",
                  d="M" + " L".join(f"{x:g},{y:g}" for x, y in points),
                  stroke=color, stroke_width=width, stroke_dasharray=dash, fill="none")

    def message(p, source, target, x1, x2, y, label, kind="data"):
        p.path(source, target, ((x1, y), (x2, y)), kind)
        text(p, (x1+x2)/2, y-17, label, 23, anchor="middle",
             color=arrows[kind][0], cls="edge-label")

    # The completed example write supplies a later cached read.
    p = page("i2c/exchange.svg", "I²C: запись и последующее чтение",
             "Успешная запись сохраняет 0x3B в кэше. При следующем внешнем чтении PS готовит окно slave из этого значения.",
             ["src/services/i2c/bvstk_i2c_slave_service.c",
              "src/services/i2c/bvstk_i2c_master_service.c"])
    actors = [
        ("host", 150, "Внешний\nведущий", "L0", None),
        ("slave", 425, "I2C_slave_0", "PL", "i2c/cores/slave/structure.svg#core"),
        ("ps", 700, "Сервисы PS", "L3", "i2c/services/read-state.svg#service"),
        ("master", 975, "I2C_master_0", "PL", "i2c/cores/master/structure.svg#core"),
        ("device", 1250, "Устройство", "L0", None),
    ]
    for key, x, label, layer, target in actors:
        p.node(key, x-115, 70, 230, 120, label, layer=layer, target=target)
        line(p, ((x, 215), (x, 1215)), dash="5 8", width=1.5)
    text(p, 70, 255, "Разрешённая запись", 24, bold=True)
    message(p, "ps", "master", 700, 975, 315, "0x10 ← 0x3B", "mmio")
    message(p, "master", "device", 975, 1250, 385, "0x10, 0x3B")
    message(p, "device", "master", 1250, 975, 455, "ACK")
    message(p, "master", "ps", 975, 700, 525, "Запись завершена", "call")
    p.node("cache", 600, 565, 200, 100, "Кэш: 0x3B", layer="L3",
           target="overview/cache.svg#cache")
    line(p, ((65, 705), (1335, 705)), dash="5 8", width=1.5)
    text(p, 70, 753, "Внешнее чтение", 24, bold=True)
    message(p, "host", "slave", 150, 425, 815, "0x10, Sr, R")
    message(p, "slave", "ps", 425, 700, 890, "IRQ", "irq")
    text(p, 700, 924, "Указатель 0x10", 21, anchor="middle", cls="edge-label")
    message(p, "ps", "slave", 700, 425, 990, "Окно в BRAM")
    message(p, "ps", "slave", 700, 425, 1075, "rd_valid", "mmio")
    message(p, "slave", "host", 425, 150, 1170, "ACK, 0x3B")
    # The band denotes physical SCL hold; it is separate from event arrows.
    el(p.under, "rect", x=414, y=825, width=22, height=305, rx=4,
       fill=palette["PL"][0], stroke=palette["PL"][1])
    text(p, 405, 967, "SCL = 0", 20, anchor="end", color=palette["PL"][1], cls="edge-label")
    text(p, 1080, 880, "Шина устройства\nсвободна", 24, anchor="middle", color=palette["L0"][1])
    p.max_y = 1230
    p.legend_note = "Sr — повторный START. R — чтение. Время идёт сверху вниз; масштаб условный."

    # A lower bound already exceeds the configured slave deadline.
    p = page("i2c/miss-deadline.svg", "I²C: промах кэша и срок ответа",
             "При 100 кГц 36 тактов master занимают около 360 мкс. Предел ожидания slave равен 100 мкс даже при немедленном старте master.",
             ["../hw_platform/fpga/ip_repo/i2c_slave/i2c/slave/rtl/i2c_slave_wrapper.sv",
              "../hw_platform/fpga/Burevestnik_21.tcl",
              "src/drivers/pl/i2c/bvstk_i2c_master.c"])
    x0, scale = 440, 2
    deadline, earliest = x0+100*scale, x0+360*scale
    text(p, x0, 100, "RD_REQUEST", 25, anchor="middle", color=arrows["irq"][0])
    line(p, ((x0, 120), (x0, 200)), arrows["irq"][0], "9 6")
    text(p, 65, 245, "I2C_slave_0", 28, bold=True, color=palette["PL"][1])
    text(p, 65, 280, "Первый байт", 23)
    el(p.blocks, "rect", id="slave-wait", x=x0, y=210, width=200, height=80,
       rx=8, fill=palette["PL"][0], stroke=palette["PL"][1], stroke_width=2)
    text(p, x0+100, 258, "SCL = 0", 26, anchor="middle")
    text(p, deadline+25, 258, "NACK адреса", 25, color=arrows["irq"][0])
    line(p, ((deadline, 145), (deadline, 515)), arrows["irq"][0], "9 6")
    text(p, deadline+20, 165, "100 мкс", 24, color=arrows["irq"][0])
    text(p, 65, 410, "I2C_master_0", 28, bold=True, color=palette["PL"][1])
    text(p, 65, 445, "Чтение регистра", 23)
    el(p.blocks, "rect", id="master-read", x=x0, y=375, width=720, height=80,
       rx=8, fill=palette["PL"][0], stroke=palette["PL"][1], stroke_width=2)
    text(p, x0+390, 425, "36 тактов при 100 кГц", 26, anchor="middle")
    text(p, earliest, 495, "≈ 360 мкс", 25, anchor="middle")
    line(p, ((x0, 550), (1240, 550)), palette["L0"][1])
    for micros in range(0, 401, 100):
        x = x0+scale*micros
        line(p, ((x, 540), (x, 560)), palette["L0"][1])
        text(p, x, 595, str(micros), 23, anchor="middle")
    text(p, 1310, 595, "мкс", 23)
    p.used_layers.add("PL")
    p.legend_note = "Оценка без задержек PS. Красный пунктир — конец ожидания slave."

    # State belongs to the service, not to the I2C engine.
    p = page("i2c/services/read-state.svg", "I²C: состояние ведомого сервиса",
             "Запись одного указателя разрешает чтение. Запросы готовят окна. Завершение, ошибка и новая запись снимают прежнее разрешение.",
             ["src/services/i2c/bvstk_i2c_slave_service.c"], width=1200)
    p.frame("service", 60, 80, 1080, 910, "bvstk_i2c_slave_service", layer="L3")
    p.node("unarmed", 370, 195, 420, 145, "Чтение закрыто", "read_armed = 0",
           layer="L3", parent="service")
    p.node("armed", 370, 480, 420, 145, "Чтение разрешено", "read_armed = 1",
           layer="L3", parent="service")
    p.node("window", 370, 785, 420, 140, "Подготовить окно", "Кэш или сервис master",
           layer="L3", parent="service", target="i2c/services/master-operations.svg#read")
    def transition(a, b, points):
        path = line(p, points, arrows["call"][0], width=2.7, parent=p.edges)
        path.set("marker-end", "url(#call-arrow)")
        path.set("data-from", a)
        path.set("data-to", b)
    transition("unarmed", "armed", ((580, 340), (580, 480)))
    text(p, 600, 394, "Только указатель", 24)
    text(p, 600, 428, "Указатель = регистр", 22)
    transition("armed", "window", ((580, 625), (580, 785)))
    text(p, 600, 710, "RD_REQUEST", 24, color=arrows["irq"][0])
    transition("window", "armed", ((790, 855), (1040, 855), (1040, 552.5), (790, 552.5)))
    text(p, 925, 658, "Окно готово\nУказатель += N", 22, anchor="middle")
    transition("armed", "unarmed", ((370, 552.5), (170, 552.5), (170, 267.5), (370, 267.5)))
    text(p, 195, 369, "Завершение,\nошибка,\nновая запись", 22)
    transition("window", "unarmed", ((580, 925), (580, 955), (1100, 955), (1100, 155), (580, 155), (580, 195)))
    text(p, 925, 935, "Ошибка", 23, anchor="middle")
    p.legend_note = "Серые стрелки — переходы. N — длина окна."

    # The master service owns policy and cache decisions; the driver owns I/O.
    p = page("i2c/services/master-operations.svg", "I²C: ведущий сервис",
             "Физическое чтение вызывает драйвер напрямую, запись проходит политику. Успех обновляет кэш, ошибка обмена снимает достоверность; отказ политики сохраняет прежний кэш.",
             ["src/services/i2c/bvstk_i2c_master_service.c",
              "src/services/i2c/bvstk_i2c_policy.c",
              "src/services/i2c/bvstk_i2c_cache.c"], width=1480)
    p.frame("service", 60, 80, 980, 1360, "bvstk_i2c_master_service", layer="L3",
            target="@architecture_bvstk_l3.svg#service-bvstk_i2c_master_service")
    p.node("read", 110, 200, 320, 125, "Физическое чтение", "read_from",
           layer="L3", parent="service")
    p.node("write", 110, 470, 320, 125, "Запись регистра", "write",
           layer="L3", parent="service")
    p.node("policy", 650, 470, 340, 125, "Политика записи", "Регистр и значение",
           layer="L3", parent="service", source="src/services/i2c/bvstk_i2c_policy.c")
    p.node("denied", 650, 690, 340, 150, "Вернуть отказ", "ERR_DENIED",
           layer="L3", parent="service")
    p.node("driver", 1100, 240, 330, 185, "bvstk_i2c_master",
           "hw_read_reg\nhw_write_reg", layer="L3",
           target="i2c/drivers/master/transfer.svg#driver")
    p.node("result", 650, 920, 340, 140, "Результат обмена", "cache_response",
           layer="L3", parent="service")
    p.node("update", 110, 1220, 440, 160, "Обновить кэш",
           "", layer="L3", parent="service",
           target="overview/cache.svg#cache")
    p.node("invalidate", 650, 1220, 340, 160, "Снять достоверность", "valid = 0",
           layer="L3", parent="service", target="overview/cache.svg#cache")
    p.edge("read", "driver", "call", start="E", end="W", b_offset=-70)
    p.edge("write", "policy", "call", start="E", end="W")
    p.edge("policy", "driver", "call", start="N", end="W", b_offset=42.5,
           via=((820, 375),), label="Разрешено", label_xy=(950, 350))
    p.edge("policy", "denied", "call", start="S", end="N",
           label="Запрещено", label_xy=(910, 644))
    p.edge("driver", "result", "data", start="S", end="E",
           via=((1265, 990),))
    p.edge("result", "update", "data", start="W", end="N",
           via=((330, 990),), label="Успех", label_xy=(405, 1140))
    p.edge("result", "invalidate", "data", start="S", end="N",
           label="Ошибка", label_xy=(885, 1140))

    # Values are state intervals, not a table of independent facts.
    p = page("mdio/services/value-lifetime.svg", "MDIO: изменение регистра и его копий",
             "Устройство меняет значение раньше PS. Успешное чтение публикуется в BRAM, затем обновляет кэш сервиса.",
             ["src/services/mdio/bvstk_mdio_service.c"])
    tracks = [("device", 570, "Регистр PHY", "L0", 430),
              ("cache", 900, "Кэш PS", "L3", 865),
              ("bram", 1230, "Ответ BRAM", "PL", 720)]
    for key, x, label, layer, changed in tracks:
        p.node(key, x-125, 80, 250, 105, label, layer=layer)
        fill, color = palette[layer]
        for index, (top, bottom, value) in enumerate(((220, changed, "0x0120"), (changed, 1005, "0x0160"))):
            el(p.blocks, "rect", id=f"{key}-{index}", x=x-85, y=top,
               width=170, height=bottom-top, fill=fill, stroke=color, stroke_width=2)
            text(p, x, top+48, value, 27, bold=True, anchor="middle", color=color)
    for y, label in [(430, "PHY меняет регистр"), (575, "Результат master"),
                     (720, "PS публикует ответ"), (865, "PS обновляет кэш")]:
        line(p, ((65, y), (1350, y)), dash="3 7", width=1.3)
        text(p, 90, y-18-33*(label.count("\n")), label, 24)
    text(p, 65, 290, "После записи\nи чтения", 24)
    line(p, ((40, 230), (40, 1005)), palette["L0"][1], parent=p.edges)
    el(p.edges, "path", d="M34,995 L40,1005 L46,995", stroke=palette["L0"][1], stroke_width=2, fill="none")
    p.max_y = 1025
    p.legend_note = "Время идёт сверху вниз; масштаб условный. Цвет обозначает уровень."
