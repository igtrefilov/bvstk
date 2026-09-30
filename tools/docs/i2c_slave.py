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
           "Карта адресов, CSR и REQ\nФлаги и подтверждение IRQ", target=sheet("completion", "irq"), parent="core")
    p.node("axi-s", 1180, 490, 440, 195, "axi_lite_slave_native",
           "Доступ PS к регистрам\nПорт s_axi", parent="core")
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
           "debouncer, затем sync\nФронты SDA и SCL", parent="engine", target=sheet("addressing", "inputs"))
    p.node("fsm", 510, 1280, 500, 215, "Автомат I²C",
           "Адрес, биты и ACK\npacket и байт ответа tx_data\nУдержание SCL в WAIT_PACKET",
           parent="engine", target=sheet("read", "wait-first"))
    p.edge("inputs", "fsm", "data")
    p.frame("data-ctrl", 1180, 920, 480, 665, "data_ctrl", parent="core", target=sheet("buffers", "data-ctrl"))
    p.node("rx-pkt", 1220, 1010, 400, 155, "Событие rx_pkt",
           "packet_view_t\nПризнак ожидания rx_full", parent="data-ctrl", target=sheet("write", "capture"))
    p.node("word", 1220, 1310, 400, 185, "Буфер data4bytes",
           "Байты и слова\nАвтомат доступа к BRAM", parent="data-ctrl", target=sheet("buffers", "packing"))
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
           "Чтение и запись BRAM\nПорт m_axi", parent="core")
    p.edge("word", "axi-m", "data", start="S", end="N", bidirectional=True)
    p.node("bram", 1180, 2110, 480, 195, "BRAM I²C",
           "Запрос: +0x1000\nОтвет: +0x2000", target=sheet("buffers", "bram"))
    p.edge("axi-m", "bram", "data", "Через AXI и контроллер BRAM", start="S", end="N",
           label_xy=(1660, 1990), bidirectional=True)
    p.edge("ps", "bram", "data", "Данные через L1", start="S", end="E",
           via=((1930, 2207.5),), label_xy=(1800, 2184), bidirectional=True)
    p.note(2420, "Роль slave относится к шине I²C. Через m_axi это же ядро само начинает обращения к BRAM.")
    p.note(2480, "Ядро передаёт принятые байты в PS. Значение первого байта как номера регистра определяет сервис L3.")
    d.finish(2580, "Далее: как ядро выбирает обращение на шине", sheet("addressing"))


def addressing(d):
    p = d.p
    d.strip("address-fields", 320, [condition("S"), field("Адрес", 270, detail="7 бит"),
                                    field("R/W", 125, detail="1 бит"), ack()], x=110)
    d.link(1110, 370, "Адресная фаза в протоколе", "i2c/protocol/transaction.svg#address")
    p.frame("engine", 420, 580, 1320, 1800, "i2c_slave_engine", target=sheet("structure", "engine"))
    p.frame("inputs", 465, 660, 1225, 280, "Обработка входов", parent="engine")
    p.node("debouncer", 540, 770, 400, 130, "debouncer", "По экземпляру для SDA и SCL", parent="inputs")
    p.node("sync", 1150, 770, 440, 130, "sync", "Уровни и фронты в домене clk", parent="inputs")
    p.edge("debouncer", "sync", "data")
    p.node("pins", 60, 760, 300, 150, "SDA и SCL", "Сигналы шины", "L0", target="i2c/protocol/interface.svg#bus")
    p.edge("pins", "debouncer", "data")
    p.node("start", 870, 1080, 420, 165, "IDLE → ADDR",
           "Спад SDA при высоком SCL\nНачать сборку адреса", parent="engine")
    p.edge("sync", "start", "data", start="S", end="N", via=((1370, 995), (1080, 995)))
    p.node("address", 870, 1380, 420, 205, "ADDR",
           "Принять 7 бит адреса\nПроверить phy_addr_list[addr]", parent="engine")
    p.edge("start", "address", "data")
    p.node("regs", 1820, 1180, 320, 210, "regs",
           "128-битная карта\nADDR_LIST_0…3", target=sheet("structure", "regs"))
    p.edge("regs", "address", "mmio", "Карта выбранных адресов", start="W", end="N",
           b_offset=135, via=((1215, 1285),), label_xy=(1530, 1262))
    p.node("ignored", 1390, 1395, 300, 175, "IDLE",
           "Адрес не выбран\nSDA отпущена", parent="engine", target=sheet("completion", "outcomes"))
    p.edge("address", "ignored", "data", "0", start="E", end="W", label_xy=(1340, 1455))
    p.node("direction", 870, 1750, 420, 190, "OP_CODE",
           "Принять бит направления\nСобытие start в data_ctrl", parent="engine")
    p.edge("address", "direction", "data", "Адрес выбран", label_xy=(1160, 1670))
    p.node("write", 465, 2110, 540, 200, "Запись: WRITE_ACK",
           "Подтвердить адрес\nЗатем принимать WRITE_BYTE", parent="engine", target=sheet("write", "address-write"))
    p.node("read", 1160, 2110, 540, 200, "Чтение: WAIT_PACKET",
           "Удерживать SCL\nДождаться первого байта", parent="engine", target=sheet("read", "wait-first"))
    p.edge("direction", "write", "data", "W = 0", start="S", end="N", a_offset=-120,
           via=((960, 2020), (735, 2020)), label_xy=(785, 1997))
    p.edge("direction", "read", "data", "R = 1", start="S", end="N", a_offset=120,
           via=((1200, 2020), (1430, 2020)), label_xy=(1380, 1997))
    p.note(2470, "При чтении ACK адреса выдаётся после появления tx_full. Путь через WAIT_PACKET раскрыт на листе чтения.")
    p.note(2530, "Повторный START приводит через RESTART к ADDR: адрес выбирается заново для следующей фазы.")
    d.finish(2630, "Продолжение: приём записи", sheet("write"))


def write(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(example=True), ack(), field("B0", 160), ack(),
                          omitted(), field("B5", 160), ack(), condition("P")])
    d.text(1480, 355, "Пример: шесть байтов данных.\nACK выдаёт аппаратный автомат.", 22, width=600)
    start = d.actors()
    end = d.sequence(start, [
        ("address-write", "host", "engine", "data", "Адрес + W: ADDR → OP_CODE → WRITE_ACK", "Адрес выбран в карте; ядро подтверждает адрес и переходит в WRITE_BYTE.", sheet("addressing", "write")),
        ("start-event", "engine", "data-ctrl", "data", "Передать событие начала фазы", "packet_view_t: start = 1, rw = 0, addr = 0x2A. data_ctrl обнуляет счётчик нового фрагмента.", None),
        ("capture", ("host", "engine"), ("engine", "data-ctrl"), "data", "WRITE_BYTE: принять байт и передать его в data_ctrl", "WRITE_ACK подтверждает байт. Событие сохраняется в rx_pkt; data4bytes заполняется от младших разрядов.", sheet("buffers", "packing")),
        ("full-word", "data-ctrl", "bram", "data", "WR_MEM_DATA: записать собранное слово", "После B3 в BRAM записывается слово B3 B2 B1 B0. Затем приём продолжается.", sheet("buffers", "wr-window")),
        ("tail-event", ("host", "engine"), ("engine", "data-ctrl"), "data", "STOP после B5: передать событие stop в data_ctrl", "Автомат отпускает линии. В data4bytes остался хвост из двух полезных байтов; его нужно сохранить.", sheet("repeated-start")),
        ("tail", "data-ctrl", "bram", "data", "WR_MEM_DATA → WR_MEM_HEADER", "Сначала слово с B4 и B5, затем заголовок с числом байтов 6 и адресом 0x2A.", sheet("buffers", "header")),
        ("publish", "data-ctrl", "ps", "irq", "После записи заголовка: DATA_VALID и FINAL_PACKET", "regs фиксирует признаки и поднимает IRQ. Драйвер PS получает событие через L1.", sheet("completion", "irq")),
        ("consume", "bram", "ps", "data", "PS читает опубликованный фрагмент", "Заголовок задаёт число полезных байтов. Сервис L3 разбирает их смысл и применяет правила устройства.", "ps-interaction/i2c-irq.svg#service"),
    ])
    p.note(end+50, "ACK на шине подтверждает аппаратный приём байта. Решение о дальнейшей записи в устройство принимает сервис PS.")
    p.note(end+112, "При заполнении окна data_ctrl публикует промежуточный фрагмент с DATA_VALID и продолжает с начала той же области.")
    d.finish(end+205, "Формат слов, заголовка и повторное использование окна", sheet("buffers"))


def read(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(True, example=True),
                          field("Ожидание", 235, "ведомый", "SCL = 0", "condition"), ack(),
                          field("B0", 160, "ведомый"), ack("ведущий"), omitted(),
                          field("Bn", 160, "ведомый"), ack("ведущий", True), condition("P")])
    d.text(60, 525, "Успешный путь: сервис PS подготовлен к чтению и успевает предоставить первый байт.", 21, color="#576c7b")
    start = d.actors(y=710)
    end = d.sequence(start, [
        ("wait-first", "host", "engine", "data", "Адрес + R: OP_CODE → WAIT_PACKET", "ack_pending = 1. Ядро удерживает SCL до готовности первого байта; ACK адреса ещё не выдан.", sheet("addressing", "read")),
        ("read-start", "engine", "data-ctrl", "data", "Событие start с rw = 1 переводит data_ctrl в RD_REQ", "Начинается ожидание control_s.rd_valid.", None),
        ("request", "data-ctrl", "ps", "irq", "RD_REQUEST через regs сообщает PS о запросе", "Доставка IRQ и обработчик выбранной ОС раскрыты на отдельном листе.", "ps-interaction/i2c-irq.svg"),
        ("prepare", "ps", "bram", "data", "Подготовить окно ответа", "Драйвер записывает заголовок и байты в область +0x2000. Значения выбирает сервис L3.", sheet("buffers", "rd-window")),
        ("accept", "ps", "data-ctrl", "mmio", "Записать CSR.rd_valid через L1 и regs", "Это разрешает data_ctrl перейти из RD_REQ к чтению подготовленного окна.", sheet("completion", "irq")),
        ("fetch", "bram", "data-ctrl", "data", "RD_MEM_HEADER → RD_MEM_DATA", "Через m_axi прочитать длину ответа и первое слово данных.", sheet("buffers", "rd-window")),
        ("first-byte", "data-ctrl", "engine", "data", "RD_SEND_BYTE: передать байт при tx_ready = 1", "Строб заполняет tx_data и устанавливает tx_full.", None),
        ("address-ack", "engine", "host", "data", "WAIT_PACKET → WRITE_ACK: подтвердить адрес", "Первый байт готов. Ядро отпускает SCL и начинает READ_BYTE.", None),
        ("send-byte", "engine", "host", "data", "READ_BYTE: выдать восемь бит по SDA", "После байта SDA отпускается для ответа ведущего. tx_data освобождается для следующего байта.", None),
        ("host-ack", "host", "engine", "data", "READ_ACK: принять ACK или NACK ведущего", "ACK ведёт в WAIT_PACKET; NACK ведёт в DONE с ожиданием STOP или повторного START.", sheet("completion", "outcomes")),
    ])
    d.heading("refill", end+65, "data_ctrl готовит следующие байты независимо от тактов на шине")
    bottom = d.table("refill-cases", end+145, [
        ("После передачи байта в engine", 720, "PL"), ("Следующее действие data_ctrl", 1320, "PL")], [
        ("В слове и окне ещё есть данные", "RD_SEND_BYTE: дождаться tx_ready и выдать следующий байт."),
        ("Слово закончилось, окно продолжается", "RD_MEM_DATA: прочитать следующее слово BRAM, затем RD_SEND_BYTE."),
        ("Выдан последний байт окна или достигнут его предел", "RD_REQ: снова поднять RD_REQUEST и ожидать новое окно от PS."),
    ])
    p.note(bottom+70, "Запрос нового окна может возникнуть до ACK ведущего на последний байт. STOP и повторный START прекращают текущую фазу.")
    p.note(bottom+132, "Ожидание каждого байта ограничивает STRETCH_TIMEOUT_US. Поведение при истечении срока зависит от этапа чтения.")
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
           "Принять байт и передать его в data_ctrl\nПри Sr: RESTART → ADDR; передать событие restart",
           target=sheet("write", "tail-event"))
    p.edge("pointer-byte", "pointer-event", "data")

    p.frame("write-event", 100, 1040, 830, 690, "Запись завершается", target=sheet("write", "tail"))
    p.node("write-data", 140, 1140, 750, 220, "data_ctrl",
           "WR_MEM_DATA: сохранить один байт 0x10\nWR_MEM_HEADER: записать длину 1 и адрес 0x2A",
           parent="write-event", target=sheet("buffers", "header"))
    p.node("pointer-publish", 140, 1510, 750, 160, "BRAM: запрос +0x1000",
           "После записи заголовка фрагмент готов для PS", parent="write-event", target=sheet("buffers", "wr-window"))
    p.edge("write-data", "pointer-publish", "data", "Данные, затем заголовок", label_xy=(725, 1443))

    p.frame("read-phase", 1180, 1040, 890, 690, "Чтение начинается", target=sheet("read", "wait-first"))
    p.node("read-address", 1220, 1140, 810, 220, "i2c_slave_engine",
           "Ведущий передаёт адрес 0x2A + R\nADDR → OP_CODE → WAIT_PACKET\nУдерживать SCL до появления первого байта",
           parent="read-phase", target=sheet("addressing", "read"))
    p.node("read-request", 1220, 1510, 810, 160, "data_ctrl: RD_REQ",
           "Запросить ответ у PS и ждать rd_valid", parent="read-phase", target=sheet("read", "request"))
    p.edge("read-address", "read-request", "data", "Событие start, rw = 1", label_xy=(1860, 1443))

    p.edge("pointer-event", "write-event", "data", start="S", end="N",
           via=((1075, 950), (515, 950)))
    p.edge("pointer-event", "read-phase", "data", start="S", end="N",
           via=((1075, 950), (1625, 950)))
    d.el(p.notes, "circle", cx=1075, cy=950, r=5, fill=d.data)
    d.text(1075, 1000, "Параллельно", 21, bold=True, anchor="middle", color=d.data)

    p.node("pointer", 680, 2010, 900, 275, "bvstk_i2c_slave_service",
           "DATA_VALID: прочитать окно запроса\nregister_pointer = 0x10; read_armed = 1\nЗатем обработать RD_REQUEST\nПодготовить ответ от сохранённого указателя", "L3",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_slave_service")
    p.edge("write-event", "pointer", "irq", "DATA_VALID и FINAL_PACKET", start="S", end="N", b_offset=-280,
           via=((515, 1870), (850, 1870)), label_xy=(485, 1837))
    p.edge("read-request", "pointer", "irq", "RD_REQUEST", start="S", end="N", b_offset=280,
           via=((1625, 1900), (1410, 1900)), label_xy=(1785, 1867))

    p.node("reply", 680, 2480, 900, 205, "BRAM: ответ +0x2000",
           "Драйвер PS записывает заголовок и байты\nПосле завершения записи передаёт rd_valid через L1 и regs",
           target=sheet("buffers", "rd-window"))
    p.edge("pointer", "reply", "data", "Запись через L1", label_xy=(1320, 2405))
    p.edge("pointer", "read-request", "mmio", "rd_valid после записи окна", start="E", end="E",
           via=((2150, 2147.5), (2150, 1590)), label_xy=(1870, 2117))

    p.node("continue", 680, 2860, 900, 245, "Выдача ответа",
           "data_ctrl читает окно и заполняет tx_data\nПервый байт: tx_full разрешает ACK адреса\ni2c_slave_engine выдаёт D0 и D1", target=sheet("read", "fetch"))
    p.edge("reply", "continue", "data")
    p.node("host-read", 60, 2860, 400, 245, "Внешний ведущий",
           "ACK после D0\nNACK после D1\nЗатем STOP", "L0", target=sheet("completion", "outcomes"))
    p.edge("continue", "host-read", "data", "SDA", start="W", end="E")

    p.note(3250, "Признаки записи и чтения могут прийти раздельно или в одном IRQ. Обработчик PS сначала разбирает DATA_VALID.")
    p.note(3312, "DATA_VALID вместе с FINAL_PACKET завершает запись указателя. FINAL_PACKET без DATA_VALID завершает чтение в PS.")
    p.note(3374, "В текущем сервисе корректный указатель сохраняется и между двумя обменами с STOP. Это поведение программы PS.")
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
           "Заголовок, затем байты\nПосле записи — rd_valid\nДоступ через L1", "L3", parent="ps")
    p.edge("writer", "wr-window", "data", "Запись", start="E", end="W", b_offset=22.5, label_xy=(605, 584))
    p.edge("wr-window", "consume", "data", "Чтение", start="E", end="W", a_offset=22.5, label_xy=(1640, 584))
    p.edge("publish", "rd-window", "data", "Запись", start="W", end="E", b_offset=40, label_xy=(1640, 1005))
    p.edge("rd-window", "reader", "data", "Чтение", start="W", end="E", a_offset=40, label_xy=(595, 1005))

    d.heading("header", 1360, "Первое слово окна — header_t")
    for x, w, title, detail in ((130, 1380, "num_bytes", "Биты 31…7: число полезных байтов текущего фрагмента"),
                               (1535, 530, "phy_addr", "Биты 6…0: адрес на I²C")):
        el(p.blocks, "rect", x=x, y=1430, width=w, height=115, rx=6, fill="white", stroke=d.data, stroke_width=2)
        d.text(x+w/2, 1472, title, 25, bold=True, anchor="middle", color=d.data)
        d.text(x+w/2, 1515, detail, 20, anchor="middle")
    p.note(1610, "В заголовке нет бита направления и номера регистра. Для чтения data_ctrl берёт длину из num_bytes.")

    d.heading("packing", 1730, "Байты располагаются от младших разрядов слова к старшим")
    for i, label in enumerate(("B3", "B2", "B1", "B0")):
        xx = 130+480*i
        el(p.blocks, "rect", x=xx, y=1810, width=465, height=105, rx=5, fill="white", stroke=d.data, stroke_width=2)
        d.text(xx+232.5, 1850, label, 27, bold=True, anchor="middle", color=d.data)
        d.text(xx+232.5, 1891, f"Биты {31-8*i}…{24-8*i}", 20, anchor="middle")
    d.text(60, 1990, "Пример шести байтов: 10 7B 20 30 40 50. Все значения ниже шестнадцатеричные.", 22)
    bottom = d.table("example", 2080, [("Смещение внутри окна", 520, "PL"), ("Слово", 520, "PL"), ("Назначение", 1000, "PL")], [
        ("+0x00", "0x0000032A", "Заголовок: адрес 0x2A, число байтов 6."),
        ("+0x04", "0x30207B10", "Первые четыре байта, B0 находится в младших восьми битах."),
        ("+0x08", "0x00005040", "Хвост из двух байтов. Верхние нули не входят в указанную длину."),
    ])
    d.heading("window-reuse", bottom+110, "Окно используется повторно")
    end = d.table("window-rules", bottom+200, [("Направление", 440, "PL"), ("Порядок работы", 1600, "PL")], [
        ("Запись ведущего", "Данные → заголовок → DATA_VALID. При STOP или Sr также FINAL_PACKET. При заполнении области публикуется промежуточный фрагмент, затем запись идёт с её начала."),
        ("Чтение ведущего", "PS записывает ответ и передаёт rd_valid. После выдачи окна в engine data_ctrl снова формирует RD_REQUEST. Счётчик отражает переданные в engine байты, включая ещё не подтверждённые на шине."),
    ])
    p.note(end+70, "MEM_DEPTH = 0x1000: четыре байта заголовка и до 4092 байтов данных. Программный сервис подготавливает окна до 64 байтов.")
    p.note(end+130, "MEM_OVERFLOW возникает, если при публикации очередного фрагмента записи IRQ ещё установлен.")
    d.finish(end+225, "События, подтверждение IRQ и завершение обмена", sheet("completion"))


def completion(d):
    p = d.p
    d.heading("outcomes", 295, "Что завершает фазу и что сообщает ядро")
    bottom = d.table("outcome-table", 400, [
        ("Событие", 345, "L0"), ("Действие ядра PL", 655, "PL"),
        ("Признаки IRQ", 425, "PL"), ("Обработка PS", 615, "L3")], [
        ("Адрес не выбран", "ADDR → IDLE. SDA отпущена; адрес не подтверждается.", "Нового события для PS нет.", "Этот запрос не передаётся сервису."),
        ("STOP или Sr после записи", "data_ctrl дописывает хвост и заголовок. engine: STOP → IDLE либо RESTART → ADDR.", "DATA_VALID\nFINAL_PACKET", "Прочитать фрагмент. Сочетание флагов завершает запись; подготовка чтения может сохраняться."),
        ("NACK ведущего после байта чтения", "READ_ACK → DONE. Линии отпущены; ожидание STOP или Sr.", "Сам NACK не создаёт ERR_ACK или FINAL_PACKET.", "Завершение фазы будет обработано по следующей границе на шине."),
        ("STOP или Sr во время чтения", "После обработки границы data_ctrl выходит из чтения в WAIT_EVENT.", "FINAL_PACKET без DATA_VALID. При выходе из WAIT_PACKET без байта возможен ERR_ACK.", "Снять read_armed и закончить подготовку ответа."),
        ("Истёк срок ожидания первого байта", "WAIT_PACKET → ERR_ACK. Адрес чтения получает NACK; ожидание STOP или Sr.", "ERR_ACK", "Снять подготовку чтения. Поздний ответ не возвращает автомат из ERR_ACK."),
        ("Истёк срок ожидания следующего байта", "WAIT_PACKET → READ_BYTE. На шину выдаётся 0xFF; затем снова принимается ответ ведущего.", "ERR_ACK", "Снять подготовку чтения. Флаг сам по себе не означает, что ведущий уже завершил обмен."),
        ("При публикации записи IRQ ещё установлен", "WR_MEM_HEADER завершён; область содержит новый фрагмент. Предыдущий мог быть потерян.", "MEM_OVERFLOW\nDATA_VALID", "Обработать ошибку и отменить подготовку ответа."),
        ("Аппаратный или программный сброс", "Состояния, флаги и карта адресов очищаются. Внешняя BRAM сохраняет содержимое.", "IRQ снимается.", "Восстановить карту адресов и нужное состояние перед новым обменом."),
    ])
    d.heading("irq", bottom+115, "regs фиксирует события до подтверждения со стороны PS")
    y = bottom+225
    p.node("events", 90, y, 520, 205, "События ядра",
           "i2c_slave_engine и data_ctrl\nИмпульсы признаков", target=sheet("structure", "core"))
    p.node("regs", 855, y, 500, 205, "regs: IRQ",
           "Флаги накапливаются\nIRQ остаётся установленным", target=sheet("structure", "regs"))
    p.node("ps", 1630, y, 510, 205, "Драйвер PS",
           "Прочитать IRQ и REQ\nПодтвердить через L1", "L3", target="ps-interaction/i2c-irq.svg")
    p.edge("events", "regs", "irq")
    p.edge("regs", "ps", "irq", "IRQ через L1")
    p.edge("ps", "regs", "mmio", "Запись бита 0 в IRQ", start="S", end="S",
           via=((1885, y+330), (1105, y+330)), label_xy=(1510, y+306))
    p.note(y+445, "Сброс IRQ снимает накопленные признаки. При совпадении со свежим событием regs отдаёт приоритет событию.")
    p.note(y+505, "Готовность ответа передаётся отдельно: запись бита rd_valid в CSR. Ожидание байта ограничивает STRETCH_TIMEOUT_US.")
    p.note(y+565, "ERR_ACK формируется при выходе из WAIT_PACKET без tx_full, включая прерывание ожидания границей транзакции.")
    d.finish(y+665, "Вернуться к составу ведомого ядра", sheet("structure"))


def generate_i2c_slave(page_class, element, palette, arrows):
    sources = [RTL+name for name in ("i2c_slave_wrapper.sv", "top_i2c_slave.sv", "i2c_slave_engine.sv", "data_ctrl.sv", "regs.sv", "include/i2c_slave_pkg.sv")]
    sources += [COMMON+name for name in ("debouncer.sv", "sync.sv", "axi_lite/axi_lite_master_native.sv", "axi_lite/axi_lite_slave_native.sv")]
    sources += ["../hw_platform/fpga/Burevestnik_21.tcl", DRIVER, SERVICE,
                "src/ports/freertos-xilinx/os/i2c/bvstk_i2c_slave_freertos.c",
                "src/ports/neutrino-zynq7000/os/i2c/bvstk_i2c_slave_neutrino.c"]
    for name, title, subtitle, draw in (
        ("structure", "I²C slave: состав ядра", "От линий шины до BRAM и регистров управления. Программа PS показана за границей PL.", structure),
        ("addressing", "I²C slave: приём адреса", "Отфильтровать входы, распознать START, выбрать адрес и направление обмена.", addressing),
        ("write", "I²C slave: приём записи", "Этапы на шине и действия модулей. Последовательность под участниками читается сверху вниз.", write),
        ("read", "I²C slave: выдача ответа", "Первый байт, подтверждение адреса и продолжение чтения из подготовленного окна.", read),
        ("repeated-start", "I²C slave: запись указателя и чтение", "После Sr ядро завершает запись и принимает новый адрес. Указатель регистра сохраняет сервис PS на L3.", repeated_start),
        ("buffers", "I²C slave: окна BRAM", "Кто записывает и читает каждое окно, как устроены заголовок и слова данных.", buffers),
        ("completion", "I²C slave: завершение и ошибки", "Состояние шины, аппаратные флаги и дальнейшие действия программы PS.", completion),
    ):
        p = page_class(sheet(name), title, subtitle, sources)
        draw(SlaveDrawing(p, element, palette, arrows))
