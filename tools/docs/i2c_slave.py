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


def structure(d):
    p = d.p
    d.canvas(2200)
    p.frame("core", 420, 310, 1280, 1630, "I2C_slave_0",
            subtitle="i2c_slave_wrapper → top_i2c_slave", target="@architecture_bvstk_ps_pl.svg#i2c-slave")
    p.node("regs", 500, 490, 470, 195, "regs",
           "Карта адресов\nCSR, REQ и IRQ", target=sheet("completion", "regs"), parent="core")
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
    p.frame("data-ctrl", 1180, 920, 480, 665, "data_ctrl", parent="core", target=sheet("buffers", "writer"))
    p.node("rx-pkt", 1220, 1010, 400, 155, "Событие rx_pkt",
           "packet_view_t\nrx_full: событие готово", parent="data-ctrl", target=sheet("write", "capture"))
    p.node("word", 1220, 1310, 400, 185, "Буфер data4bytes",
           "Упаковка байтов\nОбмен с BRAM", parent="data-ctrl", target=sheet("memory-format", "packing"))
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


def addressing(d):
    p = d.p
    p.node("pins", 60, 312.5, 310, 150, "SDA и SCL", "Шина I²C", "L0")
    p.frame("engine", 450, 190, 1270, 1090, "i2c_slave_engine")
    p.frame("inputs", 490, 255, 1190, 220, "Обработка входов", parent="engine")
    p.node("debouncer", 540, 325, 400, 125, "debouncer", "Фильтрация", parent="inputs")
    p.node("sync", 1210, 325, 410, 125, "sync", "Синхронизация с clk", parent="inputs")
    p.edge("pins", "debouncer", "data", start="E", end="W")
    p.edge("debouncer", "sync", "data")
    p.node("start", 1210, 565, 410, 140, "IDLE → ADDR", "START", parent="engine")
    p.edge("sync", "start", "data")
    p.node("address", 730, 565, 340, 140, "Выбор адреса", "phy_addr_list[addr]", parent="engine")
    p.edge("start", "address", "data", "7 бит", label_xy=(1140, 610))
    p.node("regs", 60, 565, 330, 140, "regs", "ADDR_LIST_0…3")
    p.edge("regs", "address", "mmio", "Карта", label_xy=(590, 610))
    p.node("ignored", 510, 835, 330, 140, "IDLE", "SDA отпущена", parent="engine")
    p.edge("address", "ignored", "data", "Адрес не выбран", start="S", end="N",
           a_offset=-90, via=((810, 765), (675, 765)), label_xy=(650, 743))
    p.node("direction", 970, 835, 470, 140, "OP_CODE", "Направление и событие start", parent="engine")
    p.edge("address", "direction", "data", "Адрес выбран", start="S", end="N", a_offset=90,
           via=((990, 765), (1205, 765)), label_xy=(1100, 743))
    p.node("write", 550, 1100, 450, 140, "WRITE_ACK", "ACK адреса → WRITE_BYTE", parent="engine",
           target=sheet("write"))
    p.node("read", 1170, 1100, 480, 140, "WAIT_PACKET", "SCL = 0 до первого байта", parent="engine",
           target=sheet("read"))
    p.edge("direction", "write", "data", "W = 0", start="S", end="N", a_offset=-120,
           via=((1085, 1035), (775, 1035)), label_xy=(875, 1010))
    p.edge("direction", "read", "data", "R = 1", start="S", end="N", a_offset=120,
           via=((1325, 1035), (1410, 1035)), label_xy=(1510, 1040))


def participants(d):
    return d.actors([
        ("host", "Ведущий", "Внешняя шина", "L0", "i2c/protocol/transfers.svg", 1),
        ("engine", "i2c_slave_engine", "Биты и ACK", "PL", sheet("structure", "engine"), 1.25),
        ("data-ctrl", "data_ctrl", "Байты и слова", "PL", sheet("structure", "data-ctrl"), 1),
        ("bram", "BRAM I²C", "Запрос и ответ", "PL", sheet("buffers", "bram"), .9),
        ("ps", "Программа PS", "Сервис и драйвер", "L3", "ps-interaction/i2c-irq.svg#service", 1.1),
    ], core=("engine", "data-ctrl"), core_title="I2C_slave_0")


def write(d):
    d.strip("wire", 220, [condition("S"), address(example=True), ack(), field("B0", 150), ack(),
                          omitted(), field("B5", 150), ack(), condition("P")])
    start = participants(d)
    d.sequence(start, [
        ("address-write", "host", "engine", "data", "Адрес + W"),
        ("start-event", "engine", "data-ctrl", "data", "start, rw = 0"),
        ("capture", ("host", "engine"), ("engine", "data-ctrl"), "data", "Байты B0…B5"),
        ("full-word", "data-ctrl", "bram", "data", "Полное слово B3…B0"),
        ("tail-event", ("host", "engine"), ("engine", "data-ctrl"), "data", "STOP → stop"),
        ("tail", "data-ctrl", "bram", "data", "Хвост → заголовок"),
        ("publish", "data-ctrl", "ps", "irq", "DATA_VALID и FINAL_PACKET"),
        ("consume", "bram", "ps", "data", "Фрагмент записи"),
    ])


def read(d):
    d.strip("wire", 220, [condition("S"), address(True, example=True),
                          field("Ожидание", 240, "ведомый", "SCL = 0", "condition"),
                          ack(), field("B0", 180, "ведомый"), ack("ведущий"), omitted()])
    start = participants(d)
    d.sequence(start, [
        ("wait-first", "host", "engine", "data", "Адрес + R; SCL = 0"),
        ("read-start", "engine", "data-ctrl", "data", "start, rw = 1"),
        ("request", "data-ctrl", "ps", "irq", "RD_REQUEST через L1"),
        ("prepare", "ps", "bram", "data", "Заголовок и ответ"),
        ("accept", "ps", "data-ctrl", "mmio", "CSR.rd_valid через L1"),
        ("fetch", "bram", "data-ctrl", "data", "Заголовок → слово"),
        ("first-byte", "data-ctrl", "engine", "data", "Байт → tx_full"),
        ("address-ack", "engine", "host", "data", "Отпустить SCL → ACK адреса → B0"),
    ])


def read_continuation(d):
    p = d.p
    p.frame("data-ctrl", 380, 220, 990, 1120, "data_ctrl")
    p.node("send", 440, 330, 450, 160, "RD_SEND_BYTE", "Байт при tx_ready", parent="data-ctrl")
    p.node("engine", 1430, 330, 310, 160, "i2c_slave_engine", "tx_data → SDA",
           target=sheet("read", "first-byte"))
    p.edge("send", "engine", "data", "Следующий байт", label_xy=(1180, 385))
    p.node("next", 440, 690, 450, 145, "Следующий байт окна", "После передачи в engine", parent="data-ctrl")
    p.edge("send", "next", "data")
    p.edge("next", "send", "data", "Есть байты в слове", start="W", end="W",
           via=((400, 762.5), (400, 410)), label_xy=(265, 590))
    p.node("word", 1010, 690, 310, 145, "RD_MEM_DATA", "Следующее слово", parent="data-ctrl")
    p.edge("next", "word", "data", "Слово", label_xy=(945, 730))
    p.edge("word", "send", "data", start="N", end="E", b_offset=40,
           via=((1165, 450),))
    p.node("bram", 1430, 690, 310, 145, "BRAM I²C", "Окно ответа", target=sheet("buffers", "rd-window"))
    p.edge("bram", "word", "data", "Слово", label_xy=(1375, 735))
    p.node("request", 440, 1110, 450, 145, "RD_REQ", "Ожидание rd_valid", parent="data-ctrl")
    p.edge("next", "request", "data", "Окно выдано", label_xy=(800, 975))
    p.node("ps", 1430, 1110, 310, 145, "Программа PS", "Новое окно", "L3",
           target="ps-interaction/i2c-irq.svg#service")
    p.edge("request", "ps", "irq", "RD_REQUEST", label_xy=(1150, 1150))
    p.edge("ps", "bram", "data", "Ответ через L1", start="N", end="S",
           label_xy=(1700, 975))
    p.edge("ps", "request", "mmio", "rd_valid после записи", start="S", end="S",
           via=((1585, 1430), (665, 1430)), label_xy=(1100, 1405))
    p.edge("request", "send", "data", "Новое окно: заголовок и слово", start="W", end="N",
           via=((110, 1182.5), (110, 175), (665, 175)), label_xy=(365, 150))
    p.max_y = max(p.max_y, 1460)


def repeated_start(d):
    p = d.p
    d.canvas(2200)
    d.strip("wire", 220, [condition("S"), address(width=195, example=True), ack(),
                          field("0x10", 130, detail="указатель"), ack(), condition("Sr"),
                          address(True, width=195, example=True), ack(), field("D0", 130, "ведомый"),
                          ack("ведущий"), field("D1", 130, "ведомый"), ack("ведущий", True), condition("P")])
    p.node("pointer-event", 760, 440, 680, 150, "Повторный START", "restart → ADDR")
    p.frame("write-event", 80, 755, 900, 300, "Публикация записи")
    p.node("write-data", 120, 850, 820, 155, "data_ctrl → BRAM +0x1000",
           "Байт 0x10 → заголовок", parent="write-event", target=sheet("write", "tail"))
    p.frame("read-phase", 1220, 755, 900, 300, "Начало чтения")
    p.node("read-address", 1260, 850, 820, 155, "i2c_slave_engine → data_ctrl",
           "Адрес + R → WAIT_PACKET; SCL = 0", parent="read-phase", target=sheet("read", "wait-first"))
    p.edge("pointer-event", "write-event", "data", start="S", end="N", a_offset=-150,
           via=((950, 670), (530, 670)))
    p.edge("pointer-event", "read-phase", "data", start="S", end="N", a_offset=150,
           via=((1250, 670), (1670, 670)))
    p.node("pointer", 620, 1240, 960, 185, "bvstk_i2c_slave_service",
           "Сохранить указатель 0x10 → подготовить ответ", "L3",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_slave_service")
    p.edge("write-event", "pointer", "irq", "DATA_VALID", start="S", end="N", b_offset=-300,
           via=((530, 1140), (800, 1140)), label_xy=(480, 1120))
    p.edge("read-phase", "pointer", "irq", "RD_REQUEST", start="S", end="N", b_offset=300,
           via=((1670, 1140), (1400, 1140)), label_xy=(1730, 1120))
    p.node("reply", 620, 1580, 960, 150, "BRAM: ответ +0x2000", "Заголовок и байты",
           target=sheet("buffers", "rd-window"))
    p.edge("pointer", "reply", "data", "Запись через L1", label_xy=(1280, 1520))
    p.edge("pointer", "read-address", "mmio", "rd_valid после записи", start="E", end="E",
           via=((2150, 1332.5), (2150, 927.5)), label_xy=(1880, 1308))
    p.node("continue", 620, 1890, 960, 150, "i2c_slave_engine", "tx_full → ACK адреса → D0 → D1",
           target=sheet("read", "address-ack"))
    p.edge("reply", "continue", "data")
    p.node("host-read", 80, 1890, 350, 150, "Внешний ведущий", "ACK → NACK → STOP", "L0")
    p.edge("continue", "host-read", "data", "SDA", label_xy=(530, 1938))


def buffers(d):
    p = d.p
    p.frame("bram", 560, 230, 680, 720, "BRAM I²C")
    p.frame("wr-window", 595, 320, 610, 225, "Запрос: +0x1000", parent="bram")
    p.node("wr-header", 625, 405, 225, 110, "Заголовок", "", parent="wr-window")
    p.node("wr-data", 905, 405, 265, 110, "Байты записи", "", parent="wr-window")
    p.frame("rd-window", 595, 660, 610, 225, "Ответ: +0x2000", parent="bram")
    p.node("rd-header", 625, 745, 225, 110, "Заголовок", "", parent="rd-window")
    p.node("rd-data", 905, 745, 265, 110, "Байты ответа", "", parent="rd-window")
    p.node("writer", 60, 375, 340, 145, "data_ctrl", "Запись запроса")
    p.node("reader", 60, 715, 340, 145, "data_ctrl", "Чтение ответа")
    p.node("consume", 1390, 375, 350, 145, "Драйвер PS", "После DATA_VALID", "L3")
    p.node("publish", 1390, 715, 350, 145, "Драйвер PS", "Перед rd_valid", "L3")
    p.edge("writer", "wr-window", "data", start="E", end="W", b_offset=15)
    p.edge("wr-window", "consume", "data", "Через L1", start="E", end="W", a_offset=15,
           label_xy=(1300, 420))
    p.edge("publish", "rd-window", "data", "Через L1", start="W", end="E", b_offset=15,
           label_xy=(1300, 760))
    p.edge("rd-window", "reader", "data", start="W", end="E", a_offset=15)


def memory_format(d):
    d.memory_format([("num_bytes", "31…7", 1250), ("phy_addr", "6…0", 350)])


def completion(d):
    p = d.p
    p.frame("events", 60, 250, 500, 550, "События ядра")
    p.node("engine", 100, 335, 420, 150, "i2c_slave_engine", "ERR_ACK", parent="events")
    p.node("data-ctrl", 100, 585, 420, 170, "data_ctrl", "DATA_VALID, RD_REQUEST\nFINAL_PACKET, MEM_OVERFLOW", parent="events")
    p.node("regs", 780, 475, 380, 155, "regs: IRQ и REQ", "Фиксация событий")
    p.node("ps", 1360, 475, 380, 155, "Драйвер PS", "Чтение и подтверждение", "L3",
           target="ps-interaction/i2c-irq.svg")
    p.edge("engine", "regs", "irq", start="E", end="N",
           via=((660, 410), (970, 410)))
    p.edge("data-ctrl", "regs", "irq", start="E", end="S",
           via=((660, 670), (970, 670)))
    p.edge("regs", "ps", "irq", "IRQ через L1", label_xy=(1260, 530))
    p.edge("ps", "regs", "mmio", "Подтверждение через L1", start="S", end="S",
           a_offset=100, b_offset=80, via=((1650, 880), (1050, 880)), label_xy=(1380, 854))
    p.max_y = max(p.max_y, 920)



def generate_i2c_slave(page_class, element, palette, arrows):
    sources = [RTL+name for name in ("i2c_slave_wrapper.sv", "top_i2c_slave.sv", "i2c_slave_engine.sv", "data_ctrl.sv", "regs.sv", "include/i2c_slave_pkg.sv")]
    sources += [COMMON+name for name in ("debouncer.sv", "sync.sv", "axi_lite/axi_lite_master_native.sv", "axi_lite/axi_lite_slave_native.sv")]
    sources += ["../hw_platform/fpga/Burevestnik_21.tcl", DRIVER, SERVICE,
                "src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c",
                "src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c"]
    for name, title, draw in (
        ("structure", "I²C slave: состав ядра", structure),
        ("addressing", "I²C slave: выбор адреса", addressing),
        ("write", "I²C slave: приём записи", write),
        ("read", "I²C slave: первый байт ответа", read),
        ("read-continuation", "I²C slave: продолжение чтения", read_continuation),
        ("repeated-start", "I²C slave: указатель и повторный START", repeated_start),
        ("buffers", "I²C slave: окна запроса и ответа", buffers),
        ("memory-format", "I²C slave: формат окна BRAM", memory_format),
        ("completion", "I²C slave: события и подтверждение IRQ", completion),
    ):
        p = page_class(sheet(name), title, "", sources)
        draw(CoreDrawing(p, element, palette, arrows))
