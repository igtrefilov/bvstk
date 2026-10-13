"""I2C master: commands, bus execution and the result path in the packaged RTL."""
from i2c_core import CoreDrawing
from i2c_protocol import ack, address, condition, field, omitted

BASE = "i2c/cores/master/"
RTL = "../hw_platform/fpga/ip_repo/i2c_master/src/"
DRIVER = "src/drivers/pl/i2c/bvstk_i2c_master.c"
SERVICE = "src/services/i2c/bvstk_i2c_master_service.c"


def sheet(name, fragment=""):
    return BASE+name+".svg"+("#"+fragment if fragment else "")


def structure(d):
    p = d.p
    d.canvas(2200)
    p.node("ps", 100, 310, 520, 220, "Сервис и драйвер PS",
           "bvstk_i2c_master_service\nbvstk_i2c_master_hw_transfer", "L3", target=sheet("commands", "ps"))
    p.node("platform", 820, 310, 400, 220, "Доступ PS", "MMIO", "L1",
           target="@architecture_bvstk_l1.svg#platform-access")
    p.node("irq-pin", 1550, 310, 510, 220, "IRQ → GIC",
           "Без обработчика PS", "L0", target=sheet("completion", "regs"))
    p.edge("ps", "platform", "call")

    p.frame("core", 100, 670, 2000, 1630, "I2C_master_0", target="@architecture_bvstk_ps_pl.svg#i2c-master")
    p.node("axi-s", 140, 810, 320, 190, "axi_lite_slave_native", "s_axi", parent="core")
    p.node("regs", 590, 810, 350, 190, "regs", "CSR, IRQ, TIMEOUT\nВход TX_FIFO", parent="core", target=sheet("completion", "regs"))
    p.node("tx-fifo", 1070, 810, 360, 190, "TX FIFO", "xpm_fifo_sync", parent="core", target=sheet("commands", "fifo"))
    p.node("tx-data-ctrl", 1590, 810, 440, 190, "tx_data_ctrl", "header_t → packet_view_t", parent="core", target=sheet("commands", "parser"))
    p.edge("platform", "axi-s", "mmio", "MMIO", start="S", end="N", b_offset=120,
           via=((1020, 610), (420, 610)), label_xy=(780, 588), bidirectional=True)
    p.edge("axi-s", "regs", "mmio", bidirectional=True)
    p.edge("regs", "tx-fifo", "data")
    p.edge("tx-fifo", "tx-data-ctrl", "data")
    p.edge("regs", "tx-data-ctrl", "mmio", "CSR.start", start="N", end="N",
           via=((765, 735), (1810, 735)), label_xy=(1300, 713))
    p.edge("regs", "irq-pin", "irq", start="N", end="S", a_offset=140,
           via=((905, 580), (1805, 580)))

    p.frame("engine", 1170, 1260, 860, 640, "i2c_master_engine", parent="core", target=sheet("timing", "engine"))
    p.node("fsm", 1220, 1350, 400, 455, "Автомат I²C",
           "START, RESTART, STOP", parent="engine", target=sheet("write", "address"))
    p.node("clock", 1710, 1350, 280, 180, "Такты SCL", "div_cnt, divisor\nscl_oe", parent="engine", target=sheet("timing", "clock"))
    p.node("inputs", 1710, 1625, 280, 180, "Входы", "debouncer → sync\nSDA и SCL", parent="engine", target=sheet("sampling"))
    p.edge("clock", "fsm", "clock", start="W", end="E", b_offset=-137.5)
    p.edge("inputs", "fsm", "data", start="W", end="E", b_offset=137.5)
    p.edge("tx-data-ctrl", "engine", "data", "tx_valid и tx_ready", start="S", end="N", b_offset=210,
           label_xy=(1940, 1140))
    p.edge("engine", "regs", "irq", "ERR_ACK", start="N", end="S", a_offset=-380,
           via=((1220, 1110), (765, 1110)), label_xy=(990, 1088))

    p.node("rx-data-ctrl", 690, 1350, 340, 180, "rx_data_ctrl", "rx_valid и rx_ready", parent="core", target=sheet("buffers", "rx-data-ctrl"))
    p.node("rx-fifo", 690, 1660, 340, 180, "RX FIFO", "xpm_fifo_sync", parent="core", target=sheet("buffers", "rx-fifo"))
    p.node("rx-fifo-mux", 140, 1660, 370, 180, "rx_fifo_mux", "Пакеты → слова BRAM", parent="core", target=sheet("buffers", "writer"))
    p.node("axi-m", 140, 2050, 370, 180, "axi_lite_master_native", "m_axi", parent="core")
    p.edge("engine", "rx-data-ctrl", "data", start="W", end="E", a_offset=-140)
    p.edge("rx-data-ctrl", "rx-fifo", "data")
    p.edge("rx-fifo", "rx-fifo-mux", "data")
    p.edge("rx-fifo-mux", "axi-m", "data")
    p.edge("rx-fifo-mux", "regs", "irq", "DATA_VALID", start="N", end="S", b_offset=-120,
           via=((325, 1240), (645, 1240)), label_xy=(430, 1218))

    p.node("bram", 140, 2460, 620, 220, "BRAM I²C: результат master",
           "0x40000000\nЗаголовок и данные", target=sheet("buffers", "bram"))
    p.node("device", 1220, 2460, 760, 220, "Ведомое устройство", "", "L0",
           target="i2c/protocol/interface.svg#target")
    p.edge("axi-m", "bram", "data", start="S", end="N", b_offset=-125)
    p.edge("engine", "device", "data", "SDA и SCL", label_xy=(1725, 2195), bidirectional=True)
    p.edge("bram", "ps", "data", start="W", end="W",
           via=((40, 2570), (40, 420)))


def timing(d):
    p = d.p
    d.canvas(2200)
    p.frame("engine", 60, 320, 1540, 940, "i2c_master_engine", target=sheet("structure", "engine"))
    p.node("speed", 100, 460, 360, 250, "packet.speed",
           "Выбор divisor", parent="engine", target=sheet("command-format", "header"))
    p.node("clock", 600, 460, 400, 250, "div_cnt и divisor",
           "Полупериод SCL", parent="engine")
    p.node("output", 1140, 460, 400, 250, "Выход SCL",
           "scl_oe", parent="engine")
    p.node("scl", 1750, 460, 390, 250, "Линия SCL",
           "", "L0", target=sheet("clock-stretching"))
    p.edge("speed", "clock", "mmio")
    p.edge("clock", "output", "clock")
    p.edge("output", "scl", "clock")
    p.node("inputs", 1140, 920, 400, 220, "debouncer и sync",
           "SCL: scl_pe и scl_ne\nSDA: уровень", parent="engine")
    p.node("fsm", 100, 920, 900, 220, "Автомат I²C",
           "READ_ACK\nREAD_BYTE", parent="engine")
    p.edge("scl", "inputs", "data", "scl_i", start="S", end="N",
           via=((1945, 810), (1340, 810)), label_xy=(1840, 787))
    p.edge("inputs", "fsm", "data")
    p.edge("fsm", "clock", "mmio", "Состояние", start="N", end="S",
           via=((550, 810), (800, 810)), label_xy=(675, 787))



def commands(d):
    p = d.p
    p.frame("ps", 60, 240, 520, 640, "Программа PS", "L3")
    p.node("master-service", 100, 335, 440, 150, "bvstk_i2c_master_service",
           "", "L3", parent="ps",
           target="i2c/services/master-operations.svg#service")
    p.node("driver", 100, 655, 440, 170, "Аппаратный драйвер",
           "bvstk_i2c_master_hw_transfer", "L3", parent="ps",
           target="i2c/drivers/master/transfer.svg#driver")
    p.edge("master-service", "driver", "call")
    p.frame("core", 740, 240, 1000, 1030, "I2C_master_0")
    p.node("regs", 800, 335, 350, 150, "regs", "TX_FIFO и CSR.start", parent="core")
    p.node("fifo", 800, 655, 350, 150, "TX FIFO", "Заголовки и данные", parent="core")
    p.node("parser", 1330, 655, 350, 150, "tx_data_ctrl", "Разбор команды", parent="core")
    p.node("engine", 1330, 1030, 350, 160, "i2c_master_engine", "tx_valid и tx_ready", parent="core",
           target=sheet("write"))
    p.edge("driver", "regs", "mmio", "Через L1", start="E", end="W",
           via=((655, 740), (655, 410)), label_xy=(650, 570))
    p.edge("regs", "fifo", "data", "Загрузка", label_xy=(1070, 580))
    p.edge("regs", "parser", "mmio", "CSR.start", start="E", end="N",
           via=((1505, 410),), label_xy=(1430, 385))
    p.edge("fifo", "parser", "data")
    p.edge("parser", "engine", "data", "packet_view_t", label_xy=(1630, 930))


def command_format(d):
    d.word("header", 250, [
        ("num_bytes", "31…11", 680), ("speed", "10…9", 180),
        ("restart", "8", 210), ("rw", "7", 160), ("phy_addr", "6…0", 370),
    ])


def write(d):
    d.strip("wire", 220, [condition("S"), address(example=True), ack(), field("0x10", 170), ack(),
                          field("0x7B", 170), ack(), condition("P")])
    start = d.actors([
        ("tx", "tx_data_ctrl", "Команда из TX FIFO", "PL", sheet("commands", "parser"), 1),
        ("engine", "i2c_master_engine", "", "PL", sheet("structure", "engine"), 1.2),
        ("device", "Ведомое устройство", "SDA и SCL", "L0", "i2c/protocol/interface.svg#target", 1),
    ], core=("tx", "engine"), core_title="I2C_master_0")
    d.sequence(start, [
        ("packet-address", "tx", "engine", "data", "Адресный пакет 0x54"),
        ("address", "engine", "device", "data", "START → адрес + W"),
        ("address-ack", "device", "engine", "data", "ACK адреса"),
        ("bytes", "tx", "engine", "data", "Байты 0x10 и 0x7B"),
        ("send", "engine", "device", "data", "SEND_BYTE: очередной байт"),
        ("byte-ack", "device", "engine", "data", "ACK после каждого байта"),
        ("stop", "engine", "device", "data", "STOP после последнего байта"),
    ])


def read(d):
    d.strip("wire", 220, [condition("S"), address(True, example=True), ack(), field("D0", 160, "ведомый"),
                          ack("ведущий"), field("D1", 160, "ведомый"), ack("ведущий", True), condition("P")])
    start = d.actors([
        ("tx", "tx_data_ctrl", "Длина команды: 2", "PL", sheet("commands", "parser"), 1),
        ("engine", "i2c_master_engine", "", "PL", sheet("structure", "engine"), 1.2),
        ("rx", "rx_data_ctrl", "Далее RX FIFO и BRAM", "PL", sheet("buffers", "receive"), 1.1),
        ("device", "Ведомое устройство", "SDA и SCL", "L0", "i2c/protocol/interface.svg#target", 1.1),
    ], core=("tx", "engine", "rx"), core_title="I2C_master_0")
    d.sequence(start, [
        ("packet-address", "tx", "engine", "data", "Адресный пакет 0x55"),
        ("address", "engine", "device", "data", "START → адрес + R"),
        ("address-ack", "device", "engine", "data", "ACK адреса"),
        ("request-byte", "tx", "engine", "data", "Пакеты приёма D0 и D1"),
        ("receive-byte", "device", "engine", "data", "READ_BYTE: очередной байт"),
        ("ack", "engine", "device", "data", "D0 → ACK; D1 → NACK"),
        ("capture", "engine", "rx", "data", "rx_valid и принятый пакет"),
        ("stop", "engine", "device", "data", "STOP"),
    ])


def repeated_start(d):
    p = d.p
    p.node("master-service", 60, 250, 620, 160, "bvstk_i2c_master_service",
           "read_reg: регистр 0x10, один байт", "L3",
           target="i2c/services/master-operations.svg#read")
    p.frame("queue", 850, 180, 890, 660, "TX FIFO: один запуск")
    p.node("write-header", 900, 275, 790, 125, "Заголовок W", "1 байт, restart = 1", parent="queue")
    p.node("pointer-word", 900, 475, 790, 125, "Данные записи", "Указатель 0x10", parent="queue")
    p.node("read-header", 900, 675, 790, 125, "Заголовок R", "1 байт, restart = 0", parent="queue")
    p.edge("write-header", "pointer-word", "data")
    p.edge("pointer-word", "read-header", "data")
    p.edge("master-service", "queue", "mmio", "Через L1", start="E", end="W", b_offset=-180,
           label_xy=(770, 300))
    p.node("parser", 60, 650, 620, 160, "tx_data_ctrl", "Очередь → пакеты байтов")
    p.edge("queue", "parser", "data", "CSR.start", start="W", end="E", a_offset=220,
           label_xy=(765, 700))
    p.frame("engine", 60, 1010, 1680, 350, "i2c_master_engine")
    p.node("write-phase", 100, 1110, 500, 170, "Запись указателя",
           "Адрес W → 0x10", parent="engine", target=sheet("write"))
    p.node("restart", 760, 1110, 290, 170, "Повторный START",
           "RESTART → TIMEOUT", parent="engine")
    p.node("read-phase", 1200, 1110, 500, 170, "Чтение значения",
           "Адрес R → D0 → NACK → STOP", parent="engine", target=sheet("read"))
    p.edge("parser", "write-phase", "data", "packet_view_t", start="S", end="N",
           a_offset=40, b_offset=60, label_xy=(550, 925))
    p.edge("write-phase", "restart", "data")
    p.edge("restart", "read-phase", "data")
    p.node("result", 1200, 1520, 500, 150, "RX FIFO → BRAM", "D0 → DATA_VALID", target=sheet("buffers"))
    p.node("master-service-result", 60, 1520, 620, 150, "bvstk_i2c_master_service", "Кэш и возврат D0", "L3",
           target="i2c/services/master-operations.svg#result")
    p.edge("read-phase", "result", "data")
    p.edge("result", "master-service-result", "data", "После DATA_VALID", label_xy=(940, 1570))


def buffers(d):
    p = d.p
    p.frame("receive", 60, 220, 1100, 970, "I2C_master_0: приёмные модули")
    for key, x, width, title, body in (
        ("engine", 100, 280, "i2c_master_engine", "rx_valid"),
        ("rx-data-ctrl", 440, 260, "rx_data_ctrl", "rx_ready"),
        ("rx-fifo", 780, 280, "RX FIFO", "packet_view_t"),
    ):
        p.node(key, x, 315, width, 155, title, body, parent="receive")
    p.edge("engine", "rx-data-ctrl", "data")
    p.edge("rx-data-ctrl", "rx-fifo", "data")
    p.frame("writer", 720, 650, 400, 490, "rx_fifo_mux", parent="receive")
    p.node("packing", 750, 740, 340, 135, "data4bytes", "Сборка слова", parent="writer")
    p.node("publish", 750, 970, 340, 135, "Запись BRAM", "SEND_BYTES → SEND_HEADER", parent="writer")
    p.edge("rx-fifo", "writer", "data")
    p.edge("packing", "publish", "data")
    p.frame("bram", 1280, 650, 460, 550, "BRAM: результат master")
    p.node("header", 1320, 745, 380, 135, "Заголовок", "+0x0000", parent="bram",
           target=sheet("memory-format", "header"))
    p.node("data", 1320, 1025, 380, 135, "Слова данных", "+0x0004", parent="bram",
           target=sheet("memory-format", "packing"))
    p.edge("publish", "bram", "data", "m_axi", start="E", end="W", b_offset=112.5,
           label_xy=(1185, 1010))
    p.node("regs", 100, 970, 440, 135, "regs: IRQ", "DATA_VALID", parent="receive")
    p.edge("publish", "regs", "irq")
    p.node("ps", 1280, 1340, 460, 170, "Драйвер PS", "", "L3",
           target=sheet("completion", "driver"))
    p.edge("bram", "ps", "data", "Через L1", start="S", end="N", label_xy=(1630, 1270))
    p.edge("ps", "regs", "mmio", "Опрос через L1", start="W", end="S",
           via=((320, 1425),), label_xy=(850, 1400))


def memory_format(d):
    d.memory_format([("0", "31…29", 220), ("byte_cnt", "28…7", 1000), ("phy_addr", "6…0", 380)])


def sampling(d):
    p = d.p
    p.used_layers.update(("L0", "PL"))
    x = 210
    for offset, label in ((100, "scl_ne"), (320, "scl_pe"), (540, "scl_ne"),
                          (760, "scl_pe"), (980, "scl_ne"), (1200, "scl_pe")):
        d.line(((x+offset, 290), (x+offset, 620)), color="#ced6dd", width=1.4, dash="5 7")
        d.text(x+offset, 265, label, 21, anchor="middle", color=d.palette["PL"][1])
    d.trace("SCL", x, 340, [(0, 1), (100, 1), (100, 0), (320, 0), (320, 1),
                            (540, 1), (540, 0), (760, 0), (760, 1),
                            (980, 1), (980, 0), (1200, 0), (1200, 1), (1440, 1)])
    d.trace("SDA", x, 510, [(0, 1), (140, 1), (140, 0), (580, 0), (580, 1),
                            (1020, 1), (1020, 0), (1440, 0)], color=d.data)
    for offset, bit in ((430, "0"), (870, "1"), (1310, "0")):
        d.text(x+offset, 650, bit, 28, bold=True, anchor="middle", color=d.data)
    p.max_y = 705


def clock_stretching(d):
    p = d.p
    p.used_layers.update(("L0", "PL"))
    d.el(p.under, "rect", x=450, y=265, width=550, height=375, fill=d.fill)
    d.trace("scl_oe", 210, 330, [(0, 1), (240, 1), (240, 0), (1240, 0)], color=d.palette["PL"][1])
    d.trace("SCL", 210, 520, [(0, 0), (790, 0), (790, 1), (1240, 1)])
    for x in (450, 1000):
        d.line(((x, 265), (x, 650)), width=1.5, dash="5 7")
    d.text(450, 710, "Ядро отпустило SCL", 21, anchor="middle")
    d.text(1000, 755, "Ведомый отпустил SCL", 21, anchor="middle")
    d.text(1000, 235, "scl_pe", 24, bold=True, anchor="middle", color=d.palette["PL"][1])
    d.text(1490, 520, "SDA = 0: ACK\nSDA = 1: NACK", 21, width=250)
    d.bracket(450, 1000, 845, "Ожидание подъёма SCL")
    p.max_y = 930


def completion(d):
    p = d.p
    p.node("engine", 60, 250, 470, 170, "i2c_master_engine", "core_state и ERR_ACK")
    p.node("writer", 60, 615, 470, 170, "rx_fifo_mux", "DATA_VALID")
    p.node("regs", 875, 440, 350, 180, "regs: CSR и IRQ", "")
    p.edge("engine", "regs", "irq", "ERR_ACK", start="E", end="N", a_offset=-35,
           via=((665, 300), (1050, 300)), label_xy=(910, 275))
    p.edge("engine", "regs", "mmio", "core_state", start="E", end="W", a_offset=55,
           via=((710, 390), (710, 530)), label_xy=(630, 445))
    p.edge("writer", "regs", "irq", "DATA_VALID", start="E", end="S",
           via=((665, 700), (1050, 700)), label_xy=(900, 675))
    p.frame("driver", 1320, 210, 420, 730, "Драйвер PS", "L3")
    p.node("poll", 1355, 400, 350, 165, "wait_completion", "Опрос CSR и IRQ", "L3", parent="driver")
    p.edge("poll", "regs", "mmio", "L1", start="W", end="E", b_offset=-47.5,
           label_xy=(1290, 460))
    p.node("success", 1355, 685, 350, 190, "Готово",
           "core_state = 0\nЧтение: новый DATA_VALID", "L3", parent="driver")
    p.edge("poll", "success", "call", "Без ERR_ACK", label_xy=(1625, 605))



def generate_i2c_master(page_class, element, palette, arrows):
    sources = [RTL+name for name in (
        "i2c_master_wrapper.sv", "top_i2c_master.sv", "i2c_master_engine.sv", "i2c_master_pkg.sv",
        "regs.sv", "tx_data_ctrl.sv", "rx_data_ctrl.sv", "rx_fifo_mux.sv", "debouncer.sv", "sync.sv",
        "axi_lite_master_native.sv", "axi_lite_slave_native.sv",
    )]
    sources += ["../hw_platform/fpga/ip_repo/i2c_master/component.xml", "../hw_platform/fpga/Burevestnik_21.tcl",
                DRIVER, DRIVER.replace(".c", ".h"), SERVICE, "src/hardware/pl/i2c/bvstk_i2c_regs.h",
                "src/hardware/boards/ax7020/bvstk_hw_config.h",
                "src/services/i2c/bvstk_i2c_slave_service.c"]
    for name, title, draw in (
        ("structure", "I²C master: состав ядра", structure),
        ("commands", "I²C master: подготовка команды", commands),
        ("command-format", "I²C master: заголовок header_t", command_format),
        ("write", "I²C master: передача записи", write),
        ("read", "I²C master: приём байтов", read),
        ("repeated-start", "I²C master: две фазы из одной очереди", repeated_start),
        ("buffers", "I²C master: приём и публикация результата", buffers),
        ("memory-format", "I²C master: формат результата в BRAM", memory_format),
        ("timing", "I²C master: формирование SCL", timing),
        ("sampling", "I²C master: изменение и выборка SDA", sampling),
        ("clock-stretching", "I²C master: ожидание SCL в READ_ACK", clock_stretching),
        ("completion", "I²C master: признаки готовности", completion),
    ):
        p = page_class(sheet(name), title, "", sources)
        draw(CoreDrawing(p, element, palette, arrows))
