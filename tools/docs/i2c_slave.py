"""Figures for the implemented I2C slave, its bus paths and PS boundary."""
from i2c_core import CoreDrawing
from i2c_protocol import ack, address, condition, field, omitted

BASE = "i2c/cores/slave/"
RTL = "../hw_platform/fpga/ip_repo/i2c_slave/i2c/slave/rtl/"
COMMON = "../hw_platform/fpga/ip_repo/i2c_slave/common/"
DRIVER = "src/drivers/pl/i2c/bvstk_i2c_slave.c"
SERVICE = "src/services/i2c/bvstk_i2c_slave_service.c"


def sheet(name, fragment=""):
    return BASE+name+".svg"+("#"+fragment if fragment else "")


class SlaveDrawing(CoreDrawing):
    def actors(self, y=650):
        p = self.p
        p.frame("core", 445, y-85, 855, 290, "I2C_slave_0",
                target=sheet("structure", "core"))
        actors = [
            ("host", 65, 310, "Внешний ведущий", "SDA и SCL", "L0", "i2c/protocol/transfers.svg", None),
            ("engine", 475, 350, "i2c_slave_engine", "Биты, ACK и границы", "PL", sheet("structure", "engine"), "core"),
            ("data-ctrl", 940, 325, "data_ctrl", "События и слова", "PL", sheet("structure", "data-ctrl"), "core"),
            ("bram", 1370, 340, "BRAM I²C", "Запрос и ответ", "PL", sheet("buffers", "bram"), None),
            ("ps", 1810, 340, "Сервис и драйвер PS", "Доступ через L1", "L3", "ps-interaction/i2c-irq.svg#service", None),
        ]
        self.centers = {}
        for key, x, w, title, body, layer, target, parent in actors:
            p.node(key, x, y, w, 175, title, body, layer, parent=parent, target=target)
            self.centers[key] = x+w/2
        self.actor_labels = {
            "host": ("Ведущий", "L0"), "engine": ("i2c_slave_engine", "PL"),
            "data-ctrl": ("data_ctrl", "PL"), "bram": ("BRAM I²C", "PL"), "ps": ("PS", "L3"),
        }
        return y+330


def structure(d):
    p = d.p
    p.frame("core", 420, 310, 1280, 1630, "I2C_slave_0",
            subtitle="i2c_slave_wrapper → top_i2c_slave", target="@architecture_bvstk_ps_pl.svg#i2c-slave")
    p.node("regs", 500, 490, 470, 195, "regs",
           "Карта адресов\nCSR, REQ и IRQ", target=sheet("completion", "irq"), parent="core")
    p.node("axi-s", 1180, 490, 440, 195, "axi_lite_slave_native",
           "s_axi: регистры", parent="core")
    p.node("platform", 1760, 490, 340, 195, "Доступ PS", "MMIO и доставка IRQ", "L1",
           target="@architecture_bvstk_l1.svg#platform-access")
    p.node("ps", 1760, 850, 340, 215, "Сервис и драйвер PS",
           "Указатель регистра\nПодготовка ответа", "L3", target="ps-interaction/i2c-irq.svg#service")
    p.edge("ps", "platform", "call")
    p.edge("platform", "axi-s", "mmio", start="W", end="E")
    p.edge("axi-s", "regs", "mmio", start="W", end="E")
    p.edge("regs", "platform", "irq", "IRQ через GIC", start="N", end="N",
           a_offset=105, via=((840, 425), (1930, 425)), label_xy=(1570, 407))

    p.frame("engine", 475, 920, 570, 650, "i2c_slave_engine", parent="core",
            target=sheet("addressing", "engine"))
    p.node("inputs", 510, 1010, 500, 155, "Входные сигналы",
           "debouncer → sync\nSDA и SCL", parent="engine", target=sheet("addressing", "inputs"))
    p.node("fsm", 510, 1280, 500, 215, "Автомат I²C",
           "Адрес, данные и ACK\nWAIT_PACKET: удержание SCL",
           parent="engine", target=sheet("read", "wait-first"))
    p.edge("inputs", "fsm", "data")
    p.frame("data-ctrl", 1180, 920, 480, 665, "data_ctrl", parent="core", target=sheet("buffers", "data-ctrl"))
    p.node("rx-pkt", 1220, 1010, 400, 155, "Событие rx_pkt",
           "packet_view_t\nrx_full: событие готово", parent="data-ctrl", target=sheet("write", "capture"))
    p.node("word", 1220, 1310, 400, 185, "Буфер data4bytes",
           "Упаковка байтов\nОбмен с BRAM", parent="data-ctrl", target=sheet("buffers", "packing"))
    p.edge("rx-pkt", "word", "data")
    p.edge("fsm", "rx-pkt", "data", "Событие", start="E", end="W", a_offset=-65,
           via=((1110, 1322.5), (1110, 1087.5)), label_xy=(1115, 1210))
    p.edge("word", "fsm", "data", "Байт ответа", start="W", end="E", a_offset=50, b_offset=65,
           label_xy=(1115, 1500))
    p.node("host", 60, 1010, 300, 155, "Внешний ведущий", "Шина I²C", "L0",
           target="i2c/protocol/interface.svg#controller")
    p.edge("host", "inputs", "data", start="E", end="W")
    p.edge("fsm", "host", "data", "SDA и удержание SCL", start="W", end="S",
           via=((210, 1387.5),), label_xy=(230, 1320))
    p.edge("regs", "engine", "mmio", "Карта адресов", start="S", end="N", b_offset=100,
           via=((735, 790), (860, 790)), label_xy=(840, 767))
    p.edge("regs", "data-ctrl", "mmio", "rd_valid", start="S", end="N", a_offset=170, b_offset=100,
           via=((905, 850), (1520, 850)), label_xy=(1455, 827))
    p.edge("engine", "regs", "irq", "ERR_ACK", start="N", end="S", a_offset=-120, b_offset=-95,
           label_xy=(540, 800))
    p.edge("data-ctrl", "regs", "irq", "События data_ctrl", start="N", end="S", a_offset=-150, b_offset=185,
           via=((1270, 737), (920, 737)), label_xy=(1150, 716))
    p.node("axi-m", 1180, 1715, 480, 160, "axi_lite_master_native",
           "m_axi: обмен с BRAM", parent="core")
    p.edge("word", "axi-m", "data", start="S", end="N", bidirectional=True)
    p.node("bram", 1180, 2110, 480, 195, "BRAM I²C",
           "Запрос: +0x1000\nОтвет: +0x2000", target=sheet("buffers", "bram"))
    p.edge("axi-m", "bram", "data", "Через AXI и контроллер BRAM", start="S", end="N",
           label_xy=(1660, 1990), bidirectional=True)
    p.edge("ps", "bram", "data", "Данные через L1", start="S", end="E",
           via=((1930, 2207.5),), label_xy=(1800, 2184), bidirectional=True)
    p.note(2420, "На I²C ядро — ведомое; при обращении к BRAM — инициатор AXI.")
    p.note(2480, "Указатель регистра, политика и кэш — в сервисе L3.")
    d.finish(2580, "Далее: как ядро выбирает обращение на шине", sheet("addressing"))


def addressing(d):
    p = d.p
    d.strip("address-fields", 320, [condition("S"), field("Адрес", 270, detail="7 бит"),
                                    field("R/W", 125, detail="1 бит"), ack()], x=110)
    d.link(1110, 370, "Адресная фаза в протоколе", "i2c/protocol/transaction.svg#address")
    p.frame("engine", 420, 580, 1320, 1800, "i2c_slave_engine", target=sheet("structure", "engine"))
    p.frame("inputs", 465, 660, 1225, 280, "Обработка входов", parent="engine")
    p.node("debouncer", 540, 770, 400, 130, "debouncer", "Фильтрация SDA и SCL", parent="inputs")
    p.node("sync", 1150, 770, 440, 130, "sync", "Синхронизация с clk", parent="inputs")
    p.edge("debouncer", "sync", "data")
    p.node("pins", 60, 760, 300, 150, "SDA и SCL", "Сигналы шины", "L0", target="i2c/protocol/interface.svg#bus")
    p.edge("pins", "debouncer", "data")
    p.node("start", 870, 1080, 420, 165, "IDLE → ADDR",
           "START: SDA ↓ при SCL = 1", parent="engine")
    p.edge("sync", "start", "data", start="S", end="N", via=((1370, 995), (1080, 995)))
    p.node("address", 870, 1380, 420, 205, "ADDR",
           "7 бит адреса\nphy_addr_list[addr]", parent="engine")
    p.edge("start", "address", "data")
    p.node("regs", 1820, 1180, 320, 210, "regs",
           "128-битная карта\nADDR_LIST_0…3", target=sheet("structure", "regs"))
    p.edge("regs", "address", "mmio", "Карта выбранных адресов", start="W", end="N",
           b_offset=135, via=((1215, 1285),), label_xy=(1530, 1262))
    p.node("ignored", 1390, 1395, 300, 175, "IDLE",
           "Адрес не выбран\nSDA отпущена", parent="engine", target=sheet("completion", "outcomes"))
    p.edge("address", "ignored", "data", "0", start="E", end="W", label_xy=(1340, 1455))
    p.node("direction", 870, 1750, 420, 190, "OP_CODE",
           "Бит направления\nСобытие start", parent="engine")
    p.edge("address", "direction", "data", "Адрес выбран", label_xy=(1160, 1670))
    p.node("write", 465, 2110, 540, 200, "Запись: WRITE_ACK",
           "ACK адреса → WRITE_BYTE", parent="engine", target=sheet("write", "address-write"))
    p.node("read", 1160, 2110, 540, 200, "Чтение: WAIT_PACKET",
           "SCL = 0 до первого байта", parent="engine", target=sheet("read", "wait-first"))
    p.edge("direction", "write", "data", "W = 0", start="S", end="N", a_offset=-120,
           via=((960, 2020), (735, 2020)), label_xy=(785, 1997))
    p.edge("direction", "read", "data", "R = 1", start="S", end="N", a_offset=120,
           via=((1200, 2020), (1430, 2020)), label_xy=(1380, 1997))
    p.note(2470, "Чтение: ACK адреса только после tx_full.")
    p.note(2530, "Повторный START: RESTART → ADDR, новый выбор адреса.")
    d.finish(2630, "Продолжение: приём записи", sheet("write"))


def write(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(example=True), ack(), field("B0", 160), ack(),
                          omitted(), field("B5", 160), ack(), condition("P")])
    d.text(1480, 355, "6 байтов; ACK выдаёт ядро.", 22, width=600)
    start = d.actors()
    end = d.sequence(start, [
        ("address-write", "host", "engine", "data", "Адрес + W → ACK", "ADDR → OP_CODE → WRITE_ACK → WRITE_BYTE", sheet("addressing", "write")),
        ("start-event", "engine", "data-ctrl", "data", "Начало записи", "start = 1, rw = 0, addr = 0x2A; счётчик фрагмента = 0", None),
        ("capture", ("host", "engine"), ("engine", "data-ctrl"), "data", "Приём байта → ACK", "WRITE_BYTE → rx_pkt → data4bytes; младшие разряды первыми", sheet("buffers", "packing")),
        ("full-word", "data-ctrl", "bram", "data", "Запись полного слова", "WR_MEM_DATA: B3 B2 B1 B0", sheet("buffers", "wr-window")),
        ("tail-event", ("host", "engine"), ("engine", "data-ctrl"), "data", "STOP → событие stop", "В data4bytes остались B4 и B5", sheet("repeated-start")),
        ("tail", "data-ctrl", "bram", "data", "WR_MEM_DATA → WR_MEM_HEADER", "Хвост B4 и B5 → заголовок: 6 байтов, адрес 0x2A", sheet("buffers", "header")),
        ("publish", "data-ctrl", "ps", "irq", "DATA_VALID и FINAL_PACKET", "Заголовок записан → IRQ через L1", sheet("completion", "irq")),
        ("consume", "bram", "ps", "data", "Чтение фрагмента", "Сервис L3: разбор запроса и политика", "ps-interaction/i2c-irq.svg#service"),
    ])
    p.note(end+50, "ACK подтверждает приём в PL. Запись во внешнее устройство разрешает сервис PS.")
    p.note(end+112, "Полное окно → DATA_VALID → повторное использование области.")
    d.finish(end+205, "Формат слов, заголовка и повторное использование окна", sheet("buffers"))


def read(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(True, example=True),
                          field("Ожидание", 235, "ведомый", "SCL = 0", "condition"), ack(),
                          field("B0", 160, "ведомый"), ack("ведущий"), omitted(),
                          field("Bn", 160, "ведомый"), ack("ведущий", True), condition("P")])
    d.text(60, 525, "Успешное чтение: PS успевает подготовить ответ.", 21, color="#576c7b")
    start = d.actors(y=710)
    end = d.sequence(start, [
        ("wait-first", "host", "engine", "data", "Адрес + R → WAIT_PACKET", "SCL = 0; ACK адреса отложен до первого байта", sheet("addressing", "read")),
        ("read-start", "engine", "data-ctrl", "data", "Начало чтения → RD_REQ", "start = 1, rw = 1; ожидание rd_valid", None),
        ("request", "data-ctrl", "ps", "irq", "Запрос ответа: RD_REQUEST", "IRQ через regs и L1", "ps-interaction/i2c-irq.svg"),
        ("prepare", "ps", "bram", "data", "Подготовка окна ответа", "+0x2000: заголовок и байты от сервиса L3", sheet("buffers", "rd-window")),
        ("accept", "ps", "data-ctrl", "mmio", "Ответ готов: CSR.rd_valid", "Запись через L1 и regs", sheet("completion", "irq")),
        ("fetch", "bram", "data-ctrl", "data", "RD_MEM_HEADER → RD_MEM_DATA", "m_axi: длина ответа и первое слово", sheet("buffers", "rd-window")),
        ("first-byte", "data-ctrl", "engine", "data", "RD_SEND_BYTE → tx_data", "При tx_ready = 1; после передачи tx_full = 1", None),
        ("address-ack", "engine", "host", "data", "WAIT_PACKET → WRITE_ACK", "Первый байт готов: отпустить SCL, подтвердить адрес", None),
        ("send-byte", "engine", "host", "data", "READ_BYTE: выдача байта", "8 бит → отпустить SDA для ответа ведущего", None),
        ("host-ack", "host", "engine", "data", "READ_ACK: ответ ведущего", "ACK → WAIT_PACKET; NACK → DONE до STOP или Sr", sheet("completion", "outcomes")),
    ])
    d.heading("refill", end+65, "data_ctrl: подготовка следующего байта")
    bottom = d.table("refill-cases", end+145, [
        ("После передачи байта", 720, "PL"), ("Действие data_ctrl", 1320, "PL")], [
        ("В слове есть данные", "RD_SEND_BYTE: следующий байт при tx_ready"),
        ("Слово закончилось", "RD_MEM_DATA: следующее слово → RD_SEND_BYTE"),
        ("Окно выдано или достигнут предел", "RD_REQ: RD_REQUEST → новое окно от PS"),
    ])
    p.note(bottom+70, "Новое окно может запрашиваться до ACK последнего байта. STOP и Sr прекращают фазу.")
    p.note(bottom+132, "Лимит ожидания байта: STRETCH_TIMEOUT_US. Исход зависит от этапа чтения.")
    d.link(60, bottom+184, "Получение нового значения через master", "i2c/cores/master/repeated-start.svg#master-service")
    d.finish(bottom+230, "Завершение чтения и истечение времени ожидания", sheet("completion"))


def repeated_start(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(width=195, example=True), ack(),
                          field("0x10", 130, detail="указатель"), ack(), condition("Sr"),
                          address(True, width=195, example=True), ack(), field("D0", 130, "ведомый"),
                          ack("ведущий"), field("D1", 130, "ведомый"), ack("ведущий", True), condition("P")], x=80, gap=10)
    p.node("pointer-byte", 60, 670, 380, 200, "Внешний ведущий",
           "Адрес 0x2A + W\nБайт 0x10, затем Sr", "L0", target="i2c/protocol/repeated-start.svg#combined-read")
    p.node("pointer-event", 650, 670, 850, 200, "i2c_slave_engine",
           "Байт 0x10 → data_ctrl\nSr: событие restart, RESTART → ADDR",
           target=sheet("write", "tail-event"))
    p.edge("pointer-byte", "pointer-event", "data")

    p.frame("write-event", 100, 1040, 830, 690, "Запись завершается", target=sheet("write", "tail"))
    p.node("write-data", 140, 1140, 750, 220, "data_ctrl",
           "WR_MEM_DATA: байт 0x10\nWR_MEM_HEADER: 1 байт, адрес 0x2A",
           parent="write-event", target=sheet("buffers", "header"))
    p.node("pointer-publish", 140, 1510, 750, 160, "BRAM: запрос +0x1000",
           "Заголовок записан → фрагмент готов", parent="write-event", target=sheet("buffers", "wr-window"))
    p.edge("write-data", "pointer-publish", "data", "Данные, затем заголовок", label_xy=(725, 1443))

    p.frame("read-phase", 1180, 1040, 890, 690, "Чтение начинается", target=sheet("read", "wait-first"))
    p.node("read-address", 1220, 1140, 810, 220, "i2c_slave_engine",
           "0x2A + R → WAIT_PACKET\nSCL = 0 до первого байта",
           parent="read-phase", target=sheet("addressing", "read"))
    p.node("read-request", 1220, 1510, 810, 160, "data_ctrl: RD_REQ",
           "RD_REQUEST → ожидание rd_valid", parent="read-phase", target=sheet("read", "request"))
    p.edge("read-address", "read-request", "data", "Событие start, rw = 1", label_xy=(1860, 1443))

    p.edge("pointer-event", "write-event", "data", start="S", end="N",
           via=((1075, 950), (515, 950)))
    p.edge("pointer-event", "read-phase", "data", start="S", end="N",
           via=((1075, 950), (1625, 950)))
    d.el(p.notes, "circle", cx=1075, cy=950, r=5, fill=d.data)
    d.text(1075, 1000, "Параллельно", 21, bold=True, anchor="middle", color=d.data)

    p.node("pointer", 680, 2010, 900, 275, "bvstk_i2c_slave_service",
           "DATA_VALID: сохранить указатель 0x10\nRD_REQUEST: подготовить ответ", "L3",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_slave_service")
    p.edge("write-event", "pointer", "irq", "DATA_VALID и FINAL_PACKET", start="S", end="N", b_offset=-280,
           via=((515, 1870), (850, 1870)), label_xy=(485, 1837))
    p.edge("read-request", "pointer", "irq", "RD_REQUEST", start="S", end="N", b_offset=280,
           via=((1625, 1900), (1410, 1900)), label_xy=(1785, 1867))

    p.node("reply", 680, 2480, 900, 205, "BRAM: ответ +0x2000",
           "Заголовок и байты от PS\nПосле записи — rd_valid",
           target=sheet("buffers", "rd-window"))
    p.edge("pointer", "reply", "data", "Запись через L1", label_xy=(1320, 2405))
    p.edge("pointer", "read-request", "mmio", "rd_valid после записи окна", start="E", end="E",
           via=((2150, 2147.5), (2150, 1590)), label_xy=(1870, 2117))

    p.node("continue", 680, 2860, 900, 245, "Выдача ответа",
           "tx_full → ACK адреса\ni2c_slave_engine: D0, затем D1", target=sheet("read", "fetch"))
    p.edge("reply", "continue", "data")
    p.node("host-read", 60, 2860, 400, 245, "Внешний ведущий",
           "D0 → ACK\nD1 → NACK → STOP", "L0", target=sheet("completion", "outcomes"))
    p.edge("continue", "host-read", "data", "SDA", start="W", end="E")

    p.note(3250, "При совместном IRQ сервис обрабатывает DATA_VALID перед RD_REQUEST.")
    p.note(3312, "FINAL_PACKET: с DATA_VALID — конец записи; без него — конец чтения.")
    p.note(3374, "Текущий сервис сохраняет корректный указатель и после STOP.")
    d.link(60, 3430, "Получение нового значения через master", "i2c/cores/master/repeated-start.svg#slave-service")
    d.finish(3475, "Подробно: выдача первого байта и продолжение чтения", sheet("read"))


def buffers(d):
    p, el = d.p, d.el
    p.frame("data-ctrl", 65, 320, 450, 900, "data_ctrl", target=sheet("structure", "data-ctrl"))
    p.node("writer", 100, 515, 380, 195, "Приём записи",
           "WR_MEM_DATA\nWR_MEM_HEADER", parent="data-ctrl", target=sheet("write", "tail"))
    p.node("reader", 100, 925, 380, 220, "Подготовка выдачи",
           "RD_MEM_HEADER\nRD_MEM_DATA\nRD_SEND_BYTE", parent="data-ctrl", target=sheet("read", "fetch"))
    p.frame("bram", 660, 320, 890, 900, "BRAM I²C",
            subtitle="Смещения от 0x40000000", target="reference/memory-map.svg#i2c-bank")
    p.frame("wr-window", 700, 440, 810, 300, "Запрос: +0x1000", parent="bram", target=sheet("write", "publish"))
    p.node("wr-header", 735, 520, 270, 155, "Заголовок", "Адрес и длина", parent="wr-window")
    p.node("wr-data", 1100, 520, 370, 155, "Данные", "Байты внешней записи", parent="wr-window")
    p.frame("rd-window", 700, 835, 810, 320, "Ответ: +0x2000", parent="bram", target=sheet("read", "prepare"))
    p.node("rd-header", 735, 925, 270, 155, "Заголовок", "Длина окна", parent="rd-window")
    p.node("rd-data", 1100, 925, 370, 155, "Данные", "Подготовленные байты", parent="rd-window")
    p.frame("ps", 1740, 320, 420, 900, "Драйвер PS", "L3", target="ps-interaction/i2c-irq.svg#service")
    p.node("consume", 1775, 515, 350, 195, "Прочитать запрос",
           "После DATA_VALID\nДоступ через L1", "L3", parent="ps")
    p.node("publish", 1775, 925, 350, 220, "Записать ответ",
           "Через L1\nПосле записи — rd_valid", "L3", parent="ps")
    p.edge("writer", "wr-window", "data", "Запись", start="E", end="W", b_offset=22.5, label_xy=(605, 584))
    p.edge("wr-window", "consume", "data", "Чтение", start="E", end="W", a_offset=22.5, label_xy=(1640, 584))
    p.edge("publish", "rd-window", "data", "Запись", start="W", end="E", b_offset=40, label_xy=(1640, 1005))
    p.edge("rd-window", "reader", "data", "Чтение", start="W", end="E", a_offset=40, label_xy=(595, 1005))

    d.heading("header", 1360, "Первое слово окна — header_t")
    for x, w, title, detail in ((130, 1380, "num_bytes", "31…7: число байтов фрагмента"),
                               (1535, 530, "phy_addr", "6…0: адрес I²C")):
        el(p.blocks, "rect", x=x, y=1430, width=w, height=115, rx=6, fill="white", stroke=d.data, stroke_width=2)
        d.text(x+w/2, 1472, title, 25, bold=True, anchor="middle", color=d.data)
        d.text(x+w/2, 1515, detail, 20, anchor="middle")
    p.note(1610, "num_bytes задаёт длину окна. Направление и номер регистра в header_t не входят.")

    d.heading("packing", 1730, "Упаковка: младший байт первым")
    for i, label in enumerate(("B3", "B2", "B1", "B0")):
        xx = 130+480*i
        el(p.blocks, "rect", x=xx, y=1810, width=465, height=105, rx=5, fill="white", stroke=d.data, stroke_width=2)
        d.text(xx+232.5, 1850, label, 27, bold=True, anchor="middle", color=d.data)
        d.text(xx+232.5, 1891, f"Биты {31-8*i}…{24-8*i}", 20, anchor="middle")
    d.text(60, 1990, "Пример: 10 7B 20 30 40 50, шестнадцатеричные байты.", 22)
    bottom = d.table("example", 2080, [("Смещение", 520, "PL"), ("Слово", 520, "PL"), ("Назначение", 1000, "PL")], [
        ("+0x00", "0x0000032A", "Адрес 0x2A, длина 6"),
        ("+0x04", "0x30207B10", "B0…B3; B0 в битах 7…0"),
        ("+0x08", "0x00005040", "B4 и B5; верхние байты не учитываются"),
    ])
    d.heading("window-reuse", bottom+110, "Окно используется повторно")
    end = d.table("window-rules", bottom+200, [("Направление", 440, "PL"), ("Порядок работы", 1600, "PL")], [
        ("Запись ведущего", "Данные → заголовок → DATA_VALID → повторное использование окна.\nSTOP или Sr добавляет FINAL_PACKET."),
        ("Чтение ведущего", "Ответ PS → rd_valid → выдача окна в engine → RD_REQUEST.\nСчётчик — по выдаче в engine, до ACK на шине."),
    ])
    p.note(end+70, "MEM_DEPTH = 0x1000: заголовок 4 байта, данные до 4092 байтов. Окно сервиса — до 64 байтов.")
    p.note(end+130, "Новый фрагмент записи при неснятом IRQ → MEM_OVERFLOW.")
    d.finish(end+225, "События, подтверждение IRQ и завершение обмена", sheet("completion"))


def completion(d):
    p = d.p
    d.heading("outcomes", 295, "Завершение фаз и ошибки")
    bottom = d.table("outcome-table", 400, [
        ("Событие", 345, "L0"), ("Действие ядра PL", 655, "PL"),
        ("Признаки IRQ", 425, "PL"), ("Обработка PS", 615, "L3")], [
        ("Адрес не выбран", "ADDR → IDLE\nSDA отпущена", "Нет нового события", "Запрос не поступает"),
        ("STOP или Sr после записи", "Хвост → заголовок\nSTOP → IDLE; Sr → ADDR", "DATA_VALID\nFINAL_PACKET", "Прочитать фрагмент\nЗавершить запись"),
        ("NACK ведущего при чтении", "READ_ACK → DONE\nОжидание STOP или Sr", "Без ERR_ACK\nБез FINAL_PACKET", "Ждать границу фазы"),
        ("STOP или Sr при чтении", "data_ctrl → WAIT_EVENT", "FINAL_PACKET без DATA_VALID\nВозможен ERR_ACK*", "Снять read_armed"),
        ("Тайм-аут первого байта", "WAIT_PACKET → ERR_ACK\nNACK адреса; ждать STOP или Sr", "ERR_ACK", "Снять read_armed\nПоздний ответ не возобновляет чтение"),
        ("Тайм-аут следующего байта", "READ_BYTE: выдать 0xFF\nЖдать ACK ведущего", "ERR_ACK", "Снять read_armed\nШина ещё может быть занята"),
        ("Новый фрагмент при неснятом IRQ", "Новый заголовок записан\nПредыдущие данные могли потеряться", "MEM_OVERFLOW\nDATA_VALID", "Ошибка; отменить ответ"),
        ("Сброс ядра", "Сброс состояний и карты адресов\nBRAM сохраняется", "IRQ снят", "Восстановить карту адресов"),
    ])
    d.heading("irq", bottom+115, "IRQ: фиксация и подтверждение")
    y = bottom+225
    p.node("events", 90, y, 520, 205, "События ядра",
           "i2c_slave_engine\ndata_ctrl", target=sheet("structure", "core"))
    p.node("regs", 855, y, 500, 205, "regs: IRQ",
           "Флаги до подтверждения PS", target=sheet("structure", "regs"))
    p.node("ps", 1630, y, 510, 205, "Драйвер PS",
           "Прочитать IRQ и REQ\nПодтвердить через L1", "L3", target="ps-interaction/i2c-irq.svg")
    p.edge("events", "regs", "irq")
    p.edge("regs", "ps", "irq", "IRQ через L1")
    p.edge("ps", "regs", "mmio", "Запись бита 0 в IRQ", start="S", end="S",
           via=((1885, y+330), (1105, y+330)), label_xy=(1510, y+306))
    p.note(y+445, "Сброс IRQ снимает флаги; новое событие имеет приоритет.")
    p.note(y+505, "Ответ готов: CSR.rd_valid. Лимит ожидания байта: STRETCH_TIMEOUT_US.")
    p.note(y+565, "* ERR_ACK: выход из WAIT_PACKET без tx_full, в том числе по STOP или Sr.")
    d.finish(y+665, "Вернуться к составу ведомого ядра", sheet("structure"))


def generate_i2c_slave(page_class, element, palette, arrows):
    sources = [RTL+name for name in ("i2c_slave_wrapper.sv", "top_i2c_slave.sv", "i2c_slave_engine.sv", "data_ctrl.sv", "regs.sv", "include/i2c_slave_pkg.sv")]
    sources += [COMMON+name for name in ("debouncer.sv", "sync.sv", "axi_lite/axi_lite_master_native.sv", "axi_lite/axi_lite_slave_native.sv")]
    sources += ["../hw_platform/fpga/Burevestnik_21.tcl", DRIVER, SERVICE,
                "src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c",
                "src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c"]
    for name, title, subtitle, draw in (
        ("structure", "I²C slave: состав ядра", "Шина I²C, обработка байтов и обмен с PS.", structure),
        ("addressing", "I²C slave: приём адреса", "Входы → START → адрес → направление.", addressing),
        ("write", "I²C slave: приём записи", "От байтов шины до запроса в PS. Шаги сверху вниз.", write),
        ("read", "I²C slave: выдача ответа", "От запроса к PS до байтов на шине.", read),
        ("repeated-start", "I²C slave: запись указателя и чтение", "Sr связывает фазы; указатель хранит сервис L3.", repeated_start),
        ("buffers", "I²C slave: окна BRAM", "Запрос, ответ и формат данных.", buffers),
        ("completion", "I²C slave: завершение и ошибки", "Событие на шине → признаки PL → действие PS.", completion),
    ):
        p = page_class(sheet(name), title, subtitle, sources)
        draw(SlaveDrawing(p, element, palette, arrows))
