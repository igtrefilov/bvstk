"""I2C master: commands, bus execution and the result path in the packaged RTL."""
from i2c_core import CoreDrawing
from i2c_protocol import ack, address, condition, field, omitted

BASE = "i2c/cores/master/"
RTL = "../hw_platform/fpga/ip_repo/i2c_master/src/"
DRIVER = "src/drivers/pl/i2c/bvstk_i2c_master.c"
SERVICE = "src/services/i2c/bvstk_i2c_master_service.c"


def sheet(name, fragment=""):
    return BASE+name+".svg"+("#"+fragment if fragment else "")


class MasterDrawing(CoreDrawing):
    def actors(self, y=650, receive=False):
        p = self.p
        if receive:
            p.frame("core", 390, y-85, 1130, 310, "I2C_master_0", target=sheet("structure", "core"))
            specs = [
                ("ps", 60, 290, "Драйвер PS", "Запуск и опрос\nДоступ через L1", "L3", sheet("commands", "driver"), None),
                ("tx", 420, 320, "Команды", "regs, TX FIFO\ntx_data_ctrl", "PL", sheet("commands", "core"), "core"),
                ("engine", 800, 320, "i2c_master_engine", "Состояния шины", "PL", sheet("structure", "engine"), "core"),
                ("rx", 1180, 300, "Приём", "rx_data_ctrl\nRX FIFO\nrx_fifo_mux", "PL", sheet("buffers", "receive"), "core"),
                ("bram", 1580, 250, "BRAM I²C", "Результат чтения", "PL", sheet("buffers", "bram"), None),
                ("device", 1910, 230, "Устройство", "Внешний ведомый", "L0", "i2c/protocol/interface.svg#target", None),
            ]
        else:
            p.frame("core", 490, y-85, 1120, 310, "I2C_master_0", target=sheet("structure", "core"))
            specs = [
                ("ps", 60, 360, "Драйвер PS", "Запуск и опрос\nДоступ через L1", "L3", sheet("commands", "driver"), None),
                ("tx", 550, 440, "Команды", "regs, TX FIFO\ntx_data_ctrl", "PL", sheet("commands", "core"), "core"),
                ("engine", 1170, 380, "i2c_master_engine", "Состояния шины", "PL", sheet("structure", "engine"), "core"),
                ("device", 1770, 370, "Ведомое устройство", "SDA и SCL", "L0", "i2c/protocol/interface.svg#target", None),
            ]
        short_names = {"ps": "PS", "tx": "Команды", "engine": "i2c_master_engine",
                       "rx": "Приём", "bram": "BRAM I²C", "device": "Устройство"}
        self.centers, self.actor_labels = {}, {}
        for key, x, width, title, body, layer, target, parent in specs:
            p.node(key, x, y, width, 200, title, body, layer, parent=parent, target=target)
            self.centers[key] = x+width/2
            self.actor_labels[key] = short_names[key], layer
        return y+350

    def bits(self, key, y, fields, x=100, gap=12):
        group = self.el(self.p.blocks, "g", id=key, data_kind="word-layout", data_layer="L0", data_domain="pl")
        for name, width, description in fields:
            self.el(group, "rect", x=x, y=y, width=width, height=125, rx=5,
                    fill="white", stroke=self.data, stroke_width=2)
            self.p.text(group, x+width/2, y+43, name, 25, bold=True, anchor="middle", color=self.data)
            self.p.text(group, x+width/2, y+91, description, 19, anchor="middle")
            x += width+gap
        if x-gap > 2140:
            raise ValueError(f"Word layout exceeds page: {self.p.name}#{key}")
        self.p.max_y = max(self.p.max_y, y+125)


def structure(d):
    p = d.p
    p.node("ps", 100, 310, 520, 220, "Сервис и драйвер PS",
           "bvstk_i2c_master_service\nbvstk_i2c_master_hw_transfer", "L3", target=sheet("commands", "ps"))
    p.node("platform", 820, 310, 400, 220, "Доступ PS", "MMIO", "L1",
           target="@architecture_bvstk_l1.svg#platform-access")
    p.node("irq-pin", 1550, 310, 510, 220, "IRQ → GIC",
           "PS ожидает опросом регистров", "L0", target=sheet("completion", "status"))
    p.edge("ps", "platform", "call")

    p.frame("core", 100, 670, 2000, 1630, "I2C_master_0", target="@architecture_bvstk_ps_pl.svg#i2c-master")
    p.node("axi-s", 140, 810, 320, 190, "axi_lite_slave_native", "Порт s_axi", parent="core")
    p.node("regs", 590, 810, 350, 190, "regs", "CSR, IRQ, TIMEOUT\nВход TX_FIFO", parent="core", target=sheet("completion", "status"))
    p.node("tx-fifo", 1070, 810, 360, 190, "TX FIFO", "xpm_fifo_sync\nКоманды и данные", parent="core", target=sheet("commands", "fifo"))
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
           "Адрес, байты и ACK\nSTART, RESTART и STOP", parent="engine", target=sheet("write", "address"))
    p.node("clock", 1710, 1350, 280, 180, "Такты SCL", "div_cnt, divisor\nscl_oe", parent="engine", target=sheet("timing", "clock"))
    p.node("inputs", 1710, 1625, 280, 180, "Входы", "debouncer → sync\nSDA и SCL", parent="engine", target=sheet("timing", "sampling"))
    p.edge("clock", "fsm", "clock", start="W", end="E", b_offset=-137.5)
    p.edge("inputs", "fsm", "data", start="W", end="E", b_offset=137.5)
    p.edge("tx-data-ctrl", "engine", "data", "tx_valid и tx_ready", start="S", end="N", b_offset=210,
           label_xy=(1940, 1140))
    p.edge("engine", "regs", "irq", "ERR_ACK", start="N", end="S", a_offset=-380,
           via=((1220, 1110), (765, 1110)), label_xy=(990, 1088))

    p.node("rx-data-ctrl", 690, 1350, 340, 180, "rx_data_ctrl", "rx_valid и rx_ready", parent="core", target=sheet("buffers", "rx-data-ctrl"))
    p.node("rx-fifo", 690, 1660, 340, 180, "RX FIFO", "xpm_fifo_sync\nПакеты байтов", parent="core", target=sheet("buffers", "rx-fifo"))
    p.node("rx-fifo-mux", 140, 1660, 370, 180, "rx_fifo_mux", "Пакеты → слова BRAM", parent="core", target=sheet("buffers", "writer"))
    p.node("axi-m", 140, 2050, 370, 180, "axi_lite_master_native", "m_axi: запись в BRAM", parent="core")
    p.edge("engine", "rx-data-ctrl", "data", start="W", end="E", a_offset=-140)
    p.edge("rx-data-ctrl", "rx-fifo", "data")
    p.edge("rx-fifo", "rx-fifo-mux", "data")
    p.edge("rx-fifo-mux", "axi-m", "data")
    p.edge("rx-fifo-mux", "regs", "irq", "DATA_VALID", start="N", end="S", b_offset=-120,
           via=((325, 1240), (645, 1240)), label_xy=(430, 1218))

    p.node("bram", 140, 2460, 620, 220, "BRAM I²C: результат master",
           "0x40000000\nЗаголовок и данные", target=sheet("buffers", "bram"))
    p.node("device", 1220, 2460, 760, 220, "Ведомое устройство", "Шина I²C", "L0",
           target="i2c/protocol/interface.svg#target")
    p.edge("axi-m", "bram", "data", start="S", end="N", b_offset=-125)
    p.edge("engine", "device", "data", "SDA и SCL", label_xy=(1725, 2195), bidirectional=True)
    p.edge("bram", "ps", "data", start="W", end="W",
           via=((40, 2570), (40, 420)))
    p.note(2810, "Команды → TX_FIFO. Результат чтения → BRAM через m_axi.")
    p.note(2872, "Модули упакованного IP. Политика и кэш — в сервисе L3.")
    d.finish(2975, "Далее: подготовка команды и запуск ядра", sheet("commands"))


def commands(d):
    p = d.p
    p.frame("ps", 60, 320, 630, 900, "Программа PS", "L3", target="ps-interaction/i2c-irq.svg#master-service")
    p.node("master-service", 100, 420, 550, 230, "bvstk_i2c_master_service",
           "Устройство и регистр\nПолитика операции", "L3", parent="ps",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_master_service")
    p.node("driver", 100, 790, 550, 330, "bvstk_i2c_master_hw_transfer",
           "Под mutex: ждать core_state = 0\nСброс IRQ → загрузка TX FIFO\nЗапуск: CSR.start", "L3", parent="ps", target=sheet("completion", "driver"))
    p.edge("master-service", "driver", "call")
    p.frame("core", 890, 320, 1250, 1380, "I2C_master_0: вход команды", target=sheet("structure", "core"))
    p.node("regs", 930, 420, 460, 220, "regs", "TX_FIFO: загрузка\nCSR.start: запуск", parent="core")
    p.node("fifo", 930, 850, 460, 230, "TX FIFO", "xpm_fifo_sync\nЗаголовки и данные записи", parent="core", target=sheet("repeated-start", "queue"))
    p.node("parser", 1570, 850, 520, 230, "tx_data_ctrl",
           "Заголовок → адрес\nСлова → пакеты байтов", parent="core", target=sheet("write", "bytes"))
    p.node("engine", 1570, 1380, 520, 220, "i2c_master_engine", "Приём: tx_valid и tx_ready\nОбмен на SDA и SCL", parent="core", target=sheet("write", "address"))
    p.edge("driver", "regs", "mmio", "MMIO через L1", start="E", end="W",
           via=((790, 955), (790, 530)), label_xy=(790, 732))
    p.edge("regs", "fifo", "data", "Записи TX_FIFO", label_xy=(1290, 760))
    p.edge("regs", "parser", "mmio", "CSR.start", start="E", end="N",
           via=((1470, 530), (1470, 720), (1830, 720)), label_xy=(1730, 697))
    p.edge("fifo", "parser", "data")
    p.edge("parser", "engine", "data", "packet_view_t", label_xy=(1960, 1250))

    d.heading("header", 1840, "Заголовок команды — header_t")
    d.bits("header-bits", 1915, [
        ("num_bytes", 870, "31…11: число байтов"), ("speed", 210, "Биты 10…9"),
        ("restart", 230, "Бит 8"), ("rw", 190, "Бит 7"), ("phy_addr", 440, "Биты 6…0"),
    ])
    p.note(2110, "rw: 0 — запись, 1 — чтение. restart = 1: продолжение со следующим заголовком.")
    p.note(2172, "Последний пакет фазы: packet.restart либо packet.stop. Число байтов задаёт num_bytes.")
    end = d.table("command-forms", 2300, [("Операция", 420, "L3"), ("Порядок слов в TX FIFO", 1620, "PL")], [
        ("Запись", "Заголовок W → слова данных; младший байт первым"),
        ("Чтение", "Заголовок R; длина — в num_bytes"),
        ("Запись, затем чтение", "Заголовок W с restart = 1 → данные → заголовок R"),
    ])
    p.note(end+70, "make_header: speed = 0, 100 кГц.")
    p.note(end+132, "TX FIFO: 64 слова. Загрузка до CSR.start, без ожидания свободного места.")
    d.finish(end+235, "Далее: выполнение записи на шине", sheet("write"))


def write(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(example=True), ack(), field("0x10", 160), ack(),
                          field("0x7B", 160), ack(), condition("P")])
    d.text(1440, 355, "Регистр 0x10 ← 0x7B", 22, width=650)
    start = d.actors()
    end = d.sequence(start, [
        ("launch", "ps", "tx", "mmio", "Загрузка команды W → CSR.start", "TX FIFO: 0x0000102A → 0x00007B10", sheet("commands", "header")),
        ("packet-address", "tx", "engine", "data", "PROCESSING_ADDR: адресный пакет", "packet.data = 0x54; приём при tx_valid и tx_ready", sheet("commands", "parser")),
        ("address", "engine", "device", "data", "START → SEND_ADDR → OP_CODE", "Адрес 0x2A + W; старший бит первым", sheet("timing", "sampling")),
        ("address-ack", "device", "engine", "data", "READ_ACK: ответ на адрес", "ACK: SDA = 0; NACK → ERR_ACK", sheet("completion", "outcomes")),
        ("bytes", "tx", "engine", "data", "WAIT_DATA → PROCESSING_DATA", "0x10 → 0x7B; у последнего пакета stop = 1", sheet("commands", "fifo")),
        ("send", "engine", "device", "data", "SEND_BYTE: передача байта", "SDA меняется по scl_ne; затем READ_ACK", sheet("timing", "sampling")),
        ("byte-ack", "device", "engine", "data", "ACK: продолжение", "После 0x10 — следующий байт; после 0x7B — STOP", None),
        ("stop", "engine", "device", "data", "STOP → TIMEOUT → IDLE", "TIMEOUT: выдержка условия STOP", sheet("timing", "boundaries")),
        ("poll", "ps", "tx", "mmio", "Опрос завершения", "core_state = 0, ERR_ACK отсутствует", sheet("completion", "driver")),
    ])
    p.note(end+50, "Передача байта и ACK повторяются по длине команды. NACK прерывает запись.")
    p.note(end+112, "Смысл 0x10 как указателя регистра задают сервис и устройство.")
    d.finish(end+215, "Далее: выполнение чтения и возврат результата", sheet("read"))


def read(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(True, example=True), ack(), field("D0", 170, "ведомый"),
                          ack("ведущий"), field("D1", 170, "ведомый"), ack("ведущий", True), condition("P")])
    d.text(1440, 355, "Команда PS: чтение 2 байтов.", 22, width=660)
    start = d.actors(receive=True)
    end = d.sequence(start, [
        ("launch", "ps", "tx", "mmio", "Загрузка команды R → CSR.start", "TX FIFO: 0x000010AA; адрес 0x2A, длина 2", sheet("commands", "header")),
        ("address", ("tx", "engine"), ("engine", "device"), "data", "PROCESSING_ADDR: START и адрес + R", "На шине: 0x55", sheet("write", "address")),
        ("address-ack", "device", "engine", "data", "READ_ACK: ACK адреса", "Следующее состояние — WAIT_PACKET", sheet("timing", "stretching")),
        ("request-byte", "tx", "engine", "data", "PROCESSING_DATA: запрос байта", "rw = 1; у последнего пакета stop = 1", sheet("commands", "parser")),
        ("receive-byte", "device", "engine", "data", "READ_BYTE: приём байта", "SDA по scl_pe → packet.data", sheet("timing", "sampling")),
        ("ack", "engine", "device", "data", "SEND_ACK: ACK после D0, NACK после D1", "stop или restart → NACK", None),
        ("capture", "engine", "rx", "data", "Принятый пакет → rx_data_ctrl", "rx_valid в SEND_ACK; приём при rx_ready", sheet("buffers", "receive")),
        ("stop", "engine", "device", "data", "STOP: завершение на шине", "Сохранение результата идёт параллельно", sheet("completion", "status")),
        ("publish", "rx", "bram", "data", "rx_fifo_mux: запись результата", "Слова → заголовок → DATA_VALID", sheet("buffers", "publish")),
        ("wait-result", "ps", "tx", "mmio", "Ожидание готовности", "core_state = 0, свежий DATA_VALID, без ERR_ACK", sheet("completion", "driver")),
        ("consume", "bram", "ps", "data", "Чтение результата через L1", "BRAM + 4: извлечь 2 байта", sheet("buffers", "bram")),
    ])
    p.note(end+50, "IDLE ядра и DATA_VALID в BRAM — отдельные события.")
    p.note(end+112, "Чтение регистра: сначала запись указателя, затем повторный START.")
    d.finish(end+215, "Запись указателя и чтение с повторным START", sheet("repeated-start"))


def repeated_start(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(width=195, example=True), ack(),
                          field("0x10", 130, detail="указатель"), ack(), condition("Sr"),
                          address(True, width=195, example=True), ack(), field("D0", 160, "ведомый"),
                          ack("ведущий", True), condition("P")], x=80, gap=10)
    d.text(1720, 355, "read_reg: 1 байт", 22, width=420)
    p.frame("ps", 60, 630, 650, 900, "Вызов из программы PS", "L3", target="ps-interaction/i2c-irq.svg#service")
    p.node("slave-service", 100, 750, 570, 220, "bvstk_i2c_slave_service",
           "Запрос нового значения", "L3", parent="ps",
           target="i2c/cores/slave/repeated-start.svg#pointer")
    p.node("master-service", 100, 1170, 570, 260, "bvstk_i2c_master_service",
           "backend_read → hw_read_reg\nРегистр 0x10, один байт", "L3", parent="ps", target=sheet("commands", "master-service"))
    p.edge("slave-service", "master-service", "call", "Нужно новое значение", label_xy=(480, 1070))
    p.frame("queue", 970, 630, 1180, 840, "TX FIFO: один запуск", target=sheet("commands", "fifo"))
    p.node("write-header", 1010, 740, 1100, 150, "0x0000092A — заголовок записи",
           "Адрес 0x2A, rw = 0, длина 1, restart = 1", parent="queue", target=sheet("commands", "header"))
    p.node("pointer-word", 1010, 1000, 1100, 150, "0x00000010 — слово данных",
           "7…0: указатель 0x10", parent="queue")
    p.node("read-header", 1010, 1260, 1100, 150, "0x000008AA — заголовок чтения",
           "Адрес 0x2A, rw = 1, длина 1, restart = 0", parent="queue", target=sheet("commands", "header"))
    p.edge("write-header", "pointer-word", "data")
    p.edge("pointer-word", "read-header", "data")
    p.edge("master-service", "queue", "mmio", "Заполнить через L1", start="E", end="W", b_offset=250,
           label_xy=(820, 1265))
    p.node("parser", 1010, 1670, 1100, 240, "tx_data_ctrl",
           "Запись 0x10: restart = 1 → WAIT_HEADER\nЧтение D0: stop = 1", target=sheet("commands", "parser"))
    p.edge("queue", "parser", "data", "После CSR.start", label_xy=(1700, 1590))

    p.frame("engine", 60, 2110, 2080, 500, "i2c_master_engine: последовательные фазы", target=sheet("structure", "engine"))
    p.node("write-phase", 100, 2230, 600, 290, "Запись указателя",
           "0x54 → ACK → 0x10 → ACK\npacket.restart = 1", parent="engine", target=sheet("write", "bytes"))
    p.node("restart", 850, 2230, 470, 290, "Повторный START",
           "RESTART → TIMEOUT\nWAIT_PACKET → SEND_ADDR", parent="engine", target=sheet("timing", "boundaries"))
    p.node("read-phase", 1500, 2230, 600, 290, "Чтение значения",
           "0x55 → ACK → D0 → NACK\nSTOP → TIMEOUT → IDLE", parent="engine", target=sheet("read", "receive-byte"))
    p.edge("parser", "engine", "data", start="S", end="N", b_offset=460)
    p.edge("write-phase", "restart", "data")
    p.edge("restart", "read-phase", "data")

    p.node("result", 1500, 2820, 600, 220, "RX FIFO → BRAM",
           "D0 → rx_data_ctrl → rx_fifo_mux\nЗапись → DATA_VALID", target=sheet("buffers", "receive"))
    p.node("master-service-result", 100, 2820, 730, 220, "bvstk_i2c_master_service",
           "D0 из BRAM → кэш\nВозврат значения", "L3",
           target="ps-interaction/i2c-irq.svg#master-service")
    p.edge("read-phase", "result", "data")
    p.edge("result", "master-service-result", "data", "После DATA_VALID", label_xy=(1150, 2905))
    p.note(3190, "restart задаётся в предыдущей фазе и действует после её последнего байта.")
    p.note(3252, "Указатель регистра интерпретируют сервисы; ядро исполняет очередь байтов.")
    d.finish(3355, "Как принятые байты попадают в BRAM", sheet("buffers"))


def buffers(d):
    p = d.p
    p.frame("receive", 60, 330, 2080, 470, "I2C_master_0: приёмные модули", target=sheet("structure", "rx-data-ctrl"))
    for key, x, width, title, body, target in (
        ("engine", 100, 460-100, "i2c_master_engine", "SEND_ACK → rx_valid", sheet("read", "capture")),
        ("rx-data-ctrl", 650, 390, "rx_data_ctrl", "packet_view_t при rx_ready", sheet("structure", "rx-data-ctrl")),
        ("rx-fifo", 1220, 320, "RX FIFO", "xpm_fifo_sync\nПакет: 32 бита", sheet("structure", "rx-fifo")),
        ("mux-in", 1700, 390, "rx_fifo_mux", "Пакеты → data4bytes", sheet("structure", "rx-fifo-mux")),
    ):
        p.node(key, x, 450, width, 220, title, body, parent="receive", target=target)
    p.edge("engine", "rx-data-ctrl", "data")
    p.edge("rx-data-ctrl", "rx-fifo", "data")
    p.edge("rx-fifo", "mux-in", "data")

    p.frame("writer", 610, 990, 540, 800, "rx_fifo_mux", subtitle="Запись и публикация",
            target=sheet("structure", "rx-fifo-mux"))
    p.node("axi-m", 650, 1090, 460, 220, "Запись через m_axi",
           "SEND_BYTES → SEND_HEADER\naxi_lite_master_native", parent="writer")
    p.node("publish", 650, 1520, 460, 210, "Готовность результата",
           "wr_axil_ready → DATA_VALID\nstop → FINAL_PACKET", parent="writer", target=sheet("completion", "status"))
    p.edge("mux-in", "axi-m", "data", start="S", end="N",
           via=((1895, 880), (880, 880)))
    p.edge("axi-m", "publish", "irq", "Ответ на запись заголовка", label_xy=(880, 1425))
    p.frame("bram", 1420, 990, 720, 800, "BRAM I²C: результат master",
            subtitle="База 0x40000000", target="reference/memory-map.svg#i2c-bank")
    p.node("header", 1460, 1120, 640, 210, "+0x0000: заголовок",
           "Адрес и длина\nЗапись после данных", parent="bram")
    p.node("data", 1460, 1510, 640, 220, "+0x0004: слова данных",
           "4 байта в слове\nВ конце возможен хвост", parent="bram", target=sheet("read", "consume"))
    p.edge("axi-m", "bram", "data", start="E", end="W", b_offset=-190)

    p.node("regs", 650, 2020, 460, 220, "regs: CSR и IRQ",
           "core_state и DATA_VALID", target=sheet("completion", "status"))
    p.node("ps", 1500, 2020, 640, 220, "Драйвер PS",
           "После готовности: BRAM + 4\nРаспаковка байтов", "L3", target=sheet("completion", "driver"))
    p.edge("publish", "regs", "irq")
    p.edge("ps", "regs", "mmio", "Опрос через L1", label_xy=(1300, 2100))
    p.edge("bram", "ps", "data", "Чтение через L1", start="S", end="N", a_offset=40,
           label_xy=(1955, 1920))

    d.heading("formats", 2410, "Форматы RX FIFO и BRAM")
    end = d.table("packet-formats", 2520, [("Место", 470, "PL"), ("Что хранится", 1570, "PL")], [
        ("RX FIFO", "packet_view_t, 32 бита: restart, stop, speed, rw, data, phy_addr"),
        ("BRAM: заголовок", "31…29: нули; 28…7: byte_cnt; 6…0: phy_addr"),
        ("BRAM: данные", "Четыре поля data в слове; B0 в битах 7…0"),
    ])
    d.heading("packing", end+105, "Пример пяти байтов: 12 34 56 78 9A", "Значения шестнадцатеричные. Адрес устройства — 0x2A.")
    bottom = d.table("example", end+225, [("Смещение", 430, "PL"), ("Слово", 480, "PL"), ("Смысл", 1130, "PL")], [
        ("+0x0000", "0x000002AA", "Длина 5, адрес 0x2A"),
        ("+0x0004", "0x78563412", "Четыре байта данных"),
        ("+0x0008", "0x0000009A", "Хвост: один полезный байт"),
    ])
    p.note(bottom+70, "WR_MEM_OFFSET = 0. stop завершает фрагмент; полное окно даёт промежуточный фрагмент.")
    p.note(bottom+132, "Окно 0x1000 байтов: заголовок 4 байта, данные до 4092. Это предел чтения драйвера.")
    d.finish(bottom+235, "Когда PS считает операцию завершённой", sheet("completion"))


def timing(d):
    p = d.p
    p.frame("engine", 60, 320, 1540, 940, "i2c_master_engine", target=sheet("structure", "engine"))
    p.node("speed", 100, 460, 360, 250, "packet.speed",
           "00: 100 кГц\n01: 400 кГц\n10: 1 МГц\n11: 1,5 МГц", parent="engine", target=sheet("commands", "header"))
    p.node("clock", 600, 460, 400, 250, "div_cnt и divisor",
           "divisor выбирается в START\nПолупериод: divisor тактов", parent="engine")
    p.node("output", 1140, 460, 400, 250, "Выход SCL",
           "scl_oe = 1: тянуть в 0\nscl_oe = 0: отпустить", parent="engine")
    p.node("scl", 1750, 460, 390, 250, "Линия SCL",
           "Подтяжка к 1\nВедомый может удерживать 0", "L0", target="i2c/protocol/interface.svg#clock-stretching")
    p.edge("speed", "clock", "mmio")
    p.edge("clock", "output", "clock")
    p.edge("output", "scl", "clock")
    p.node("inputs", 1140, 920, 400, 220, "debouncer и sync",
           "SCL: scl_pe и scl_ne\nSDA: уровень", parent="engine")
    p.node("fsm", 100, 920, 900, 220, "Автомат I²C",
           "READ_ACK: ожидание SCL ↑\nREAD_BYTE: выборка SDA по scl_pe", parent="engine")
    p.edge("scl", "inputs", "data", "scl_i", start="S", end="N",
           via=((1945, 810), (1340, 810)), label_xy=(1840, 787))
    p.edge("inputs", "fsm", "data")
    p.edge("fsm", "clock", "mmio", "Состояние", start="N", end="S",
           via=((550, 810), (800, 810)), label_xy=(675, 787))
    p.note(1340, "IDLE и TIMEOUT: SCL отпущена. READ_ACK: отдельное управление SCL.")

    d.heading("sampling", 1450, "Изменение SDA и выборка бита", "Масштаб условный; задержки debouncer и sync не показаны.")
    x = 300
    for xx, label in ((120, "scl_ne"), (360, "scl_pe"), (600, "scl_ne"), (840, "scl_pe"), (1080, "scl_ne"), (1320, "scl_pe")):
        d.line(((x+xx, 1570), (x+xx, 1840)), color="#ced6dd", width=1.4, dash="5 7")
        d.text(x+xx, 1550, label, 18, anchor="middle")
    d.trace("SCL", x, 1600, [(0, 1), (120, 1), (120, 0), (360, 0), (360, 1),
                            (600, 1), (600, 0), (840, 0), (840, 1), (1080, 1),
                            (1080, 0), (1320, 0), (1320, 1), (1500, 1)])
    d.trace("SDA", x, 1760, [(0, 1), (160, 1), (160, 0), (640, 0), (640, 1),
                            (1120, 1), (1120, 0), (1500, 0)], color=d.data)
    for xx, value in ((480, "0"), (960, "1"), (1440, "0")):
        d.text(x+xx, 1885, value, 25, bold=True, anchor="middle", color=d.data)
    p.note(1960, "Передача — по scl_ne, приём — по scl_pe. Старший бит первым.")

    d.heading("stretching", 2080, "READ_ACK: дождаться фактического подъёма SCL")
    d.el(p.under, "rect", x=580, y=2150, width=540, height=370, fill=d.fill)
    d.trace("scl_oe", 320, 2210, [(0, 1), (260, 1), (260, 0), (1300, 0)], color=d.palette["PL"][1])
    d.trace("SCL", 320, 2400, [(0, 0), (800, 0), (800, 1), (1300, 1)])
    d.line(((580, 2150), (580, 2520)), width=1.4, dash="5 7")
    d.line(((1120, 2150), (1120, 2520)), width=1.4, dash="5 7")
    d.text(580, 2565, "Ядро отпустило SCL", 19, anchor="middle")
    d.text(1120, 2610, "Ведомый отпустил SCL", 19, anchor="middle")
    d.text(1730, 2220, "На scl_pe:\nSDA = 0 → ACK\nSDA = 1 → NACK", 23, width=390)
    p.note(2700, "READ_ACK: выдержка SCL = 0 → отпускание → ожидание scl_pe.")
    p.note(2762, "Переполнение stretch_cnt → ERR_ACK. Выход из READ_ACK по scl_pe либо сбросу ядра.")

    d.heading("boundaries", 2890, "Границы фаз и разные виды ожидания")
    bottom = d.table("waits", 3000, [("Место в RTL", 535, "PL"), ("Что происходит", 1505, "PL")], [
        ("START", "SDA = 0 → снижение SCL → SEND_ADDR. Выбор divisor по speed."),
        ("STOP → TIMEOUT", "SCL = 1: выдержка divisor → отпустить SDA → IDLE"),
        ("RESTART → TIMEOUT", "SDA отпущена → выдержка → SDA = 0 → WAIT_PACKET"),
        ("WAIT_PACKET", "Ожидание tx_valid и tx_ready; div_cnt продолжает счёт"),
        ("Регистр TIMEOUT", "Выдержка tx_data_ctrl: COMPLETE → WAIT_START.\nОтдельна от состояния TIMEOUT в engine."),
    ])
    p.note(bottom+70, "Драйвер: speed = 0. Разрядность stretch_cnt задаёт STRETCH_TIMEOUT_MS.")
    d.finish(bottom+175, "Ошибки ожидания и действия программы PS", sheet("completion"))


def completion(d):
    p = d.p
    p.frame("status", 60, 320, 1730, 850, "Аппаратные признаки", target=sheet("structure", "regs"))
    p.node("engine", 100, 440, 530, 230, "i2c_master_engine",
           "core_state = 1 вне IDLE\nERR_ACK: NACK или тайм-аут SCL", parent="status", target=sheet("timing", "stretching"))
    p.node("writer", 100, 840, 530, 230, "rx_fifo_mux",
           "Данные → заголовок\nПодтверждение AXI → DATA_VALID", parent="status", target=sheet("buffers", "publish"))
    p.node("regs", 1000, 620, 700, 300, "regs: CSR и IRQ",
           "CSR: core_state\nIRQ: DATA_VALID, ERR_ACK\nFINAL_PACKET и состояние FIFO", parent="status")
    p.edge("engine", "regs", "irq", "ERR_ACK", start="E", end="N", a_offset=-40,
           via=((790, 515), (1350, 515)), label_xy=(1120, 490))
    p.edge("engine", "regs", "mmio", "core_state", start="E", end="W", a_offset=60, b_offset=30,
           via=((900, 615), (900, 800)), label_xy=(805, 590))
    p.edge("writer", "regs", "irq", "DATA_VALID", start="E", end="S",
           via=((790, 955), (790, 1100), (1350, 1100)), label_xy=(1130, 1075))
    p.node("irq", 1860, 675, 280, 190, "IRQ → GIC",
           "PS: опрос регистров", "L0", target="@architecture_bvstk_ps_pl.svg#i2c-master")
    p.edge("regs", "irq", "irq")

    p.frame("driver", 60, 1370, 2080, 1040, "Драйвер PS: wait_completion", "L3",
            subtitle="Весь вызов под mutex", target=sheet("commands", "driver"))
    p.node("prepare", 100, 1490, 530, 220, "Подготовить и запустить",
           "Свободное ядро → сброс IRQ\nКоманда → CSR.start", "L3", parent="driver", target=sheet("commands", "driver"))
    p.node("poll", 850, 1490, 550, 220, "Опрос CSR и IRQ",
           "ERR_ACK и готовность\nТайм-аут по часам PS", "L3", parent="driver")
    p.node("success", 1610, 1490, 480, 220, "Успешное завершение",
           "core_state = 0\nПри чтении: свежий DATA_VALID", "L3", parent="driver")
    p.node("recover", 850, 1910, 550, 290, "Сброс ядра",
           "CSR.soft_reset → 10 мс\nДо трёх попыток; NACK без повтора", "L3", parent="driver")
    p.node("return", 1610, 1925, 480, 260, "Завершить вызов",
           "Успех чтения: данные из BRAM\nОсвободить mutex; вернуть статус", "L3", parent="driver", target=sheet("buffers", "ps"))
    p.edge("prepare", "poll", "call")
    p.edge("poll", "success", "call")
    p.edge("success", "return", "call")
    p.edge("poll", "recover", "call", "Ошибка или истёк срок", label_xy=(1280, 1830))
    p.edge("recover", "return", "call", "NACK\nили лимит попыток", label_xy=(1505, 2010))
    p.edge("recover", "prepare", "call", "Повтор после других ошибок", start="S", end="S",
           via=((1125, 2320), (365, 2320)), label_xy=(650, 2295))
    p.edge("poll", "regs", "mmio", "Чтение через L1", start="N", end="S", a_offset=160, b_offset=200,
           via=((1285, 1270), (1550, 1270)), label_xy=(1450, 1245))

    d.heading("outcomes", 2550, "Как различаются окончания и ошибки")
    bottom = d.table("outcome-table", 2660, [("Событие", 550, "L0"), ("Поведение PL", 745, "PL"), ("Действие драйвера PS", 745, "L3")], [
        ("Запись завершена", "core_state = 0\nDATA_VALID не требуется", "Завершить при отсутствии ERR_ACK"),
        ("Ядро свободно, BRAM ещё записывается", "IDLE без свежего DATA_VALID", "Продолжать опрос"),
        ("NACK master в конце чтения", "SEND_ACK по stop или restart\nШтатное завершение", "Ждать core_state = 0 и DATA_VALID"),
        ("NACK ведомого", "READ_ACK → ERR_ACK → STOP", "Сброс → ошибка без повтора"),
        ("Тайм-аут SCL в READ_ACK", "ERR_ACK; ожидание SCL может продолжаться", "Сброс → ошибка без повтора"),
        ("Тайм-аут драйвера", "Условие готовности не выполнено", "Сброс → повтор в пределах лимита"),
        ("TX FIFO или RX FIFO заполнен", "full → IRQ", "Отдельной обработки full нет"),
    ])
    p.note(bottom+70, "DATA_VALID и ERR_ACK — до сброса IRQ. FINAL_PACKET и FIFO — текущие сигналы.")
    p.note(bottom+132, "Тайм-аут драйвера: 100 мс по умолчанию. Ошибка MMIO также ведёт к восстановлению.")
    d.finish(bottom+235, "Вернуться к составу ведущего ядра", sheet("structure"))


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
    for name, title, subtitle, draw in (
        ("structure", "I²C master: состав ядра", "Команды PS → шина I²C → результат в BRAM.", structure),
        ("commands", "I²C master: подготовка команды", "Сервис → драйвер → TX FIFO → engine.", commands),
        ("write", "I²C master: выполнение записи", "От команды PS до STOP. Шаги сверху вниз.", write),
        ("read", "I²C master: выполнение чтения", "От команды PS до результата в BRAM.", read),
        ("repeated-start", "I²C master: запись указателя и чтение", "Две фазы, одна очередь, один запуск.", repeated_start),
        ("buffers", "I²C master: приёмные буферы и BRAM", "Пакет → слово → готовый результат.", buffers),
        ("timing", "I²C master: управление SDA и SCL", "Делитель, выборка битов и ожидания. Масштаб условный.", timing),
        ("completion", "I²C master: завершение и восстановление", "Признаки PL → опрос PS → результат или повтор.", completion),
    ):
        p = page_class(sheet(name), title, subtitle, sources)
        draw(MasterDrawing(p, element, palette, arrows))
