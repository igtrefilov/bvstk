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
           "bvstk_i2c_master_service\nbvstk_i2c_master_hw_transfer\nЧитает результат через L1", "L3", target=sheet("commands", "ps"))
    p.node("platform", 820, 310, 400, 220, "Доступ PS", "MMIO\nЧтение и запись регистров", "L1",
           target="@architecture_bvstk_l1.svg#platform-access")
    p.node("irq-pin", 1550, 310, 510, 220, "IRQ → GIC",
           "Аппаратная линия подключена\nДрайвер опрашивает CSR и IRQ", "L0", target=sheet("completion", "status"))
    p.edge("ps", "platform", "call")

    p.frame("core", 100, 670, 2000, 1630, "I2C_master_0", target="@architecture_bvstk_ps_pl.svg#i2c-master")
    p.node("axi-s", 140, 810, 320, 190, "axi_lite_slave_native", "Порт s_axi", parent="core")
    p.node("regs", 590, 810, 350, 190, "regs", "CSR, IRQ, TIMEOUT\nВход TX_FIFO", parent="core", target=sheet("completion", "status"))
    p.node("tx-fifo", 1070, 810, 360, 190, "TX FIFO", "xpm_fifo_sync\nЗаголовки и слова данных", parent="core", target=sheet("commands", "fifo"))
    p.node("tx-data-ctrl", 1590, 810, 440, 190, "tx_data_ctrl", "Разбор заголовка\nВыдача packet_view_t", parent="core", target=sheet("commands", "parser"))
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
           "packet\nSTART, SEND_ADDR\nSEND_BYTE, READ_BYTE\nREAD_ACK, SEND_ACK\nRESTART, STOP", parent="engine", target=sheet("write", "address"))
    p.node("clock", 1710, 1350, 280, 180, "Такты SCL", "div_cnt и divisor\nВыход scl_oe", parent="engine", target=sheet("timing", "clock"))
    p.node("inputs", 1710, 1625, 280, 180, "Входы", "debouncer, sync\nSCL: фронты\nSDA: уровень", parent="engine", target=sheet("timing", "sampling"))
    p.edge("clock", "fsm", "clock", start="W", end="E", b_offset=-137.5)
    p.edge("inputs", "fsm", "data", start="W", end="E", b_offset=137.5)
    p.edge("tx-data-ctrl", "engine", "data", "tx_valid и tx_ready", start="S", end="N", b_offset=210,
           label_xy=(1940, 1140))
    p.edge("engine", "regs", "irq", "ERR_ACK", start="N", end="S", a_offset=-380,
           via=((1220, 1110), (765, 1110)), label_xy=(990, 1088))

    p.node("rx-data-ctrl", 690, 1350, 340, 180, "rx_data_ctrl", "rx_valid и rx_ready\nПередача пакета в FIFO", parent="core", target=sheet("buffers", "rx-data-ctrl"))
    p.node("rx-fifo", 690, 1660, 340, 180, "RX FIFO", "xpm_fifo_sync\nПакеты принятых байтов", parent="core", target=sheet("buffers", "rx-fifo"))
    p.node("rx-fifo-mux", 140, 1660, 370, 180, "rx_fifo_mux", "Собрать слова\nЗаписать заголовок", parent="core", target=sheet("buffers", "writer"))
    p.node("axi-m", 140, 2050, 370, 180, "axi_lite_master_native", "Порт m_axi\nЗапись результата в BRAM", parent="core")
    p.edge("engine", "rx-data-ctrl", "data", start="W", end="E", a_offset=-140)
    p.edge("rx-data-ctrl", "rx-fifo", "data")
    p.edge("rx-fifo", "rx-fifo-mux", "data")
    p.edge("rx-fifo-mux", "axi-m", "data")
    p.edge("rx-fifo-mux", "regs", "irq", "DATA_VALID", start="N", end="S", b_offset=-120,
           via=((325, 1240), (645, 1240)), label_xy=(430, 1218))

    p.node("bram", 140, 2460, 620, 220, "BRAM I²C: результат master",
           "База 0x40000000, смещение +0x0000\nЗаголовок и принятые байты", target=sheet("buffers", "bram"))
    p.node("device", 1220, 2460, 760, 220, "Ведомое устройство", "Физическая шина I²C\nАдрес, данные и подтверждения", "L0",
           target="i2c/protocol/interface.svg#target")
    p.edge("axi-m", "bram", "data", start="S", end="N", b_offset=-125)
    p.edge("engine", "device", "data", "SDA и SCL", label_xy=(1725, 2195), bidirectional=True)
    p.edge("bram", "ps", "data", start="W", end="W",
           via=((40, 2570), (40, 420)))
    p.note(2810, "Команды поступают через TX_FIFO. m_axi используется для записи результатов чтения в BRAM.")
    p.note(2872, "Состав показан по i2c_master_wrapper и top_i2c_master из упакованного IP. Политика и кэш обрабатываются сервисом L3.")
    d.finish(2975, "Далее: подготовка команды и запуск ядра", sheet("commands"))


def commands(d):
    p = d.p
    p.frame("ps", 60, 320, 630, 900, "Программа PS", "L3", target="ps-interaction/i2c-irq.svg#master-service")
    p.node("master-service", 100, 420, 550, 230, "bvstk_i2c_master_service",
           "Выбрать устройство и регистр\nПрименить правила операции\nВызвать аппаратный драйвер", "L3", parent="ps",
           target="@architecture_bvstk_l3.svg#service-bvstk_i2c_master_service")
    p.node("driver", 100, 790, 550, 330, "bvstk_i2c_master_hw_transfer",
           "Захватить mutex\nДождаться core_state = 0\nСбросить признаки IRQ\nЗаписать заголовки и данные\nПередать CSR.start", "L3", parent="ps", target=sheet("completion", "driver"))
    p.edge("master-service", "driver", "call")
    p.frame("core", 890, 320, 1250, 1380, "I2C_master_0: вход команды", target=sheet("structure", "core"))
    p.node("regs", 930, 420, 460, 220, "regs", "Записи TX_FIFO поступают в очередь\nCSR.start запускает разбор", parent="core")
    p.node("fifo", 930, 850, 460, 230, "TX FIFO", "xpm_fifo_sync\nЗаголовок, затем слова записи\nДля чтения достаточно заголовка", parent="core", target=sheet("repeated-start", "queue"))
    p.node("parser", 1570, 850, 520, 230, "tx_data_ctrl",
           "WAIT_HEADER: прочитать header_t\nPROCESSING_ADDR: выдать адрес\nWAIT_DATA: взять слово записи", parent="core", target=sheet("write", "bytes"))
    p.node("engine", 1570, 1380, 520, 220, "i2c_master_engine", "packet_view_t\nПриём при tx_valid и tx_ready\nНачало обмена на SDA и SCL", parent="core", target=sheet("write", "address"))
    p.edge("driver", "regs", "mmio", "MMIO через L1", start="E", end="W",
           via=((790, 955), (790, 530)), label_xy=(790, 732))
    p.edge("regs", "fifo", "data", "Записи TX_FIFO", label_xy=(1290, 760))
    p.edge("regs", "parser", "mmio", "CSR.start", start="E", end="N",
           via=((1470, 530), (1470, 720), (1830, 720)), label_xy=(1730, 697))
    p.edge("fifo", "parser", "data")
    p.edge("parser", "engine", "data", "packet_view_t", label_xy=(1960, 1250))

    d.heading("header", 1840, "Заголовок команды — header_t")
    d.bits("header-bits", 1915, [
        ("num_bytes", 870, "Биты 31…11: число байтов"), ("speed", 210, "Биты 10…9"),
        ("restart", 230, "Бит 8"), ("rw", 190, "Бит 7"), ("phy_addr", 440, "Биты 6…0"),
    ])
    p.note(2110, "rw = 0: запись; rw = 1: чтение. restart = 1 связывает эту фазу со следующим заголовком в FIFO.")
    p.note(2172, "В конце фазы tx_data_ctrl задаёт packet.restart либо packet.stop. Для чтения он выдаёт команды на нужное число байтов.")
    end = d.table("command-forms", 2300, [("Операция", 420, "L3"), ("Порядок слов в TX FIFO", 1620, "PL")], [
        ("Запись", "Заголовок W → слова данных. Байты каждого слова идут от младших разрядов к старшим."),
        ("Чтение", "Заголовок R. Длина в заголовке определяет число принимаемых байтов."),
        ("Запись, затем чтение", "Заголовок W с restart = 1 → слова записи → заголовок R. После всей очереди PS записывает CSR.start."),
    ])
    p.note(end+70, "Текущий make_header оставляет speed = 0: 100 кГц. Другие значения поля и работа делителя раскрыты на листе тактирования.")
    p.note(end+132, "В упакованном IP FIFO_DEPTH = 64 слова. Драйвер заполняет TX FIFO до запуска; ожидание свободного места в цикле записи не предусмотрено.")
    d.finish(end+235, "Далее: выполнение записи на шине", sheet("write"))


def write(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(example=True), ack(), field("0x10", 160), ack(),
                          field("0x7B", 160), ack(), condition("P")])
    d.text(1440, 355, "Пример: записать значение 0x7B\nпо указателю регистра 0x10.", 22, width=650)
    start = d.actors()
    end = d.sequence(start, [
        ("launch", "ps", "tx", "mmio", "Записать команду W, два байта и CSR.start", "TX FIFO: 0x0000102A, затем 0x00007B10. Длина 2, restart = 0, адрес 0x2A.", sheet("commands", "header")),
        ("packet-address", "tx", "engine", "data", "PROCESSING_ADDR: передать адресный пакет", "packet.data = 0x54. Передача внутри ядра подтверждается сочетанием tx_valid и tx_ready.", sheet("commands", "parser")),
        ("address", "engine", "device", "data", "START → SEND_ADDR → OP_CODE", "Ядро формирует START и передаёт адрес с битом W. На шину первым выходит старший бит.", sheet("timing", "sampling")),
        ("address-ack", "device", "engine", "data", "READ_ACK: проверить подтверждение адреса", "SDA = 0 на фронте SCL означает ACK. При NACK устанавливается ERR_ACK.", sheet("completion", "outcomes")),
        ("bytes", "tx", "engine", "data", "WAIT_DATA → PROCESSING_DATA: выдать очередной байт", "Из data4bytes сначала извлекается 0x10, затем 0x7B. У последнего пакета stop = 1.", sheet("commands", "fifo")),
        ("send", "engine", "device", "data", "SEND_BYTE: передать восемь бит", "Автомат задаёт SDA на спаде SCL. После байта переходит в READ_ACK и отпускает SDA.", sheet("timing", "sampling")),
        ("byte-ack", "device", "engine", "data", "ACK разрешает продолжить фазу", "После 0x10 повторяются выдача пакета и передача байта. После 0x7B признак stop ведёт к STOP.", None),
        ("stop", "engine", "device", "data", "STOP → TIMEOUT → IDLE", "Ядро завершает обмен. TIMEOUT здесь задаёт выдержку для условия STOP.", sheet("timing", "boundaries")),
        ("poll", "ps", "tx", "mmio", "Драйвер проверяет завершение записи", "Он опрашивает CSR и IRQ: core_state = 0 при отсутствии ERR_ACK завершает ожидание записи.", sheet("completion", "driver")),
    ])
    p.note(end+50, "Шаги передачи байта и приёма ACK повторяются по длине команды. NACK устройства прерывает успешный путь.")
    p.note(end+112, "Смысл 0x10 как указателя регистра задаёт сервис и ведомое устройство. Ядро передаёт заданные байты.")
    d.finish(end+215, "Далее: выполнение чтения и возврат результата", sheet("read"))


def read(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(True, example=True), ack(), field("D0", 170, "ведомый"),
                          ack("ведущий"), field("D1", 170, "ведомый"), ack("ведущий", True), condition("P")])
    d.text(1440, 355, "Пример: принять два байта.\nЧисло байтов задано командой PS.", 22, width=660)
    start = d.actors(receive=True)
    end = d.sequence(start, [
        ("launch", "ps", "tx", "mmio", "Поставить команду чтения и запустить разбор", "Заголовок 0x000010AA: адрес 0x2A, rw = 1, длина 2, restart = 0. Затем CSR.start.", sheet("commands", "header")),
        ("address", ("tx", "engine"), ("engine", "device"), "data", "PROCESSING_ADDR: START и адрес + R", "tx_data_ctrl передаёт адресный пакет; engine выдаёт на шину байт 0x55.", sheet("write", "address")),
        ("address-ack", "device", "engine", "data", "READ_ACK: устройство подтверждает адрес", "После ACK автомат ждёт следующий управляющий пакет в WAIT_PACKET.", sheet("timing", "stretching")),
        ("request-byte", "tx", "engine", "data", "PROCESSING_DATA: задать приём очередного байта", "Для каждого байта выдаётся packet_view_t с rw = 1. Последний пакет содержит stop = 1.", sheet("commands", "parser")),
        ("receive-byte", "device", "engine", "data", "READ_BYTE: собрать восемь бит", "Ядро формирует SCL и считывает SDA на фронтах scl_pe. Данные сохраняются в packet.data.", sheet("timing", "sampling")),
        ("ack", "engine", "device", "data", "SEND_ACK: ACK после D0, NACK после D1", "packet.stop или packet.restart заставляет ядро отпустить SDA на девятом такте.", None),
        ("capture", "engine", "rx", "data", "Передать принятый пакет в rx_data_ctrl", "На фронте SCL в SEND_ACK появляется rx_valid. rx_data_ctrl принимает пакет через rx_ready.", sheet("buffers", "receive")),
        ("stop", "engine", "device", "data", "Завершить фазу условием STOP", "Приёмная ветвь продолжает сохранять данные параллельно завершению обмена на шине.", sheet("completion", "status")),
        ("publish", "rx", "bram", "data", "rx_fifo_mux записывает слова и заголовок", "После завершения записи заголовка появляется DATA_VALID. Подробный путь показан на листе буферов.", sheet("buffers", "publish")),
        ("wait-result", "ps", "tx", "mmio", "Дождаться освобождения ядра и свежего DATA_VALID", "wait_completion проверяет core_state = 0, DATA_VALID и отсутствие ERR_ACK.", sheet("completion", "driver")),
        ("consume", "bram", "ps", "data", "Прочитать два байта результата через L1", "Драйвер читает слова начиная с базы BRAM + 4 и извлекает запрошенное число байтов.", sheet("buffers", "bram")),
    ])
    p.note(end+50, "Приёмные модули работают параллельно engine. Освобождение шины и готовность данных в BRAM — отдельные события.")
    p.note(end+112, "Для чтения регистра сервис сначала задаёт запись указателя. Этот составной обмен раскрыт на следующем листе.")
    d.finish(end+215, "Запись указателя и чтение с повторным START", sheet("repeated-start"))


def repeated_start(d):
    p = d.p
    d.strip("wire", 320, [condition("S"), address(width=195, example=True), ack(),
                          field("0x10", 130, detail="указатель"), ack(), condition("Sr"),
                          address(True, width=195, example=True), ack(), field("D0", 160, "ведомый"),
                          ack("ведущий", True), condition("P")], x=80, gap=10)
    d.text(1720, 355, "Пример read_reg:\nодин регистр, один байт.", 22, width=420)
    p.frame("ps", 60, 630, 650, 900, "Вызов из программы PS", "L3", target="ps-interaction/i2c-irq.svg#service")
    p.node("slave-service", 100, 750, 570, 220, "bvstk_i2c_slave_service",
           "Обработать внешний запрос\nПолучить новое значение регистра", "L3", parent="ps",
           target="i2c/cores/slave/repeated-start.svg#pointer")
    p.node("master-service", 100, 1170, 570, 260, "bvstk_i2c_master_service",
           "backend_read → hw_read_reg\nПередать указатель 0x10\nПолучить один байт\nКоманду загружает драйвер", "L3", parent="ps", target=sheet("commands", "master-service"))
    p.edge("slave-service", "master-service", "call", "Нужно новое значение", label_xy=(480, 1070))
    p.frame("queue", 970, 630, 1180, 840, "TX FIFO: один запуск", target=sheet("commands", "fifo"))
    p.node("write-header", 1010, 740, 1100, 150, "0x0000092A — заголовок записи",
           "Адрес 0x2A, rw = 0, длина 1, restart = 1", parent="queue", target=sheet("commands", "header"))
    p.node("pointer-word", 1010, 1000, 1100, 150, "0x00000010 — слово данных",
           "Указатель регистра находится в младших восьми битах", parent="queue")
    p.node("read-header", 1010, 1260, 1100, 150, "0x000008AA — заголовок чтения",
           "Адрес 0x2A, rw = 1, длина 1, restart = 0", parent="queue", target=sheet("commands", "header"))
    p.edge("write-header", "pointer-word", "data")
    p.edge("pointer-word", "read-header", "data")
    p.edge("master-service", "queue", "mmio", "Заполнить через L1", start="E", end="W", b_offset=250,
           label_xy=(820, 1265))
    p.node("parser", 1010, 1670, 1100, 240, "tx_data_ctrl",
           "Первый заголовок: адрес W и пакет байта 0x10 с restart = 1\nПосле этого WAIT_HEADER читает второй заголовок\nДля последнего байта чтения задаётся stop = 1", target=sheet("commands", "parser"))
    p.edge("queue", "parser", "data", "После CSR.start", label_xy=(1700, 1590))

    p.frame("engine", 60, 2110, 2080, 500, "i2c_master_engine: последовательные фазы", target=sheet("structure", "engine"))
    p.node("write-phase", 100, 2230, 600, 290, "Запись указателя",
           "SEND_ADDR: 0x54\nSEND_BYTE: 0x10\nREAD_ACK проверяет ACK\npacket.restart = 1", parent="engine", target=sheet("write", "bytes"))
    p.node("restart", 850, 2230, 470, 290, "Повторный START",
           "RESTART → TIMEOUT\nЗатем WAIT_PACKET\nПринять следующий адрес\nПерейти в SEND_ADDR", parent="engine", target=sheet("timing", "boundaries"))
    p.node("read-phase", 1500, 2230, 600, 290, "Чтение значения",
           "SEND_ADDR: 0x55\nREAD_BYTE: D0\nSEND_ACK: NACK\nSTOP → TIMEOUT → IDLE", parent="engine", target=sheet("read", "receive-byte"))
    p.edge("parser", "engine", "data", start="S", end="N", b_offset=460)
    p.edge("write-phase", "restart", "data")
    p.edge("restart", "read-phase", "data")

    p.node("result", 1500, 2820, 600, 220, "RX FIFO → BRAM",
           "rx_data_ctrl принимает D0\nrx_fifo_mux записывает результат\nПоявляется DATA_VALID", target=sheet("buffers", "receive"))
    p.node("master-service-result", 100, 2820, 730, 220, "bvstk_i2c_master_service",
           "Драйвер читает D0 из BRAM\nСервис обновляет кэш\nЗначение возвращается вызывающей стороне", "L3",
           target="ps-interaction/i2c-irq.svg#master-service")
    p.edge("read-phase", "result", "data")
    p.edge("result", "master-service-result", "data", "После DATA_VALID", label_xy=(1150, 2905))
    p.note(3190, "restart относится к предыдущей фазе. Передача её последнего байта приводит к повторному START и разбору следующего заголовка.")
    p.note(3252, "Сервисы сохраняют смысл указателя 0x10. Аппаратная очередь содержит заголовки и байты, заданные драйвером.")
    d.finish(3355, "Как принятые байты попадают в BRAM", sheet("buffers"))


def buffers(d):
    p = d.p
    p.frame("receive", 60, 330, 2080, 470, "I2C_master_0: приёмные модули", target=sheet("structure", "rx-data-ctrl"))
    for key, x, width, title, body, target in (
        ("engine", 100, 460-100, "i2c_master_engine", "SEND_ACK\nrx_valid с принятым байтом", sheet("read", "capture")),
        ("rx-data-ctrl", 650, 390, "rx_data_ctrl", "Принять при rx_ready\nЗаписать packet_view_t", sheet("structure", "rx-data-ctrl")),
        ("rx-fifo", 1220, 320, "RX FIFO", "xpm_fifo_sync\nОдно слово на пакет", sheet("structure", "rx-fifo")),
        ("mux-in", 1700, 390, "rx_fifo_mux", "Читать пакеты\nСобирать data4bytes", sheet("structure", "rx-fifo-mux")),
    ):
        p.node(key, x, 450, width, 220, title, body, parent="receive", target=target)
    p.edge("engine", "rx-data-ctrl", "data")
    p.edge("rx-data-ctrl", "rx-fifo", "data")
    p.edge("rx-fifo", "mux-in", "data")

    p.frame("writer", 610, 990, 540, 800, "rx_fifo_mux", subtitle="Запись и публикация",
            target=sheet("structure", "rx-fifo-mux"))
    p.node("axi-m", 650, 1090, 460, 220, "Запись через m_axi",
           "SEND_BYTES: слова данных\nSEND_HEADER: заголовок\nЧерез axi_lite_master_native", parent="writer")
    p.node("publish", 650, 1520, 460, 210, "Готовность результата",
           "FINISHED либо OVERFLOW\nwr_axil_ready → DATA_VALID\nstop задаёт FINAL_PACKET", parent="writer", target=sheet("completion", "status"))
    p.edge("mux-in", "axi-m", "data", start="S", end="N",
           via=((1895, 880), (880, 880)))
    p.edge("axi-m", "publish", "irq", "Ответ на запись заголовка", label_xy=(880, 1425))
    p.frame("bram", 1420, 990, 720, 800, "BRAM I²C: результат master",
            subtitle="База 0x40000000", target="reference/memory-map.svg#i2c-bank")
    p.node("header", 1460, 1120, 640, 210, "+0x0000: заголовок",
           "Адрес устройства и число байтов\nЗаписывается после данных", parent="bram")
    p.node("data", 1460, 1510, 640, 220, "+0x0004: слова данных",
           "Четыре байта в слове\nПоследнее слово может быть неполным", parent="bram", target=sheet("read", "consume"))
    p.edge("axi-m", "bram", "data", start="E", end="W", b_offset=-190)

    p.node("regs", 650, 2020, 460, 220, "regs: CSR и IRQ",
           "Фиксирует DATA_VALID\nПоказывает core_state\nPS опрашивает оба регистра", target=sheet("completion", "status"))
    p.node("ps", 1500, 2020, 640, 220, "Драйвер PS",
           "Дождаться завершения\nПрочитать данные начиная с +4\nРаспаковать нужное число байтов", "L3", target=sheet("completion", "driver"))
    p.edge("publish", "regs", "irq")
    p.edge("ps", "regs", "mmio", "Опрос через L1", label_xy=(1300, 2100))
    p.edge("bram", "ps", "data", "Чтение через L1", start="S", end="N", a_offset=40,
           label_xy=(1955, 1920))

    d.heading("formats", 2410, "Пакет в RX FIFO и результат в BRAM имеют разный формат")
    end = d.table("packet-formats", 2520, [("Место", 470, "PL"), ("Что хранится", 1570, "PL")], [
        ("RX FIFO", "packet_view_t: restart, stop, speed, rw, один байт data и phy_addr. Пакет занимает одно 32-битное слово FIFO."),
        ("BRAM: заголовок", "Биты 31…29 равны нулю; биты 28…7 содержат 22-битный счётчик byte_cnt; биты 6…0 — phy_addr."),
        ("BRAM: данные", "rx_fifo_mux извлекает data из пакетов. B0 занимает биты 7…0, затем B1, B2 и B3 в старших разрядах слова."),
    ])
    d.heading("packing", end+105, "Пример пяти байтов: 12 34 56 78 9A", "Значения шестнадцатеричные. Адрес устройства — 0x2A.")
    bottom = d.table("example", end+225, [("Смещение", 430, "PL"), ("Слово", 480, "PL"), ("Смысл", 1130, "PL")], [
        ("+0x0000", "0x000002AA", "Заголовок: число байтов 5, адрес 0x2A."),
        ("+0x0004", "0x78563412", "Первое полное слово из четырёх байтов."),
        ("+0x0008", "0x0000009A", "Хвост: один полезный байт. Остальные разряды не входят в длину."),
    ])
    p.note(bottom+70, "WR_MEM_OFFSET в этой конфигурации равен 0. При stop публикуется последний фрагмент, при заполнении окна — промежуточный.")
    p.note(bottom+132, "Окно занимает 0x1000 байтов: заголовок и до 4092 байтов данных. Драйвер ограничивает длину чтения этим размером.")
    d.finish(bottom+235, "Когда PS считает операцию завершённой", sheet("completion"))


def timing(d):
    p = d.p
    p.frame("engine", 60, 320, 1540, 940, "i2c_master_engine", target=sheet("structure", "engine"))
    p.node("speed", 100, 460, 360, 250, "packet.speed",
           "00: 100 кГц\n01: 400 кГц\n10: 1 МГц\n11: 1,5 МГц", parent="engine", target=sheet("commands", "header"))
    p.node("clock", 600, 460, 400, 250, "div_cnt и divisor",
           "Выбор divisor в START\nПереключение scl_oe\nчерез divisor тактов clk", parent="engine")
    p.node("output", 1140, 460, 400, 250, "Выход SCL",
           "scl_o всегда равен 0\nscl_oe = 1: тянуть в 0\nscl_oe = 0: отпустить", parent="engine")
    p.node("scl", 1750, 460, 390, 250, "Линия SCL",
           "Фактический уровень\nВедомый может удерживать 0\nПосле отпускания — подтяжка", "L0", target="i2c/protocol/interface.svg#clock-stretching")
    p.edge("speed", "clock", "mmio")
    p.edge("clock", "output", "clock")
    p.edge("output", "scl", "clock")
    p.node("inputs", 1140, 920, 400, 220, "debouncer и sync",
           "SCL: scl_pe и scl_ne\nSDA: фильтрованный уровень\nСобытия в домене clk", parent="engine")
    p.node("fsm", 100, 920, 900, 220, "Автомат использует входные фронты",
           "READ_ACK ждёт подъёма SCL\nREAD_BYTE считывает SDA на scl_pe\nСостояние автомата управляет генератором SCL", parent="engine")
    p.edge("scl", "inputs", "data", "scl_i", start="S", end="N",
           via=((1945, 810), (1340, 810)), label_xy=(1840, 787))
    p.edge("inputs", "fsm", "data")
    p.edge("fsm", "clock", "mmio", "Состояние", start="N", end="S",
           via=((550, 810), (800, 810)), label_xy=(675, 787))
    p.note(1340, "На обычных тактах делитель задаёт полупериод SCL. В IDLE и TIMEOUT ядро отпускает SCL; для READ_ACK действует отдельная ветвь.")

    d.heading("sampling", 1450, "Изменение SDA и выборка бита", "Масштаб условный. debouncer и sync добавляют задержку между линией и событием автомата.")
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
    p.note(1960, "SEND_ADDR и SEND_BYTE меняют SDA по scl_ne. READ_BYTE собирает входной байт по scl_pe, начиная со старшего бита.")

    d.heading("stretching", 2080, "READ_ACK: дождаться фактического подъёма SCL")
    d.el(p.under, "rect", x=580, y=2150, width=540, height=370, fill=d.fill)
    d.trace("scl_oe", 320, 2210, [(0, 1), (260, 1), (260, 0), (1300, 0)], color=d.palette["PL"][1])
    d.trace("SCL", 320, 2400, [(0, 0), (800, 0), (800, 1), (1300, 1)])
    d.line(((580, 2150), (580, 2520)), width=1.4, dash="5 7")
    d.line(((1120, 2150), (1120, 2520)), width=1.4, dash="5 7")
    d.text(580, 2565, "Ядро отпустило SCL", 19, anchor="middle")
    d.text(1120, 2610, "Ведомый отпустил SCL", 19, anchor="middle")
    d.text(1730, 2220, "Появился scl_pe:\nсчитать SDA.\n\n0 означает ACK.\n1 означает NACK.", 23, width=390)
    p.note(2700, "В начале READ_ACK ядро выдерживает низкий SCL. Затем отпускает линию и ждёт её подъёма; stretch_cnt продолжает счёт.")
    p.note(2762, "При заполнении stretch_cnt формируется ERR_ACK. Сам переход из READ_ACK остаётся привязан к scl_pe; драйвер может сбросить ядро.")

    d.heading("boundaries", 2890, "Границы фаз и разные виды ожидания")
    bottom = d.table("waits", 3000, [("Место в RTL", 535, "PL"), ("Что происходит", 1505, "PL")], [
        ("START", "Ядро тянет SDA в 0. После снижения SCL начинает SEND_ADDR. Здесь выбирается divisor по packet.speed."),
        ("STOP → TIMEOUT", "На высоком SCL выдерживается интервал divisor. Для пакета с stop = 1 SDA отпускается; автомат возвращается в IDLE."),
        ("RESTART → TIMEOUT", "SDA отпущена перед повторным START. После выдержки ядро тянет SDA в 0 и ждёт следующий адрес в WAIT_PACKET."),
        ("WAIT_PACKET", "Автомат ждёт tx_valid вместе с tx_ready. В этой ветви генератор SCL продолжает счёт div_cnt."),
        ("Регистр TIMEOUT", "Задаёт выдержку tx_data_ctrl в COMPLETE перед возвратом в WAIT_START. Это отдельный счётчик, не состояние TIMEOUT автомата шины."),
    ])
    p.note(bottom+70, "Текущий драйвер задаёт speed = 0. Порог stretch_cnt определяется разрядностью, рассчитанной из STRETCH_TIMEOUT_MS.")
    d.finish(bottom+175, "Ошибки ожидания и действия программы PS", sheet("completion"))


def completion(d):
    p = d.p
    p.frame("status", 60, 320, 1730, 850, "Аппаратные признаки", target=sheet("structure", "regs"))
    p.node("engine", 100, 440, 530, 230, "i2c_master_engine",
           "core_state = 1 вне IDLE\nERR_ACK при NACK устройства\nили долгом ожидании SCL", parent="status", target=sheet("timing", "stretching"))
    p.node("writer", 100, 840, 530, 230, "rx_fifo_mux",
           "Записать слова и заголовок\nПолучить подтверждение AXI\nПередать DATA_VALID", parent="status", target=sheet("buffers", "publish"))
    p.node("regs", 1000, 620, 700, 300, "regs: CSR и IRQ",
           "CSR.core_state: состояние engine\nIRQ.DATA_VALID: готов новый результат\nIRQ.ERR_ACK: ошибка подтверждения\nIRQ: признаки empty и full для FIFO\nFINAL_PACKET: последний пакет", parent="status")
    p.edge("engine", "regs", "irq", "ERR_ACK", start="E", end="N", a_offset=-40,
           via=((790, 515), (1350, 515)), label_xy=(1120, 490))
    p.edge("engine", "regs", "mmio", "core_state", start="E", end="W", a_offset=60, b_offset=30,
           via=((900, 615), (900, 800)), label_xy=(805, 590))
    p.edge("writer", "regs", "irq", "DATA_VALID", start="E", end="S",
           via=((790, 955), (790, 1100), (1350, 1100)), label_xy=(1130, 1075))
    p.node("irq", 1860, 675, 280, 190, "IRQ → GIC",
           "Линия аппаратуры\nОжидание в PS — опрос", "L0", target="@architecture_bvstk_ps_pl.svg#i2c-master")
    p.edge("regs", "irq", "irq")

    p.frame("driver", 60, 1370, 2080, 1040, "Драйвер PS: wait_completion и восстановление", "L3",
            subtitle="Один захват mutex на весь вызов", target=sheet("commands", "driver"))
    p.node("prepare", 100, 1490, 530, 220, "Подготовить и запустить",
           "Дождаться свободного ядра\nСбросить старые признаки IRQ\nЗагрузить команду и запустить", "L3", parent="driver", target=sheet("commands", "driver"))
    p.node("poll", 850, 1490, 550, 220, "Опрос CSR и IRQ",
           "Проверить ERR_ACK\nПроверить условие готовности\nСверить срок по часам PS", "L3", parent="driver")
    p.node("success", 1610, 1490, 480, 220, "Успешное завершение",
           "Запись: core_state = 0\nЧтение: core_state = 0\nи новый DATA_VALID", "L3", parent="driver")
    p.node("recover", 850, 1910, 550, 290, "Сброс ядра",
           "Записать soft_reset в CSR\nВыдержать 10 мс\nNACK: вернуть ошибку\nДругие ошибки: повторить\nВсего до трёх попыток", "L3", parent="driver")
    p.node("return", 1610, 1925, 480, 260, "Завершить вызов",
           "При успешном чтении\nзабрать байты из BRAM\nОсвободить mutex\nВернуть статус и данные", "L3", parent="driver", target=sheet("buffers", "ps"))
    p.edge("prepare", "poll", "call")
    p.edge("poll", "success", "call")
    p.edge("success", "return", "call")
    p.edge("poll", "recover", "call", "Ошибка или истёк срок", label_xy=(1280, 1830))
    p.edge("recover", "return", "call", "NACK\nили лимит попыток", label_xy=(1505, 2010))
    p.edge("recover", "prepare", "call", "Новая попытка после других ошибок", start="S", end="S",
           via=((1125, 2320), (365, 2320)), label_xy=(650, 2295))
    p.edge("poll", "regs", "mmio", "Чтение через L1", start="N", end="S", a_offset=160, b_offset=200,
           via=((1285, 1270), (1550, 1270)), label_xy=(1450, 1245))

    d.heading("outcomes", 2550, "Как различаются окончания и ошибки")
    bottom = d.table("outcome-table", 2660, [("Событие", 550, "L0"), ("Поведение PL", 745, "PL"), ("Действие драйвера PS", 745, "L3")], [
        ("Ядро свободно после записи", "core_state = 0. DATA_VALID для обычной записи не требуется.", "Завершить ожидание, если ERR_ACK отсутствует."),
        ("Ядро свободно, результат чтения ещё записывается", "engine уже в IDLE; приёмная ветвь ещё не передала DATA_VALID.", "Продолжать опрос. Старое содержимое BRAM не принимается за новый результат."),
        ("NACK ведущего в конце чтения", "Master сам отпускает SDA в SEND_ACK по stop или restart. Это обычное окончание чтения.", "Дождаться core_state = 0 и DATA_VALID."),
        ("NACK устройства после адреса или байта записи", "READ_ACK → ERR_ACK → STOP. В IRQ фиксируется ERR_ACK.", "Сбросить ядро и вернуть ошибку; запись повторно не запускается."),
        ("Долгое удержание SCL в READ_ACK", "При заполнении stretch_cnt появляется ERR_ACK. Автомат может продолжать ждать фронт SCL.", "Обработать ERR_ACK, сбросить ядро и вернуть ошибку."),
        ("Истёк срок ожидания драйвера", "Аппаратная операция ещё не удовлетворила условию завершения.", "Сбросить ядро. Повторить попытку в пределах установленного лимита."),
        ("TX FIFO или RX FIFO заполнен", "В IRQ виден full; аппаратная линия IRQ поднимается.", "wait_completion проверяет ERR_ACK и DATA_VALID; отдельной ветви для full в нём нет."),
    ])
    p.note(bottom+70, "DATA_VALID и ERR_ACK сохраняются до сброса IRQ. FINAL_PACKET и признаки FIFO читаются из текущих аппаратных сигналов.")
    p.note(bottom+132, "Срок по умолчанию в драйвере — 100 мс. Ошибка MMIO также ведёт к восстановлению; подтверждённый NACK завершает вызов без повтора.")
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
        ("structure", "I²C master: состав ядра", "От команды PS к обмену на шине. Принятые байты проходят через RX FIFO в BRAM.", structure),
        ("commands", "I²C master: подготовка команды", "Заголовки и данные в TX FIFO, запуск через CSR и передача пакетов в engine.", commands),
        ("write", "I²C master: выполнение записи", "Команда, адрес, байты, подтверждения ведомого и завершение обмена.", write),
        ("read", "I²C master: выполнение чтения", "Длина команды задаёт число байтов. Ядро формирует такты, получает данные и сохраняет результат.", read),
        ("repeated-start", "I²C master: запись указателя и чтение", "Две фазы из одной очереди. Повторный START связывает передачу указателя и получение значения.", repeated_start),
        ("buffers", "I²C master: приёмные буферы и BRAM", "Пакеты RX FIFO, сборка слов и публикация результата для программы PS.", buffers),
        ("timing", "I²C master: управление SDA и SCL", "Делитель, входные фронты и ожидания в текущем i2c_master_engine. Масштаб времени условный.", timing),
        ("completion", "I²C master: завершение и восстановление", "Аппаратные признаки, опрос регистров программой PS и условия повторной попытки.", completion),
    ):
        p = page_class(sheet(name), title, subtitle, sources)
        draw(MasterDrawing(p, element, palette, arrows))
