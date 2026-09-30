"""Draw the introductory I2C sheets using the architecture series renderer.

These are protocol diagrams: fields and signal traces are deliberately drawn
without turning them into software components or implementation-specific RTL.
"""

NXP_SPEC = "https://www.nxp.com/docs/en/user-guide/UM10204.pdf"
TI_INTRO = "https://www.ti.com/lit/an/slva704/slva704.pdf"


def field(label, width=180, driver="ведущий", detail="8 бит", kind="byte", target=None):
    return dict(label=label, width=width, driver=driver, detail=detail, kind=kind, target=target)


def condition(label):
    return field(label, 84, "ведущий", {"S": "START", "Sr": "START", "P": "STOP"}[label], "condition")


def ack(driver="ведомый", nack=False):
    return field("NACK" if nack else "ACK", 94, driver, "1 бит", "ack")


def address(read=False, width=240, example=False):
    operation = "R" if read else "W"
    label = f"0x2A + {operation}" if example else f"Адрес + {operation}"
    detail = ("байт 0x55" if read else "байт 0x54") if example else ("7 бит + 1" if read else "7 бит + 0")
    return field(label, width, "ведущий", detail, target="i2c/protocol/transaction.svg#address")


def omitted(width=66):
    return field("…", width, "", "", "omitted")


class ProtocolDrawing:
    def __init__(self, page, element, palette, arrows):
        self.p, self.el = page, element
        self.ink, self.fill = palette["L0"][1], palette["L0"][0]
        self.data = arrows["data"][0]
        page.used_layers.add("L0")
        page.root.set("data-content", "protocol")
        page.root.set("data-layer", "L0")
        page.legend_note = "L0 — физический интерфейс. Синие поля показывают передаваемые биты. Знак ↗ открывает продолжение."

    def text(self, x, y, value, size=21, **options):
        return self.p.text(self.p.notes, x, y, value, size, **options)

    def line(self, points, color=None, width=3, parent=None, clearance=False, dash=None):
        group = parent if parent is not None else self.p.edges
        path = "M" + " L".join(f"{x:g},{y:g}" for x, y in points)
        if clearance:
            self.el(group, "path", d=path, fill="none", stroke="white", stroke_width=11)
        return self.el(group, "path", d=path, fill="none", stroke=color or self.ink,
                       stroke_width=width, stroke_dasharray=dash, stroke_linejoin="round")

    def heading(self, key, y, title, subtitle=None):
        group = self.el(self.p.notes, "g", id=key, data_kind="protocol-section", data_layer="L0")
        self.el(group, "rect", x=60, y=y-34, width=2080, height=86 if subtitle else 52,
                fill="none", stroke=self.ink, stroke_width=0)
        self.p.text(group, 60, y, title, 28, bold=True)
        if subtitle:
            self.p.text(group, 60, y+40, subtitle, 21, color="#576c7b", width=2080)
        self.p.max_y = max(self.p.max_y, y+50)

    def link(self, x, y, text, target, size=21):
        group = self.p.link(self.p.notes, target)
        self.p.text(group, x, y, text+" ↗", size, bold=True, color=self.data)
        self.p.links.append({"node": "protocol-guide", "target": group.get("{http://www.w3.org/1999/xlink}href")})

    def strip(self, key, y, fields, x=100, gap=12):
        """Place wire fields in time order, with their SDA owner underneath."""
        total = sum(item["width"] for item in fields)+gap*(len(fields)-1)
        if x+total > 2140:
            raise ValueError(f"I2C strip exceeds page width: {self.p.name}#{key}")
        group = self.el(self.p.blocks, "g", id=key, data_kind="protocol-fields", data_layer="L0")
        self.el(group, "rect", x=x-12, y=y-12, width=total+24, height=150,
                fill="none", stroke=self.ink, stroke_width=0)
        positions = []
        for index, item in enumerate(fields):
            w, kind = item["width"], item["kind"]
            cell = self.el(group, "g", id=f"{key}-{index}", data_kind="protocol-field",
                           data_label=item["label"], data_driver=item["driver"])
            if kind == "condition":
                self.el(cell, "polygon", points=f"{x},{y+12} {x+12},{y} {x+w-12},{y} {x+w},{y+12} {x+w},{y+78} {x+w-12},{y+90} {x+12},{y+90} {x},{y+78}",
                        fill=self.fill, stroke=self.ink, stroke_width=2)
            elif kind != "omitted":
                self.el(cell, "rect", x=x, y=y, width=w, height=90, rx=5,
                        fill="white", stroke=self.data, stroke_width=2)
            content = self.p.link(cell, item["target"]) if item["target"] else cell
            self.p.text(content, x+w/2, y+37, item["label"], 24 if kind != "ack" else 20,
                        bold=True, anchor="middle", color=self.ink if kind == "condition" else self.data)
            if item["detail"]:
                self.p.text(content, x+w/2, y+69, item["detail"], 17, anchor="middle", color="#576c7b")
            if item["driver"]:
                self.p.text(cell, x+w/2, y+120, item["driver"], 17, anchor="middle", color=self.ink)
            if item["target"]:
                self.p.links.append({"node": f"{key}-{index}", "target": content.get("{http://www.w3.org/1999/xlink}href")})
            positions.append((x, x+w))
            x += w+gap
        self.p.max_y = max(self.p.max_y, y+150)
        return positions

    def trace(self, name, x, y, points, color=None):
        self.text(x-30, y+36, name, 22, bold=True, anchor="end", color=color or self.ink)
        self.line([(x+xx, y+(0 if level else 60)) for xx, level in points], color=color, width=3.5)

    def bracket(self, x1, x2, y, text):
        self.line(((x1, y-9), (x1, y), (x2, y), (x2, y-9)), width=1.8)
        self.text((x1+x2)/2, y+34, text, 20, anchor="middle", color=self.ink)

    def footer(self, y, next_title, next_target):
        self.link(60, y, "NXP UM10204: раздел 3.1", NXP_SPEC, 18)
        self.link(655, y, "TI: Understanding the I²C Bus", TI_INTRO, 18)
        self.link(60, y+64, next_title, next_target)
        self.p.max_y = max(self.p.max_y, y+94)


def interface_sheet(d):
    p, el = d.p, d.el
    d.heading("bus", 290, "Две общие линии", "SDA передаёт данные. SCL задаёт такты; ведущий формирует их и при чтении, и при записи.")
    # Shared wires, individual pull-ups and three addressed participants.
    d.line(((230, 440), (2090, 440)), width=4)
    d.line(((230, 515), (2090, 515)), width=4)
    d.text(90, 448, "SDA", 25, bold=True)
    d.text(90, 523, "SCL", 25, bold=True)
    for x, line_y, name in ((1970, 440, "R₁"), (2070, 515, "R₂")):
        d.line(((x, 340), (x, 363)))
        el(p.edges, "rect", x=x-12, y=363, width=24, height=48, fill="white", stroke=d.ink, stroke_width=2.5)
        d.line(((x, 411), (x, line_y)), clearance=True)
        el(p.edges, "circle", cx=x, cy=line_y, r=5, fill=d.ink)
        d.text(x-20, 391, name, 17, anchor="end")
    d.line(((1970, 340), (2070, 340)))
    d.text(2020, 315, "VDD", 22, bold=True, anchor="middle")
    for key, x, w, title, body in (
        ("controller", 260, 470, "Ведущий", "Начинает и завершает обмен\nВыбирает адрес и направление"),
        ("target", 970, 470, "Ведомое устройство", "Условный адрес 0x2A\nОтвечает при выборе адреса"),
        ("other-target", 1620, 470, "Другое устройство", "Условный адрес 0x50"),
    ):
        p.node(key, x, 635, w, 200 if key != "other-target" else 165, title, body, "L0")
        for pin, wire_y, offset in (("SDA", 440, 95), ("SCL", 515, w-95)):
            xx = x+offset
            d.line(((xx, wire_y), (xx, 635)), clearance=True)
            el(p.edges, "circle", cx=xx, cy=wire_y, r=5, fill=d.ink)
            d.text(xx+14, 604, pin, 17)
    p.note(905, "Открытый сток: участник тянет линию в 0 или отпускает её. Подтяжка поднимает отпущенную линию в 1.")
    p.note(959, "Если хотя бы один участник удерживает 0, на линии остаётся 0. Свободная шина: SDA = 1, SCL = 1. Земля общая.")

    d.heading("bit", 1075, "Бит читается при высоком SCL")
    # Two bits. SDA changes only inside a low SCL interval.
    for x in (390, 850):
        el(p.under, "rect", x=x, y=1135, width=220, height=235, fill=d.fill)
    d.trace("SCL", 235, 1160, [(0, 0), (155, 0), (155, 1), (375, 1), (375, 0),
                             (615, 0), (615, 1), (835, 1), (835, 0), (970, 0)])
    d.trace("SDA", 235, 1300, [(0, 1), (500, 1), (500, 0), (970, 0)], color=d.data)
    d.text(500, 1410, "1", 25, bold=True, anchor="middle", color=d.data)
    d.text(960, 1410, "0", 25, bold=True, anchor="middle", color=d.data)
    d.text(1380, 1195, "При SCL = 1 данные на SDA стабильны.\nБит меняют при SCL = 0.", 23, width=740)
    d.link(1380, 1345, "START и STOP — отдельные условия", "i2c/protocol/transaction.svg#conditions", 20)

    d.heading("clock-stretching", 1550, "Clock stretching: устройство продлевает низкий SCL")
    el(p.under, "rect", x=525, y=1620, width=460, height=130, fill=d.fill)
    d.trace("SCL", 235, 1640, [(0, 1), (105, 1), (105, 0), (750, 0), (750, 1),
                             (915, 1), (915, 0), (1030, 0)])
    d.line(((525, 1612), (525, 1760)), width=1.5, dash="5 6")
    d.text(525, 1800, "Ведущий отпустил SCL", 19, anchor="middle")
    d.text(985, 1840, "Устройство отпустило SCL", 19, anchor="middle")
    d.text(1380, 1650, "Ведущий ждёт подъёма SCL.\nПосле этого такты продолжаются.\nПоддержка зависит от участников.", 23, width=740)
    p.note(1945, "Здесь показаны обычный двунаправленный I²C, один ведущий и 7-битные адреса. Временные масштабы условны.")
    d.footer(2040, "Далее: из чего состоит транзакция", "i2c/protocol/transaction.svg")


def transaction_sheet(d):
    p, el = d.p, d.el
    d.heading("format", 285, "От START до STOP", "Поля идут слева направо. Под ними указан участник, который задаёт SDA.")
    d.strip("frame", 385, [
        condition("S"), field("Адрес", 300, detail="7 бит", target="i2c/protocol/transaction.svg#address"),
        field("R/W", 126, detail="1 бит"), ack(),
        field("Данные", 265, "передатчик"), field("ACK", 130, "получатель", "1 бит", "ack"),
        omitted(86), field("Данные", 265, "передатчик"),
        field("ACK или NACK", 202, "получатель", "1 бит", "ack"), condition("P"),
    ])
    d.text(100, 585, "S и P — условия на линиях. Адрес с направлением занимает один байт; каждый байт сопровождается девятым тактом.", 21)

    d.heading("conditions", 700, "START и STOP формирует ведущий")
    d.text(235, 770, "S — START", 25, bold=True)
    d.trace("SCL", 235, 825, [(0, 1), (590, 1), (590, 0), (760, 0)])
    d.trace("SDA", 235, 945, [(0, 1), (340, 1), (340, 0), (760, 0)], color=d.data)
    d.line(((575, 800), (575, 1025)), width=1.5, dash="5 6")
    d.text(235, 1070, "SDA: 1 → 0 при SCL = 1", 22)
    d.text(1315, 770, "P — STOP", 25, bold=True)
    d.trace("SCL", 1315, 825, [(0, 0), (150, 0), (150, 1), (760, 1)])
    d.trace("SDA", 1315, 945, [(0, 0), (420, 0), (420, 1), (760, 1)], color=d.data)
    d.line(((1735, 800), (1735, 1025)), width=1.5, dash="5 6")
    d.text(1315, 1070, "SDA: 0 → 1 при SCL = 1", 22)

    d.heading("byte", 1200, "Восемь бит и подтверждение", "Пример: передатчик выдаёт байт 0x54, получатель отвечает ACK.")
    origin, pitch = 260, 193
    clock, data = [(0, 0)], []
    bits = [0, 1, 0, 1, 0, 1, 0, 0, 0]
    el(p.under, "rect", x=origin+8*pitch, y=1290, width=pitch, height=275, fill=d.fill)
    previous = bits[0]
    for i, bit in enumerate(bits):
        xx = i*pitch
        clock.extend([(xx+42, 0), (xx+42, 1), (xx+142, 1), (xx+142, 0)])
        if i == 0:
            data.append((0, bit))
        elif previous != bit:
            data.extend([(xx+16, previous), (xx+16, bit)])
        previous = bit
        d.text(origin+xx+92, 1318, f"D{7-i}" if i < 8 else "ACK", 22, bold=True, anchor="middle")
        d.text(origin+xx+92, 1580, str(bit), 21, anchor="middle", color=d.data)
    clock.append((9*pitch, 0))
    data.append((9*pitch, previous))
    d.trace("SCL", origin, 1350, clock)
    d.trace("SDA", origin, 1465, data, color=d.data)
    d.bracket(origin, origin+8*pitch-12, 1610, "Такты 1–8: SDA задаёт передатчик, старший бит идёт первым")
    d.bracket(origin+8*pitch+6, origin+9*pitch, 1610, "9: получатель")
    p.note(1720, "На девятом такте передатчик отпускает SDA. ACK: получатель тянет SDA в 0. NACK: SDA остаётся в 1.")
    p.note(1775, "Все девять тактов формирует ведущий. При чтении получателем байтов данных становится сам ведущий.")

    d.heading("address", 1900, "Первый байт: 7-битный адрес и направление")
    bits = [0, 1, 0, 1, 0, 1, 0, 0]
    for i, bit in enumerate(bits):
        xx = 115+138*i
        el(p.blocks, "rect", x=xx, y=1970, width=120, height=100, rx=4,
           fill="white", stroke=d.data, stroke_width=2)
        d.text(xx+60, 1950, f"A{6-i}" if i < 7 else "R/W", 20, anchor="middle")
        d.text(xx+60, 2036, str(bit), 30, bold=True, anchor="middle", color=d.data)
    d.bracket(115, 1063, 2102, "Адрес устройства 0x2A")
    d.text(1310, 1980, "W = 0: запись. Ведущий передаёт данные.\nR = 1: чтение. Ведущий получает данные.", 23, width=800)
    d.text(1310, 2100, "Для адреса 0x2A первый байт:\n0x54 при записи, 0x55 при чтении.", 23, width=800)
    p.note(2220, "Адрес устройства содержит 7 бит. Направление добавляется при передаче первого байта.")
    d.footer(2320, "Далее: запись и чтение целиком", "i2c/protocol/transfers.svg")


def transfers_sheet(d):
    p = d.p
    d.text(60, 285, "Читаем слева направо. Под полями указано, кто задаёт SDA. Ширина полей условна.", 21, color="#576c7b")
    d.heading("write", 395, "Запись: данные передаёт ведущий", "Выбранное устройство подтверждает адрес и каждый принятый байт данных.")
    d.strip("write-fields", 505, [condition("S"), address(), ack(), field("Данные 0", 260), ack(),
                                  omitted(100), field("Данные n", 260), ack(), condition("P")])
    p.note(730, "Для записи одного байта достаточно одной пары «данные, ACK». Затем ведущий формирует STOP.")
    p.note(785, "При записи нескольких байтов эта пара повторяется. Смысл и число байтов определяются устройством.")

    d.heading("read", 940, "Чтение: данные передаёт устройство", "Адрес подтверждает ведомый. Байты данных подтверждает ведущий.")
    d.strip("read-fields", 1050, [condition("S"), address(True), ack(), field("Данные 0", 250, "ведомый"),
                                   ack("ведущий"), omitted(100), field("Данные n", 250, "ведомый"),
                                   ack("ведущий", nack=True), condition("P")])
    p.note(1265, "ACK ведущего запрашивает следующий байт. После последнего нужного байта ведущий выдаёт NACK, затем STOP.")

    d.heading("single-byte-read", 1420, "Если нужен ровно один байт")
    d.strip("single-read-fields", 1500, [condition("S"), address(True), ack(), field("Данные", 250, "ведомый"),
                                        ack("ведущий", nack=True), condition("P")])
    d.text(1390, 1535, "NACK сразу после первого байта —\nнормальное завершение такого чтения.", 24, width=690)

    d.heading("address-nack", 1780, "Адрес не подтверждён", "SDA остаётся в 1 на девятом такте. В этом примере ведущий завершает попытку.")
    d.strip("nack-fields", 1890, [condition("S"), address(), ack("—", nack=True), condition("P")])
    d.text(1020, 1925, "Возможные причины: устройство отсутствует\nпо этому адресу или ещё не готово отвечать.", 23, width=1060)
    p.note(2110, "NACK возможен и после байта данных при записи. Ведущий выбирает дальнейшее действие.")
    d.footer(2210, "Далее: чтение с повторным START", "i2c/protocol/repeated-start.svg")


def repeated_start_sheet(d):
    p, el = d.p, d.el
    d.heading("combined-read", 290, "Пример: два байта начиная с регистра 0x10", "Условное устройство имеет адрес 0x2A. В этом примере оно поддерживает указатель регистра и его автоувеличение.")
    positions = d.strip("combined-fields", 425, [
        condition("S"), address(width=226, example=True), ack(),
        field("0x10", 130, detail="регистр"), ack(), condition("Sr"),
        address(True, width=226, example=True), ack(), field("D0", 150, "ведомый"),
        ack("ведущий"), field("D1", 150, "ведомый"), ack("ведущий", nack=True), condition("P"),
    ], x=90, gap=10)
    d.bracket(positions[0][0], positions[4][1], 607, "Записать указатель регистра")
    d.bracket(positions[6][0], positions[-1][1], 607, "Прочитать D0 и D1")
    d.text(60, 760, "0x10 передаётся как обычный байт данных. Его значение как номера регистра задаёт устройство.", 23)
    d.text(60, 823, "После Sr ведущий повторяет адрес 0x2A с направлением R. Устройство выдаёт D0, затем D1.", 23)
    d.text(60, 886, "Адрес шины выбирает устройство. Указатель регистра выбирает данные внутри него.", 23)

    d.heading("restart", 1040, "Sr — новый START без промежуточного STOP")
    el(p.under, "rect", x=620, y=1120, width=530, height=290, fill=d.fill)
    d.trace("SCL", 235, 1150, [(0, 0), (385, 0), (385, 1), (915, 1), (915, 0), (1090, 0)])
    d.trace("SDA", 235, 1300, [(0, 0), (145, 0), (145, 1), (680, 1), (680, 0), (1090, 0)], color=d.data)
    d.line(((915, 1100), (915, 1410)), width=1.5, dash="5 6")
    d.text(915, 1094, "Sr", 25, bold=True, anchor="middle")
    d.text(380, 1458, "SDA отпущена при SCL = 0", 20, anchor="middle")
    d.text(915, 1515, "SDA: 1 → 0 при SCL = 1", 20, anchor="middle")
    d.text(1470, 1195, "Шина остаётся занятой.\nПосле Sr начинается адресная фаза.\nАдрес и направление могут измениться.", 23, width=630)

    d.heading("stop-between", 1670, "Если между частями есть STOP — это две транзакции")
    d.strip("separate-write", 1780, [condition("S"), field("Запись …", 450, "", "адрес, подтверждения и данные"), condition("P")], x=100)
    d.strip("separate-read", 1780, [condition("S"), field("Чтение …", 450, "", "адрес, подтверждения и данные"), condition("P")], x=1420)
    d.text(1105, 1825, "Шина свободна", 24, bold=True, anchor="middle")
    d.line(((850, 1855), (1360, 1855)), width=1.8)
    p.note(2025, "Состояние указателя после STOP определяется устройством. Два отдельных обмена не всегда заменяют обмен с Sr.")
    p.note(2085, "Число байтов указателя, порядок байтов и автоувеличение нужно смотреть в описании выбранного устройства.")
    d.footer(2190, "Как ведомое ядро обрабатывает транзакции", "i2c/cores/slave/structure.svg")


def generate_i2c_protocol(page_class, element, palette, arrows):
    for name, title, subtitle, draw in (
        ("interface", "I²C: линии и участники", "L0. Общие линии SDA и SCL, открытый сток и тактирование.", interface_sheet),
        ("transaction", "I²C: устройство транзакции", "L0. START, 7-битный адрес, направление, байты данных и STOP.", transaction_sheet),
        ("transfers", "I²C: запись и чтение", "L0. Кто передаёт данные, кто подтверждает байт и как заканчивается обмен.", transfers_sheet),
        ("repeated-start", "I²C: повторный START", "L0. Связанные части обмена и пример чтения по указателю регистра.", repeated_start_sheet),
    ):
        page = page_class(f"i2c/protocol/{name}.svg", title, subtitle, [NXP_SPEC, TI_INTRO])
        draw(ProtocolDrawing(page, element, palette, arrows))
