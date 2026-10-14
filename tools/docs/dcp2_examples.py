"""Concrete DCP2 exchanges: field diagrams and their operation sequences.

Lengths and offsets are derived from the example fields. Large file and cache
arrays are shown as grouped data, not expanded into unreadable byte dumps.
These are documentation fixtures, not commands to execute against a board.
"""
from __future__ import annotations

from dataclasses import dataclass
import json
import xml.etree.ElementTree as ET

import generate_pl_subsystem as drawing
from generate_pl_subsystem import el, NS, PALETTE
from dcp2_introduction import HEADER, PAYLOAD, NETWORK, MUTED
from dcp2_reference import ProtocolFigure

SOURCES = (
    "src/apps/freertos/services/dcp2/dcp2_server.c",
    "src/apps/freertos/services/dcp2/dcp2_stream_sim.c",
    "src/protocols/dcp2/bvstk_dcp2_fs.c",
    "src/protocols/dcp2/bvstk_dcp2_cache.c",
    "src/services/fs/bvstk_fs_reader.c",
    "configs/mdio/lan8720/lan8720.json",
)
FIGURES = []


@dataclass(frozen=True)
class Field:
    name: str
    size: int
    display: str
    raw: bytes | None = None


def number(name, value, size):
    raw = value.to_bytes(size, "big")
    return Field(name, size, raw.hex(" ").upper(), raw)


def string(name, value):
    raw = value.encode("utf-8")
    visible = value.replace("\n", "\\n")
    return Field(name, len(raw), f'"{visible}"', raw)


def group(name, fields, display):
    raw = b"".join(f.raw for f in fields) if all(f.raw is not None for f in fields) else None
    return Field(name, sum(f.size for f in fields), display, raw)


def named(value, key):
    return [number(key + "_len", len(value.encode("utf-8")), 1), string(key, value)]


def path(value):
    return [number("path_len", len(value.encode("utf-8")), 2), string("path", value)]


@dataclass(frozen=True)
class Frame:
    label: str
    service: int
    operation: int
    sequence: int
    body: tuple = ()
    status: int | None = None

    def fields(self):
        body = list(self.body)
        if self.status is not None:
            body.insert(0, number("status", self.status, 2))
        length = 4 + sum(f.size for f in body)
        if not 4 <= length <= 4096:
            raise ValueError(f"Invalid DCP2 payload length: {self.label}: {length}")
        return [Field("magic", 4, "44 43 50 32", b"DCP2"),
                number("ver", 2, 2), number("dcp_len", length, 2),
                number("srv", self.service, 1), number("op", self.operation, 1),
                number("seq", self.sequence, 2)] + body


def request(label, srv, op, seq, body=()):
    return Frame(label, srv, op, seq, tuple(body))


def response(label, srv, op, seq, body=(), status=0):
    return Frame(label, srv, op | 0x80, seq, tuple(body), status)


def event(label, srv, op, body):
    return Frame(label, srv, op | 0x40, 0, tuple(body))


class ExampleFigure(ProtocolFigure):
    def __init__(self, name, description):
        super().__init__("examples/" + name, description, description, SOURCES)
        self.fixture_metadata = []
        FIGURES.append(name)

    def save(self):
        super().save()
        if not self.fixture_metadata:
            return
        output = drawing.OUT / self.name
        tree = ET.parse(output)
        metadata = tree.getroot().find(f"{{{NS}}}metadata")
        value = json.loads(metadata.text)
        value["examples"] = self.fixture_metadata
        metadata.text = json.dumps(value, ensure_ascii=False, indent=2)
        tree.write(output, encoding="utf-8", xml_declaration=True)


def field_width(field):
    return max(115, len(field.name) * 13 + 36,
               max(len(line) for line in field.display.splitlines()) * 14 + 36)


def fields_at(p, fields, y, prefix, header=False):
    x, row_height = 60, 100
    for i, f in enumerate(fields):
        width = field_width(f)
        height = 100 + (len(f.display.splitlines()) - 1) * 31
        if x + width > 1740:
            x = 60
            y += row_height + 24
            row_height = 100
        row_height = max(row_height, height)
        fill = HEADER if header and i < 3 else PAYLOAD
        g = p.cell(f"{prefix}-{i}", x, y, width, height, fill)
        p.text(g, x + width/2, y + 31, f.name, 22, bold=True, anchor="middle")
        # Direct text nodes keep hexadecimal columns in a monospace font.
        for line, value in enumerate(f.display.splitlines()):
            el(g, "text", value, x=x + width/2, y=y + 69 + line*31,
               font_family="DejaVu Sans Mono, monospace", font_size=23,
               fill="#233747", text_anchor="middle")
        x += width + 12
    return y + row_height


def wire(name, description, *frames):
    p = ExampleFigure(name, description)
    y = 70
    for i, frame in enumerate(frames):
        fields = frame.fields()
        total = sum(f.size for f in fields)
        p.text(p.notes, 60, y, frame.label, 29, bold=True)
        y = fields_at(p, fields, y + 35, f"frame-{i}", header=True) + 80
        offset = 0
        entries = []
        for field in fields:
            if field.raw is not None and len(field.raw) != field.size:
                raise ValueError(f"Wrong field size: {field.name}")
            entries.append({"name": field.name, "offset": offset, "size": field.size,
                            "hex": field.raw.hex() if field.raw is not None else None})
            offset += field.size
        p.fixture_metadata.append({"label": frame.label, "length": total, "fields": entries})
    p.key(60, y, HEADER, "Заголовок")
    p.key(370, y, PAYLOAD, "Полезная нагрузка")
    p.text(p.notes, 860, y, "HEX; строки в кавычках — UTF-8 без NUL", 21, color=MUTED)
    p.save()


def body_diagram(name, description, fields):
    p = ExampleFigure(name, description)
    y = fields_at(p, fields, 80, "body")
    p.text(p.notes, 60, y + 65,
           "HEX; строки в кавычках — UTF-8 без NUL.",
           21, color=MUTED)
    p.save()


class Sequence(ExampleFigure):
    def __init__(self, name, description, server="DCP2", extra=None):
        super().__init__(name, description)
        self.actor_x = (300, 1450) if extra is None else (240, 850, 1490)
        self.actor_layers = (None, "L4") if extra is None else (None, "L4", "L3")
        self.actor_labels = ("Клиент", server) if extra is None else ("Клиент", server, extra)
        self.y = 235
        for i, (x, label, layer) in enumerate(zip(self.actor_x, self.actor_labels, self.actor_layers)):
            fill, stroke = (PALETTE[layer][0], PALETTE[layer][1]) if layer else (NETWORK, MUTED)
            g = self.cell(f"actor-{i}", x-205, 60, 410, 105, fill, stroke)
            self.text(g, x, 110, label, 27, bold=True, anchor="middle")
            if layer:
                self.text(g, x, 145, layer, 21, color=stroke, anchor="middle")

    def send(self, a, b, label, kind="data", step=115):
        self.arrow(self.actor_x[a], self.y, self.actor_x[b], self.y, label, kind, size=24)
        self.y += step

    def state(self, actor, label, layer=None, step=145):
        x = self.actor_x[actor]
        fill = PALETTE[layer][0] if layer else NETWORK
        self.box(f"state-{int(self.y)}-{actor}", x-245, self.y-35, 490, 100, label, fill, 23)
        self.y += step

    def finish(self):
        for x in self.actor_x:
            el(self.under, "path", d=f"M{x},165 V{self.y-45}", fill="none",
               stroke="#bdc8d0", stroke_width=2, stroke_dasharray="5 8")
        y = self.y + 10
        x = 60
        for kind, label in (("data", "Передача данных"), ("call", "Вызов"), ("irq", "Событие")):
            if kind in self.protocol_arrows:
                self.arrow(x, y-7, x+65, y-7, kind=kind)
                self.text(self.notes, x+85, y, label, 21, color=MUTED)
                x += 415
        self.text(self.notes, 1740, y, "Время сверху вниз", 21, anchor="end", color=MUTED)
        y += 55
        self.key(60, y, PALETTE["L4"][0], "L4: протокольный интерфейс")
        if "L3" in self.actor_layers:
            self.key(695, y, PALETTE["L3"][0], "L3: сервис подсистемы")
        self.save()


def connection():
    wire("ping", "Проверка ответа DCP2",
         request("PING", 0, 0, 1), response("Ответ PING", 0, 0, 1))
    wire("unsupported", "Ответ на неизвестную команду MDIO",
         request("Неизвестная команда MDIO", 3, 0x3F, 2),
         response("ERR_UNSUPPORTED", 3, 0x3F, 2, status=2))
    wire("malformed", "Ответ на неполные параметры MDIO_WRITE",
         request("MDIO_WRITE: один байт val16 отсутствует", 3, 1, 3,
                 (number("phy", 1, 1), number("reg", 0, 1), number("val16: фрагмент", 0x12, 1))),
         response("ERR_MALFORMED", 3, 1, 3, status=1))
    p = Sequence("connection", "Операционная ошибка не завершает соединение")
    p.send(0, 1, "MDIO: opcode = 0x3F, seq = 2")
    p.send(1, 0, "ERR_UNSUPPORTED, seq = 2")
    p.send(0, 1, "PING, seq = 4")
    p.send(1, 0, "OK, seq = 4")
    p.state(0, "TCP-соединение сохранено")
    p.finish()


def memory():
    params = [number("flags", 1, 1), number("width", 32, 1),
              number("addr", 0x40000000, 4), number("count", 2, 2)]
    values = [number("data[0]", 0x11223344, 4), number("data[1]", 0xAABBCCDD, 4)]
    wire("memory-write", "Запись двух 32-битных элементов MEM",
         request("MEM_WRITE", 1, 1, 0x101, params + values),
         response("Ответ MEM_WRITE", 1, 1, 0x101))
    wire("memory-read", "Чтение двух 32-битных элементов MEM",
         request("MEM_READ", 1, 0, 0x102, params),
         response("Ответ MEM_READ", 1, 0, 0x102, values))
    p = ExampleFigure("memory-addresses", "Развитие адреса при MEM AUTOINC в FreeRTOS")
    for row, label, targets in ((150, "AUTOINC = 1", ("0x40000000", "0x40000004")),
                                (530, "AUTOINC = 0", ("0x40000000", "0x40000000"))):
        p.text(p.notes, 70, row-50, label, 29, bold=True)
        p.box(f"element-{row}-0", 70, row, 460, 110, "Элемент 0: 32 бита", PAYLOAD)
        p.box(f"element-{row}-1", 70, row+170, 460, 110, "Элемент 1: 32 бита", PAYLOAD)
        for i in range(2):
            p.box(f"address-{row}-{i}", 1200, row+i*170, 500, 110, targets[i], NETWORK)
            p.arrow(530, row+55+i*170, 1200, row+55+i*170, "Адрес доступа", "call")
    p.key(70, 920, PAYLOAD, "Элемент массива data")
    p.key(740, 920, NETWORK, "Физический адрес")
    p.save()


def i2c():
    target = [number("addr7", 0x2A, 1), number("reg", 0x10, 1)]
    wire("i2c-read", "Байтовое чтение I2C",
         request("I2C_READ_REG", 2, 0, 0x201, target),
         response("Ответ I2C_READ_REG", 2, 0, 0x201, [number("val", 0x42, 1)]))
    wire("i2c-deny", "Запрет записи и отклонённая команда I2C",
         request("I2C_POLICY_SET: BLACKLIST", 2, 2, 0x202,
                 [number("addr7", 0x2A, 1), number("policy", 1, 1)]),
         response("Политика установлена", 2, 2, 0x202),
         request("I2C_WRITE_REG", 2, 1, 0x203, target + [number("val", 0x3B, 1)]),
         response("ERR_DENIED", 2, 1, 0x203, status=3))
    wire("i2c-allow", "Разрешение записи и успешная команда I2C",
         request("I2C_POLICY_SET: WHITELIST", 2, 2, 0x204,
                 [number("addr7", 0x2A, 1), number("policy", 0, 1)]),
         response("Политика установлена", 2, 2, 0x204),
         request("I2C_WRITE_REG", 2, 1, 0x205, target + [number("val", 0x3B, 1)]),
         response("Запись завершена", 2, 1, 0x205))
    p = Sequence("i2c-policy", "Проверка политики при записи регистра I2C", extra="Сервис I²C")
    p.send(0, 1, "WRITE: 0x2A, R0x10, 0x3B")
    p.send(1, 2, "Записать регистр", "call")
    p.state(2, "BLACKLIST\nЗапись отклонена", "L3")
    p.send(2, 1, "DENIED")
    p.send(1, 0, "ERR_DENIED")
    p.send(0, 1, "POLICY_SET: WHITELIST")
    p.send(1, 2, "Разрешить запись", "call")
    p.send(2, 1, "OK")
    p.send(1, 0, "OK")
    p.send(0, 1, "Повтор WRITE, новый seq")
    p.send(1, 2, "Записать регистр", "call")
    p.state(2, "Запись через драйвер", "L3")
    p.send(2, 1, "OK")
    p.send(1, 0, "OK")
    p.finish()


def mdio():
    wire("mdio-read", "Чтение 16-битного регистра MDIO",
         request("MDIO_READ", 3, 0, 0x301, [number("phy", 1, 1), number("reg", 1, 1)]),
         response("Ответ MDIO_READ", 3, 0, 0x301, [number("val16", 0x24, 2)]))
    wire("mdio-write", "Запись 16-битного регистра MDIO",
         request("MDIO_WRITE", 3, 1, 0x302,
                 [number("phy", 1, 1), number("reg", 0, 1), number("val16", 0x1200, 2)]),
         response("Ответ MDIO_WRITE", 3, 1, 0x302))
    p = Sequence("mdio-read-path", "Выполнение чтения MDIO через сервис", extra="Сервис MDIO")
    p.send(0, 1, "READ: PHY 1, R1")
    p.send(1, 2, "Читать PHY 1, R1", "call")
    p.state(2, "Чтение через драйвер\nРезультат: 0x0024", "L3")
    p.send(2, 1, "OK, значение 0x0024")
    p.send(1, 0, "RESP, status = 0, 00 24")
    p.finish()


def notify():
    filter_fields = [number("class_mask", 2, 4), number("source_mask", 4, 4),
                     number("bus_mask", 2, 4), number("flags", 1, 1)]
    wire("notify-subscribe", "Подписка на завершённые записи MDIO от DCP",
         request("NOTIFY_SUBSCRIBE", 6, 0x10, 0x401, filter_fields),
         response("Подписка установлена", 6, 0x10, 0x401))
    commit = [number("time_us", 1250000, 8), number("ev_type", 2, 2), number("status", 0, 2),
              number("source", 2, 1), number("bus", 1, 1), number("op_kind", 1, 1),
              number("reserved", 0, 1), number("arg0: PHY", 1, 4),
              number("arg1: reg", 0, 4), number("arg2: value", 0x1200, 4)]
    wire("notify-event", "Уведомление об успешной записи MDIO",
         event("REG_COMMIT", 6, 0x10, commit))
    loss = [number("time_us", 1260000, 8), number("ev_type", 6, 2), number("status", 0, 2),
            number("source", 3, 1), number("bus", 1, 1), number("op_kind", 4, 1),
            number("reserved", 0, 1), number("arg0: lost", 3, 4),
            number("arg1", 0, 4), number("arg2", 0, 4)]
    wire("notify-loss", "Уведомление о потере трёх записей журнала MDIO",
         event("SOFTWARE_LOSS", 6, 0x10, loss))
    wire("notify-unsubscribe", "Отмена NOTIFY",
         request("NOTIFY_UNSUBSCRIBE", 6, 0x11, 0x402),
         response("Подписка отменена", 6, 0x11, 0x402))
    p = Sequence("notify-delivery", "Независимая доставка ответа MDIO и уведомления NOTIFY")
    p.send(0, 1, "NOTIFY_SUBSCRIBE: COMMIT, DCP, MDIO")
    p.send(1, 0, "Подписка установлена, seq = 0x0401")
    p.send(0, 1, "MDIO_WRITE: PHY 1, R0, 0x1200; seq = 0x0302")
    p.send(1, 0, "Ответ MDIO_WRITE, seq = 0x0302")
    p.send(1, 0, "NOTIFY: REG_COMMIT, seq = 0", "irq")
    p.state(0, "Ответ завершает запрос\nСобытие пополняет журнал")
    p.send(0, 1, "NOTIFY_UNSUBSCRIBE")
    p.send(1, 0, "Подписка отменена")
    p.finish()


def stream():
    wire("stream-subscribe", "Подписка на STREAM MDIO с меткой времени",
         request("PL_SUBSCRIBE_STREAM: MDIO", 3, 0x10, 0x501, [number("flags", 3, 1)]),
         response("Подписка установлена", 3, 0x10, 0x501))
    observation = [Field("magic", 4, '"BVT1"', b"BVT1"), number("sequence", 10, 8),
                   number("type", 2, 2), number("status", 0, 2), number("source", 2, 1),
                   number("bus", 1, 1), number("operation", 1, 1), number("width", 16, 1),
                   number("address", 1, 4), number("reg", 0, 4), number("value", 0x1200, 4)]
    wire("stream-observation", "STREAM переносит наблюдение MDIO в записи BVT1",
         event("STREAM: OBSERVATION", 3, 0x10,
               [number("time_us", 1250000, 8), number("ev_flags", 2, 1),
                number("lost_delta", 0, 4), number("data_len", 32, 2),
                group("data", observation, "Запись BVT1")]))
    body_diagram("stream-bvt1", "Поля записи наблюдения BVT1", observation)
    words = [number(f"word[{i}]", v, 4) for i, v in enumerate((1, 0, 0, 0xC0000001))]
    wire("stream-synthetic", "Первый набор слов генератора SPI COUNTER",
         request("PL_SUBSCRIBE_STREAM: SPI", 4, 0x10, 0x502, [number("flags", 7, 1)]),
         response("Подписка установлена", 4, 0x10, 0x502),
         event("STREAM: SYNTHETIC", 4, 0x10,
               [number("time_us", 2000000, 8), number("ev_flags", 4, 1),
                number("lost_delta", 0, 4), number("data_len", 16, 2)] + words))
    wire("stream-unsubscribe", "Отмена потоковой подписки MDIO",
         request("PL_UNSUBSCRIBE_STREAM: MDIO", 3, 0x11, 0x503),
         response("Подписка MDIO отменена", 3, 0x11, 0x503))


def fs_open(seq, open_id, kind, flags, block, filename):
    return request("FS_OPEN", 7, 2, seq,
                   [number("open_id", open_id, 4), number("kind", kind, 1),
                    number("flags", flags, 1), number("requested_block_size", block, 2)] + path(filename))


def fs_open_response(seq, handle, block, total):
    return response("Ответ FS_OPEN", 7, 2, seq,
                    [number("handle", handle, 4), number("block_size", block, 2), number("total_size", total, 8)])


def fs_read(seq, handle, block):
    return request("FS_READ", 7, 3, seq, [number("handle", handle, 4), number("block_no", block, 4)])


def fs_read_response(seq, handle, block, eof, data):
    return response("Ответ FS_READ", 7, 3, seq,
                    [number("handle", handle, 4), number("block_no", block, 4),
                     number("flags", int(eof), 1), number("reserved", 0, 1),
                     number("data_len", data.size, 2)] + ([data] if data.size else []))


def files():
    info = [number("fs_version", 1, 2), number("capabilities", 0x3F, 4),
            number("max_block_size", 3072, 2), number("max_path_bytes", 511, 2),
            number("max_depth", 16, 2), number("max_handles", 2, 2),
            number("idle_timeout_ms", 30000, 4), number("volume_count", 3, 2)]
    for name, state in (("sd", 0), ("flash", 1), ("sd-pl", 2)):
        info += [group("volume: " + name,
                       [number("state", state, 1)] + named(name, "name"),
                       f'{state:02X} {len(name):02X} "{name}"')]
    wire("fs-info", "Возможности FS и условное состояние томов",
         request("FS_INFO", 7, 0, 0x601), response("Ответ FS_INFO", 7, 0, 0x601, info))
    wire("fs-stat", "Сведения о шестибайтовом файле",
         request("FS_STAT", 7, 1, 0x602, path("flash:/demo.txt")),
         response("Ответ FS_STAT", 7, 1, 0x602,
                  [number("type", 1, 1), number("attributes", 8, 1), number("size", 6, 8)]))
    wire("fs-open", "Открытие файла с блоком четыре байта",
         fs_open(0x603, 1, 1, 0, 4, "flash:/demo.txt"), fs_open_response(0x603, 1, 4, 6))
    wire("fs-read", "Получение первого блока файла",
         fs_read(0x604, 1, 0), fs_read_response(0x604, 1, 0, False, string("data", "BVST")))
    wire("fs-eof", "Получение конца файла и закрытие потока",
         fs_read(0x606, 1, 1), fs_read_response(0x606, 1, 1, True, string("data", "K\n")),
         request("FS_CLOSE", 7, 4, 0x607, [number("handle", 1, 4)]),
         response("Поток закрыт", 7, 4, 0x607))
    p = Sequence("fs-file", "Повтор блока не добавляет данные в файл второй раз")
    p.send(0, 1, "OPEN: FILE, open_id = 1, block_size = 4")
    p.send(1, 0, "handle = 1, total_size = 6")
    p.send(0, 1, "READ: block_no = 0, seq = 0x0604")
    p.send(1, 0, 'data = "BVST", EOF = 0')
    p.send(0, 1, "Повтор READ: block_no = 0, seq = 0x0605")
    p.send(1, 0, 'Тот же блок: "BVST"')
    p.state(0, "Блок 0 уже получен\nПовтор не дописывается")
    p.send(0, 1, "READ: block_no = 1, seq = 0x0606")
    p.send(1, 0, 'data = "K\\n", EOF = 1')
    p.send(0, 1, "CLOSE: handle = 1")
    p.send(1, 0, "OK")
    p.finish()


def directories():
    wire("fs-tree-open", "Открытие рекурсивного перечисления каталога",
         fs_open(0x701, 2, 2, 1, 64, "flash:/demo/"),
         fs_open_response(0x701, 2, 64, 0xFFFFFFFFFFFFFFFF))
    entry = [number("entry_len", 19, 2), number("type", 1, 1), number("reserved", 0, 1),
             number("size", 6, 8), number("path_len", 5, 2), string("path", "a.txt")]
    wire("fs-tree-read", "Один файл в потоке TREE",
         fs_read(0x702, 2, 0),
         fs_read_response(0x702, 2, 0, True, group("data", entry, "Запись TREE")))
    body_diagram("fs-tree-entry", "Запись TREE для файла a.txt", entry)
    wire("fs-tar", "Открытие потока TAR для того же каталога",
         fs_open(0x704, 3, 3, 0, 3072, "flash:/demo/"),
         fs_open_response(0x704, 3, 3072, 0xFFFFFFFFFFFFFFFF))
    wire("fs-tar-read", "Чтение небольшого архива TAR одним блоком",
         fs_read(0x705, 3, 0),
         fs_read_response(0x705, 3, 0, True, Field("data", 2048, "Архив TAR")))
    p = ExampleFigure("fs-directory-formats", "TREE передаёт описание файла, TAR — описание и содержимое")
    p.box("file", 60, 105, 580, 155, 'flash:/demo/a.txt\n6 Б: "BVSTK\\n"', NETWORK, 27)
    p.box("tree", 1040, 105, 680, 155, "TREE: запись 19 Б\na.txt, FILE, размер 6 Б", PAYLOAD, 27)
    p.arrow(640, 182, 1040, 182, "Метаданные")
    p.text(p.notes, 60, 380, "TAR: 2048 Б", 29, bold=True)
    for key, x, width, label in (
        ("tar-header", 60, 440, "Заголовок a.txt\n512 Б"),
        ("tar-file", 520, 320, 'Содержимое\n6 Б'),
        ("tar-pad", 860, 390, "Выравнивание\n506 нулевых Б"),
        ("tar-end", 1270, 450, "Конец архива\n1024 нулевых Б"),
    ):
        p.box(key, x, 440, width, 140, label, PAYLOAD, 25)
    p.key(60, 680, NETWORK, "Объект файловой системы")
    p.key(780, 680, PAYLOAD, "Данные потока FS")
    p.text(p.notes, 60, 725, "Размеры блоков условные", 21, color=MUTED)
    p.save()


def cache_target(subsystem="mdio", device="lan8720"):
    return [number("version", 1, 1)] + named(subsystem, "subsystem") + named(device, "device")


def resource(name, value, updated=1000, width=16, valid=1, source=2):
    return named(name, "id") + [number("width_bits", width, 1), number("valid", valid, 1),
                               number("source", source, 1), number("value", value, 8),
                               number("updated_ms", updated, 8)]


def cache_page(sub, token, revision, total, first, entries, next_index, captured=1000):
    body = [number("version", 1, 1), number("kind", 0, 1), number("subscription_id", sub, 2),
            number("snapshot_id", token, 4), number("generation", 1, 4),
            number("revision", revision, 8), number("captured_ms", captured, 8),
            number("total", total, 2), number("first", first, 2),
            number("count", len(entries), 2), number("next", next_index, 2), number("presence", 1, 1)]
    if entries:
        body.append(group("resources", [f for record in entries for f in record],
                          "Массив ресурсов"))
    return body


def cache_event(kind, rev, time, presence, reason=0, records=()):
    body = [number("version", 1, 1), number("kind", kind, 1), number("subscription_id", 1, 2),
            number("generation", 1, 4), number("revision", rev, 8), number("time_ms", time, 8),
            number("presence", presence, 1), number("reason", reason, 1), number("count", len(records), 2)]
    if records:
        body.append(group("resources", [f for record in records for f in record], "Запись ресурса"))
    return body


def cache():
    # Resource IDs match configs/mdio/lan8720/lan8720.json. Only the first two
    # entries have illustrative device values; the remaining entries are invalid.
    entries = [resource("reg_00", 0x1200), resource("reg_01", 0x24)]
    entries += [resource(f"reg_{i:02x}", 0, 0, valid=0, source=0) for i in range(2, 32)]
    wire("cache-get", "Получение снимка кэша устройства",
         request("CACHE_GET: новый снимок", 8, 0x20, 0x801,
                 cache_target() + [number("snapshot_id", 0, 4), number("first", 0, 2)]))
    wire("cache-snapshot", "Снимок тридцати двух ресурсов с ревизией 10",
         response("Ответ CACHE_GET", 8, 0x20, 0x801, cache_page(0, 1, 10, 32, 0, entries, 0xFFFF)))
    body_diagram("cache-resource", "Одна запись ресурса reg_01 в снимке CACHE", entries[1])
    wire("cache-subscribe", "Установка подписки CACHE с начальным снимком",
         request("CACHE_SUBSCRIBE", 8, 0x21, 0x802, cache_target()),
         response("Снимок и subscription_id", 8, 0x21, 0x802, cache_page(1, 2, 10, 32, 0, entries, 0xFFFF)))
    wire("cache-resource-event", "Изменение значения ресурса CACHE",
         event("RESOURCE: revision = 11", 8, 0x21,
               cache_event(1, 11, 1200, 1, records=[resource("reg_01", 0x25, 1200)])))
    body_diagram("cache-resource-update", "Новая запись reg_01 в событии CACHE", resource("reg_01", 0x25, 1200))
    wire("cache-presence", "Изменение доступности устройства CACHE",
         event("PRESENCE: revision = 12", 8, 0x21, cache_event(2, 12, 1300, 2)))
    wire("cache-resync", "Подписка CACHE требует нового снимка",
         event("RESYNC_REQUIRED: потеря журнала", 8, 0x21, cache_event(3, 12, 1500, 0, reason=1)))
    wire("cache-unsubscribe", "Отмена подписки CACHE",
         request("CACHE_UNSUBSCRIBE", 8, 0x22, 0x804,
                 [number("version", 1, 1), number("subscription_id", 1, 2)]),
         response("Подписка отменена", 8, 0x22, 0x804))
    p = Sequence("cache-lifecycle", "Согласование локальной копии с кэшем BVSTK")
    p.send(0, 1, "SUBSCRIBE: mdio, lan8720")
    p.send(1, 0, "Снимок S = 2: generation = 1, revision = 10")
    p.state(0, "reg_00 = 0x1200\nreg_01 = 0x0024; ещё 30 записей")
    p.send(1, 0, "RESOURCE: revision = 11, reg_01 = 0x0025", "irq")
    p.send(1, 0, "PRESENCE: revision = 12, недоступно", "irq")
    p.send(1, 0, "RESYNC_REQUIRED: потеря журнала", "irq")
    p.state(0, "Копия требует обновления")
    p.send(0, 1, "Новый SUBSCRIBE")
    p.send(1, 0, "Новый снимок: S = 3, revision = R′")
    p.send(1, 0, "Следующее изменение: revision = R′ + 1", "irq")
    p.finish()

    # 25 bytes per resource, so the actual 4090-byte result limit forces pages:
    # floor((4090 - 37) / 25) = 162, followed by 38 records.
    many = []
    for i in range(200):
        fields = resource(f"reg{i:02X}", i, 5000, width=8)
        many.append(fields)
    wire("cache-pages", "Продолжение многостраничного снимка CACHE",
         response("Первая страница: ответ SUBSCRIBE", 8, 0x21, 0x811,
                  cache_page(1, 0x21, 256, 200, 0, many[:162], 162, 5000)),
         request("CACHE_GET: продолжение снимка 0x21", 8, 0x20, 0x812,
                 cache_target("i2c", "demo") + [number("snapshot_id", 0x21, 4), number("first", 162, 2)]),
         response("Последняя страница: ответ GET", 8, 0x20, 0x812,
                  cache_page(1, 0x21, 256, 200, 162, many[162:], 0xFFFF, 5000)))
    p = ExampleFigure("cache-pagination", "Две страницы одного снимка из двухсот ресурсов")
    p.box("snapshot", 60, 85, 1680, 120, "snapshot_id = 0x21     revision = 256     total = 200", NETWORK, 28)
    p.box("first-page", 60, 390, 980, 180, "Страница 1\nfirst = 0, count = 162, next = 162", PAYLOAD, 27)
    p.box("last-page", 1150, 390, 590, 180, "Страница 2\nfirst = 162, count = 38\nnext = 0xFFFF", PAYLOAD, 25)
    p.arrow(550, 205, 550, 390, "SUBSCRIBE")
    p.arrow(1445, 205, 1445, 390, "GET: first = 162")
    p.box("copy", 60, 755, 1680, 120, "Локальная копия: все 200 ресурсов, revision = 256", NETWORK, 28)
    p.arrow(550, 570, 550, 755)
    p.arrow(1445, 570, 1445, 755)
    p.box("changes", 60, 1070, 1680, 100, "События: revision = 257, 258, …", PAYLOAD, 28)
    p.arrow(900, 875, 900, 1070, "После последней страницы", "irq")
    p.key(60, 1260, NETWORK, "Состояние кэша")
    p.key(640, 1260, PAYLOAD, "Данные DCP2")
    p.arrow(1210, 1253, 1275, 1253, kind="irq")
    p.text(p.notes, 1295, 1260, "Изменения", 21, color=MUTED)
    p.save()


def generate():
    FIGURES.clear()
    for draw in (connection, memory, i2c, mdio, notify, stream, files, directories, cache):
        draw()
    return len(FIGURES)
