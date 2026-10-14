"""Service body maps for the DCP2 reference, without offset or size labels."""
from dcp2_reference import ProtocolFigure
from dcp2_introduction import HEADER, PAYLOAD, NETWORK, MUTED
from generate_pl_subsystem import el


class FieldMap(ProtocolFigure):
    def __init__(self, name, description, sources=()):
        super().__init__(name, description, description, sources)
        self.serial = 0
        self.optional_fields = False
        self.optional_label = "Поле при WITH_TIMESTAMP в подписке"

    def strip(self, y, service, operation, sequence="q", label=None):
        if label:
            self.text(self.notes, 60, y-25, label, 28, bold=True)
        for key, x, width, value, fill in (
            ("header", 60, 420, "Заголовок DCP2\nmagic, ver, dcp_len", HEADER),
            ("service", 480, 230, "srv\n" + service, PAYLOAD),
            ("operation", 710, 310, "op\n" + operation, PAYLOAD),
            ("sequence", 1020, 250, "seq\n" + sequence, PAYLOAD),
            ("body", 1270, 470, "body", PAYLOAD),
        ):
            self.box(f"strip-{self.serial}-{key}", x, y, width, 105, value, fill, 25)
        self.serial += 1

    def panel(self, key, y, height, label, x=60, width=1680):
        g = self.cell(key, x, y, width, height, "white")
        self.text(g, x+28, y+40, label, 26, bold=True)
        return g

    def fields(self, labels, x, y, width=1580, height=84, optional=()):
        """Natural field widths, with line breaks preserving wire order."""
        start, row_y = x, y
        boxes = {}
        for i, label in enumerate(labels):
            field_width = max(130, max(len(s) for s in label.split("\n"))*14 + 38)
            if x + field_width > start + width:
                x = start
                row_y += height + 22
            key = f"field-{self.serial}-{i}"
            g = self.box(key, x, row_y, field_width, height, label, PAYLOAD, 24)
            if label in optional:
                rect = next(child for child in g if child.tag.endswith("rect"))
                rect.set("stroke-dasharray", "8 5")
                self.optional_fields = True
            boxes[label] = (x, row_y, field_width, height)
            x += field_width + 12
        self.serial += 1
        return boxes

    def reveal(self, top, bottom, x=1505):
        # Keep a field expansion visible across its enclosing panel borders.
        el(self.notes, "path", d=f"M{x},{top+2} V{bottom-4}",
           fill="none", stroke="white", stroke_width=8)
        self.arrow(x, top, x, bottom, kind="call")
        connector = self.edges[-1]
        self.edges.remove(connector)
        self.notes.append(connector)

    def finish(self, y, fields=True, reveal=True):
        if fields:
            self.key(60, y, HEADER, "Заголовок DCP2")
            self.key(460, y, PAYLOAD, "Поля полезной нагрузки")
        if reveal:
            self.arrow(1090, y-7, 1155, y-7, kind="call")
            self.text(self.notes, 1175, y, "Раскрытие поля", 21, color=MUTED)
        if self.optional_fields:
            el(self.notes, "rect", x=60, y=y+35, width=38, height=28,
               fill=PAYLOAD, stroke=MUTED, stroke_width=2, stroke_dasharray="8 5")
            self.text(self.notes, 115, y+57, self.optional_label, 21, color=MUTED)
        self.save()


def memory_fields():
    p = FieldMap("memory-fields", "Тело MEM_READ и MEM_WRITE в запросе и ответе")
    p.strip(90, "0x01 (MEM)", "флаги, opcode")
    p.panel("body", 285, 690, "body: запрос и успешный ответ")
    p.reveal(195, 285)
    for y, label, fields in (
        (390, "READ: opcode = 0x00", ("flags", "width", "addr", "count")),
        (535, "Ответ READ", ("status", "data")),
        (715, "WRITE: opcode = 0x01", ("flags", "width", "addr", "count", "data")),
        (860, "Ответ WRITE", ("status",)),
    ):
        p.text(p.notes, 105, y+49, label, 24, bold=True)
        p.fields(fields, 540, y, 1150)
    p.finish(1060)


def register_fields():
    p = FieldMap("register-fields", "Расположение полей регистровых команд I2C и MDIO")
    p.strip(90, "I²C или MDIO", "флаги, opcode")
    p.panel("bodies", 285, 945, "body: запрос и успешный ответ")
    p.reveal(195, 285)
    p.text(p.notes, 530, 405, "Запрос", 25, bold=True)
    p.text(p.notes, 1320, 405, "Ответ", 25, bold=True)
    p.text(p.notes, 105, 480, "I²C: srv = 0x02", 27, bold=True)
    for y, name, opcode, params, result in (
        (530, "I2C_READ_REG", "0x00", ("addr7", "reg"), ("status", "val")),
        (685, "I2C_WRITE_REG", "0x01", ("addr7", "reg", "val"), ("status",)),
        (925, "MDIO_READ", "0x00", ("phy", "reg"), ("status", "val16")),
        (1080, "MDIO_WRITE", "0x01", ("phy", "reg", "val16"), ("status",)),
    ):
        p.text(p.notes, 105, y+31, name, 23, bold=True)
        p.text(p.notes, 105, y+68, "opcode = " + opcode, 22, color=MUTED)
        p.fields(params, 530, y, 640)
        p.fields(result, 1290, y, 420)
    p.text(p.notes, 105, 875, "MDIO: srv = 0x03", 27, bold=True)
    p.finish(1320)


def notify_subscribe_fields():
    p = FieldMap("notify-subscribe-fields", "Фильтр NOTIFY внутри тела запроса подписки")
    p.strip(90, "0x06", "0x10", label="NOTIFY_SUBSCRIBE")
    p.panel("body", 295, 230, "body запроса")
    p.fields(("class_mask", "source_mask", "bus_mask", "flags"), 110, 390)
    p.reveal(195, 295)
    p.finish(615)


def notify_event_fields():
    p = FieldMap("notify-event-fields", "Поля события NOTIFY и условная метка времени")
    p.strip(90, "0x06", "0x50 (EVENT)", "0")
    p.panel("body", 295, 230, "body события")
    p.fields(("time_us", "ev_type", "status", "source", "bus", "op_kind", "reserved",
              "arg0", "arg1", "arg2"), 110, 385, optional=("time_us",))
    p.reveal(195, 295)
    p.finish(615)


def notify_filter():
    p = FieldMap("notify-filter", "Отбор событий после курсора подписки NOTIFY")
    p.cell("selected-journal", 60, 90, 1680, 230, "white")
    p.text(p.notes, 100, 140, "Журнал шины, выбранной bus_mask", 28, bold=True)
    p.box("old-records", 100, 185, 580, 90, "Записи до курсора", NETWORK, 25)
    p.box("new-records", 810, 185, 860, 90, "Новые записи", PAYLOAD, 27)
    el(p.notes, "path", d="M745,170 V285", stroke=MUTED, stroke_width=3)
    p.text(p.notes, 745, 365, "Курсор", 23, color=MUTED, anchor="middle")
    p.arrow(1240, 275, 1240, 470, kind="data")
    p.cell("filter", 720, 470, 1020, 205, "white")
    p.text(p.notes, 760, 515, "Совпали обе маски", 27, bold=True)
    p.box("class", 760, 555, 390, 85, "class_mask", PAYLOAD)
    p.text(p.notes, 1225, 610, "И", 29, bold=True, anchor="middle")
    p.box("source", 1300, 555, 400, 85, "source_mask", PAYLOAD)
    p.box("emit", 1140, 835, 600, 105, "NOTIFY EVENT", PAYLOAD, 28)
    p.arrow(1440, 675, 1440, 835, "Да", "irq", (1500, 760))
    p.box("skip", 650, 835, 400, 105, "Пропуск записи", NETWORK, 26)
    p.arrow(850, 675, 850, 835, "Нет", "call", (910, 760))
    p.key(60, 1050, NETWORK, "Состояние журнала")
    p.key(620, 1050, PAYLOAD, "Данные подписки")
    for x, kind, label in ((60, "data", "Чтение записи"),
                           (620, "call", "Результат отбора"),
                           (1210, "irq", "Событие")):
        p.arrow(x, 1113, x+65, 1113, kind=kind)
        p.text(p.notes, x+85, 1120, label, 21, color=MUTED)
    p.save()


def stream_event_fields():
    p = FieldMap("stream-event-fields", "Вложенные поля события STREAM и записи BVT1")
    p.strip(90, "сервис шины", "0x50 (EVENT)", "0")
    p.panel("body", 295, 205, "body события")
    p.reveal(195, 295)
    fields = p.fields(("time_us", "ev_flags", "lost_delta", "data_len"), 110, 385,
                      width=1100, optional=("time_us",))
    data_x = fields["data_len"][0] + fields["data_len"][2] + 12
    data_width = 1690 - data_x
    p.box("data", data_x, 385, data_width, 84, "data", PAYLOAD, 27)
    p.panel("data-content", 685, 620, "data: формат определяется ev_flags")
    p.reveal(469, 685, data_x + data_width/2)
    p.text(p.notes, 110, 805, "OBSERVATION", 26, bold=True)
    p.fields(("magic = BVT1", "sequence", "type", "status", "source", "bus"), 110, 845)
    p.fields(("operation", "width", "address", "reg", "value"), 110, 965)
    p.text(p.notes, 110, 1170, "SYNTHETIC", 26, bold=True)
    p.box("synthetic-data", 110, 1200, 1580, 75, "Данные тестового генератора", PAYLOAD, 26)
    p.finish(1390)


def fs_open_fields():
    p = FieldMap("fs-open-fields", "Тело открытия потока FS и выданный дескриптор",
                 ("src/protocols/dcp2/bvstk_dcp2_fs.c",))
    p.strip(100, "0x07", "0x02", label="FS_OPEN")
    p.panel("request", 305, 230, "body запроса")
    p.fields(("open_id", "kind", "flags", "requested_block_size", "path_len", "path"), 110, 395)
    p.reveal(205, 305)
    p.strip(675, "0x07", "0x82 (RESP)", label="Ответ FS_OPEN")
    p.panel("response", 880, 210, "body успешного ответа")
    p.fields(("status", "handle", "block_size", "total_size"), 110, 970)
    p.reveal(780, 880)
    p.finish(1180)


def fs_read_fields():
    p = FieldMap("fs-read-fields", "Метаданные блока FS_READ и его содержимое",
                 ("src/protocols/dcp2/bvstk_dcp2_fs.c",))
    p.strip(100, "0x07", "0x03", label="FS_READ")
    p.panel("request", 305, 200, "body запроса")
    p.fields(("handle", "block_no"), 110, 390)
    p.reveal(205, 305)
    p.strip(645, "0x07", "0x83 (RESP)", label="Ответ FS_READ")
    p.panel("response", 850, 360, "body успешного ответа")
    p.fields(("status", "handle", "block_no", "flags", "reserved", "data_len"), 110, 940)
    p.box("data", 110, 1070, 1580, 100, "data", PAYLOAD, 28)
    p.reveal(750, 850)
    p.finish(1300)


def cache_request_fields():
    p = FieldMap("cache-request-fields", "Адресация запросов CACHE",
                 ("src/protocols/dcp2/bvstk_dcp2_cache.c",))
    p.strip(90, "0x08", "opcode")
    p.panel("request", 295, 670, "body запроса")
    p.reveal(195, 295)
    for y, name, opcode, fields in (
        (400, "GET", "0x20", ("cache_version", "subsystem", "device", "snapshot_id", "first")),
        (610, "SUBSCRIBE", "0x21", ("cache_version", "subsystem", "device")),
        (820, "UNSUBSCRIBE", "0x22", ("cache_version", "subscription_id")),
    ):
        p.text(p.notes, 100, y+28, name, 25, bold=True)
        p.text(p.notes, 100, y+67, "opcode = " + opcode, 23, color=MUTED)
        p.fields(fields, 440, y, 1250)
    p.finish(1055)


def cache_page_fields():
    p = FieldMap("cache-page-fields", "Страница CACHE внутри успешного ответа DCP2",
                 ("src/protocols/dcp2/bvstk_dcp2_cache.c",))
    p.strip(90, "0x08", "RESP | opcode")
    p.panel("body", 295, 560, "body ответа GET или SUBSCRIBE")
    p.reveal(195, 295)
    p.box("status", 110, 385, 210, 84, "status", PAYLOAD, 27)
    p.cell("page", 345, 370, 1350, 435, "white")
    p.text(p.notes, 380, 415, "Страница снимка", 26, bold=True)
    p.fields(("cache_version", "kind", "subscription_id", "snapshot_id", "generation",
              "revision", "captured_ms", "total", "first", "count", "next", "presence"),
             385, 455, 1260)
    p.box("resources", 385, 695, 1260, 75, "resources[0], resources[1], …", PAYLOAD, 26)
    p.panel("entry", 1030, 210, "Одна запись resources")
    p.reveal(770, 1030, 1015)
    p.fields(("resource_id", "width_bits", "valid", "source", "value", "updated_ms"), 110, 1120)
    p.finish(1330)


def cache_event_fields():
    p = FieldMap("cache-event-fields", "Поля события CACHE и условная запись ресурса",
                 ("src/protocols/dcp2/bvstk_dcp2_cache.c",))
    p.strip(90, "0x08", "0x61 (EVENT)", "0")
    p.panel("event", 295, 340, "body события")
    p.reveal(195, 295)
    p.fields(("cache_version", "kind", "subscription_id", "generation", "revision", "time_ms",
              "presence", "reason", "count", "resources"), 110, 385, optional=("resources",))
    p.optional_label = "Запись resources при kind = 1"
    p.finish(725)


def generate():
    figures = (memory_fields, register_fields, notify_subscribe_fields,
               notify_event_fields, notify_filter, stream_event_fields,
               fs_open_fields, fs_read_fields, cache_request_fields,
               cache_page_fields, cache_event_fields)
    for draw in figures:
        draw()
    return len(figures)
