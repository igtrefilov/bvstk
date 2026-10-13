"""MDIO core, driver and service figures for the system article."""
from i2c_core import CoreDrawing
from transaction_drawing import owned, sequence as draw_sequence


CORE = "src/drivers/pl/mdio/bvstk_mdio_core.c"
SERVICE = "src/services/mdio/bvstk_mdio_service.c"
PORT = "src/ports/freertos-xilinx/os/mdio/bvstk_mdio_freertos.c"
BASE = "mdio/"


def generate_mdio_details(Page, el, palette, arrows):
    def sheet(name, title, sources, width=1800):
        p = Page(BASE + name, title, title, sources)
        d = CoreDrawing(p, el, palette, arrows)
        d.canvas(width)
        return p, d

    def rtl(role, *names):
        return [f"../hw_platform/fpga/ip_repo/SMI_{role}/src/{n}.sv" for n in names]

    def sequence(name, title, actors, rows, sources, width=2200, ownership=(), core=(), core_title=""):
        p, d = sheet(name, title, sources, width)
        draw_sequence(d, actors, rows, ownership=ownership, core=core, core_title=core_title)
        return p, d

    sequence("cores/slave/decode.svg", "Разбор принятого кадра MDIO", [
        ("host", "Внешний ведущий", "", "L0", BASE + "transaction.svg#frame", 1),
        ("engine", "mdio_slave_engine", "", "PL", BASE + "cores/slave/structure.svg#engine", 1.2),
        ("ctrl", "mdio_data_ctrl", "", "PL", BASE + "cores/slave/structure.svg#ctrl", 1.2),
    ], [
        ("start", "host", "engine", "data", "ST = 01"),
        ("header", "host", "engine", "data", "OP = 01; PHY = 1; REG = 4"),
        ("value", "host", "engine", "data", "DATA = 0x0120"),
        ("done", "engine", "engine", "data", "DONE: запись собрана"),
        ("packet", "engine", "ctrl", "data", "transaction_view_t: 0x04240120"),
    ], rtl("slave", "mdio_slave_engine", "mdio_data_ctrl"), 1900,
       core=("engine", "ctrl"), core_title="SMI_slave_0")

    sequence("cores/master/command.svg", "Выполнение команды MDIO master", [
        ("fifo", "TX FIFO", "0x04240120", "PL", BASE + "cores/master/structure.svg#tx-fifo", .8),
        ("ctrl", "mdio_data_ctrl", "", "PL", BASE + "cores/master/structure.svg#data-ctrl", 1.1),
        ("engine", "mdio_master_engine", "", "PL", BASE + "cores/master/structure.svg#engine", 1.25),
        ("phy", "Физический PHY", "PHY 1", "L0", BASE + "subsystem.svg#external-target", 1),
    ], [
        ("take", "fifo", "ctrl", "data", "Одна команда"),
        ("accept", "ctrl", "engine", "data", "tx_valid и tx_ready"),
        ("prefix", "engine", "phy", "data", "PRE; ST; OP; PHY 1; REG 4"),
        ("data", "engine", "phy", "data", "TA = 10; DATA = 0x0120"),
        ("done", "engine", "engine", "data", "DONE → IDLE"),
    ], rtl("master", "mdio_data_ctrl", "mdio_master_engine"),
       core=("fifo", "ctrl", "engine"), core_title="SMI_master_0")

    p, d = sheet("cores/master/structure.svg", "Состав MDIO master",
                 rtl("master", "top_mdio_master", "regs", "mdio_data_ctrl",
                     "mdio_master_engine", "rx_fifo_mux"))
    p.node("driver", 800, 60, 500, 140, "Драйвер MDIO", "bvstk_mdio_core", "L3",
           target=BASE + "drivers/master/transfer.svg#driver")
    p.frame("core", 60, 310, 1320, 1200, "SMI_master_0",
            target=BASE + "subsystem.svg#master")
    p.node("regs", 800, 410, 500, 150, "Регистры", "regs", parent="core")
    p.node("tx-fifo", 140, 410, 500, 150, "TX FIFO", "xpm_fifo_sync", parent="core")
    p.frame("data-ctrl", 100, 660, 580, 365, "mdio_data_ctrl", parent="core")
    p.node("dispatch", 140, 740, 500, 105, "Выдача команды", parent="data-ctrl")
    p.node("capture", 140, 880, 500, 105, "Приём результата", parent="data-ctrl")
    p.node("engine", 800, 740, 500, 245, "Автомат MDIO", "mdio_master_engine", parent="core")
    p.node("rx-fifo", 140, 1120, 500, 130, "RX FIFO", "xpm_fifo_sync", parent="core")
    p.frame("publication", 760, 1060, 580, 405, "Публикация", parent="core")
    p.node("mux", 800, 1140, 500, 110, "rx_fifo_mux", parent="publication")
    p.node("axi", 800, 1320, 500, 105, "axi_lite_master_native", parent="publication")
    p.node("phy", 1450, 740, 290, 245, "Физический\nPHY", layer="L0")
    p.node("gic", 1450, 410, 290, 150, "GIC", layer="L0",
           target="@architecture_bvstk_ps_pl.svg#irq-smi-master")
    p.node("bram", 800, 1640, 500, 145, "BRAM: результат", "+0x0000",
           target=BASE + "cores/master/result.svg#bram")
    p.edge("driver", "regs", "mmio")
    p.edge("regs", "tx-fifo", "data", start="W", end="E")
    p.edge("tx-fifo", "dispatch", "data")
    p.edge("dispatch", "engine", "data", start="E", end="W", b_offset=-70)
    p.edge("engine", "capture", "data", start="W", end="E", a_offset=70)
    p.edge("capture", "rx-fifo", "data")
    p.edge("rx-fifo", "mux", "data", start="E", end="W", a_offset=10)
    p.edge("mux", "axi", "data")
    p.edge("axi", "bram", "data")
    p.edge("engine", "phy", "data", start="E", end="W", bidirectional=True)
    p.edge("regs", "gic", "irq", "IRQ", start="E", end="W")

    sequence("cores/master/result.svg", "Публикация результата MDIO", [
        ("core", "SMI_master_0", "", "PL", BASE + "cores/master/structure.svg#core", 1),
        ("bram", "BRAM", "+0x0000", "PL", None, .8),
        ("driver", "Драйвер MDIO", "", "L3", BASE + "drivers/master/transfer.svg#driver", 1.1),
        ("gic", "GIC", "Без обработчика MDIO", "L0", None, 1.1),
    ], [
        ("write", "core", "bram", "data", "0x42000090 ← 0x00000120"),
        ("stored", "bram", "core", "mmio", "AXI: запись завершена"),
        ("irq", "core", "gic", "irq", "DATA_VALID и IRQ"),
        ("hold", "core", "core", "data", "Новая публикация запрещена"),
        ("address", "driver", "core", "mmio", "Чтение MEM_ADDR"),
        ("take", "bram", "driver", "data", "Результат PHY 1, REG 4: 0x0120"),
        ("ack", "driver", "core", "mmio", "IRQ ← 1"),
        ("resume", "core", "core", "data", "Публикация снова разрешена"),
    ], rtl("master", "rx_fifo_mux", "regs") + [CORE])

    p, d = sheet("cores/slave/structure.svg", "Состав MDIO slave",
                 rtl("slave", "top_mdio_slave", "mdio_slave_engine", "mdio_data_ctrl"), 1600)
    p.node("host", 550, 60, 500, 140, "Внешний ведущий", "MDC и MDIO", "L0")
    p.frame("core", 430, 320, 740, 860, "SMI_slave_0", target=BASE + "subsystem.svg#slave")
    p.node("engine", 550, 415, 500, 150, "mdio_slave_engine", "debouncer и sync", parent="core")
    p.node("ctrl", 550, 720, 500, 150, "mdio_data_ctrl", parent="core")
    p.node("axi", 550, 990, 500, 140, "axi_lite_master_native", parent="core")
    p.node("gic", 1250, 720, 290, 150, "GIC", layer="L0")
    p.node("writes", 130, 1320, 500, 150, "Последние записи", "BRAM +0x1000",
           target=BASE + "drivers/slave/mailbox.svg#bram")
    p.node("replies", 970, 1320, 500, 150, "Подготовленные ответы", "BRAM +0x2000",
           target=BASE + "cores/slave/read.svg#bram")
    p.edge("host", "engine", "data", bidirectional=True)
    p.edge("engine", "ctrl", "data", start="S", end="N", a_offset=-100, b_offset=-100,
           label="Запрос", label_xy=(625, 650))
    p.edge("ctrl", "engine", "data", start="N", end="S", a_offset=100, b_offset=100,
           label="Ответ", label_xy=(970, 650))
    p.edge("ctrl", "axi", "data", bidirectional=True)
    p.edge("ctrl", "gic", "irq", "IRQ", start="E", end="W")
    p.edge("axi", "writes", "data", start="S", end="N", a_offset=-110,
           via=((690, 1250), (380, 1250)))
    p.edge("replies", "axi", "data", start="N", end="S", b_offset=110,
           via=((1220, 1250), (910, 1250)))

    sequence("cores/slave/read.svg", "Чтение готового ответа MDIO", [
        ("host", "Внешний ведущий", "", "L0", None, .9),
        ("engine", "mdio_slave_engine", "", "PL", BASE + "cores/slave/structure.svg#engine", 1.2),
        ("ctrl", "mdio_data_ctrl", "", "PL", BASE + "cores/slave/structure.svg#ctrl", 1.1),
        ("bram", "BRAM ответа", "+0x2000", "PL", None, .9),
        ("gic", "GIC", "", "L0", None, .55),
    ], [
        ("address", "host", "engine", "data", "Чтение: PHY 1, REG 4"),
        ("request", "engine", "ctrl", "data", "tx_valid и tx_ready"),
        ("fetch", "ctrl", "bram", "mmio", "Чтение 0x42002090"),
        ("value", "bram", "ctrl", "data", "0x0120"),
        ("event", "ctrl", "gic", "irq", "host_read и IRQ"),
        ("capture", "ctrl", "engine", "data", "rx_valid и rx_ready: send_data"),
        ("deadline", "engine", "engine", "data", "TA_TX: ответ уже принят"),
        ("reply", "engine", "host", "data", "TA = Z0; DATA = 0x0120"),
    ], rtl("slave", "mdio_slave_engine", "mdio_data_ctrl"),
       core=("engine", "ctrl"), core_title="SMI_slave_0")

    sequence("cores/slave/write.svg", "Приём записи MDIO", [
        ("host", "Внешний ведущий", "", "L0", BASE + "transaction.svg#frame", 1.1),
        ("core", "SMI_slave_0", "", "PL", BASE + "cores/slave/structure.svg#core", 1),
        ("bram", "Банк записей", "+0x1000", "PL", BASE + "drivers/slave/mailbox.svg#bram", 1),
        ("gic", "GIC", "", "L0", None, .8),
    ], [
        ("frame", "host", "core", "data", "PHY 1, REG 4, DATA 0x0120"),
        ("store", "core", "bram", "data", "0x42001090 ← 0x00000120"),
        ("done", "bram", "core", "mmio", "AXI: запись завершена"),
        ("irq", "core", "gic", "irq", "DATA_VALID и IRQ"),
    ], rtl("slave", "mdio_data_ctrl"))

    p, d = sheet("drivers/platform.svg", "Подключение MDIO к ОС", [CORE, SERVICE, PORT], 2200)
    owned(p, "freertos", 60, 70, 570, 910, "FreeRTOS")
    owned(p, "neutrino", 1620, 70, 520, 300, "Нейтрино")
    p.text(p.notes, 1670, 180, "MDIO не запущен", 24, width=420)
    p.node("worker", 110, 180, 470, 170, "Задача mdio", "bvstk_mdio_freertos", "L1", parent="freertos")
    p.node("service", 780, 180, 660, 170, "bvstk_mdio_service", "", "L3",
           target=BASE + "services/step.svg#service")
    p.node("driver", 780, 480, 660, 150, "bvstk_mdio_core", "", "L3",
           target=BASE + "drivers/master/transfer.svg#driver")
    p.node("interfaces", 780, 790, 660, 150, "MMIO, часы, mutex", "", "L1",
           target="@architecture_bvstk_l1.svg#platform-access--mmio")
    p.node("port", 110, 790, 470, 150, "Порт FreeRTOS", "MMIO и время", "L1", parent="freertos")
    p.node("kernel", 110, 480, 470, 150, "Ядро FreeRTOS", "Паузы и mutex", "L2", parent="freertos")
    p.node("hardware", 130, 1120, 430, 145, "SMI и BRAM", layer="PL",
           target=BASE + "subsystem.svg#subsystem")
    p.edge("worker", "service", "call", "service_step", start="E", end="W")
    p.edge("service", "driver", "call")
    p.edge("driver", "interfaces", "call")
    p.edge("interfaces", "port", "call", start="W", end="E")
    p.edge("port", "kernel", "call", start="N", end="S")
    p.edge("worker", "kernel", "call", start="S", end="N")
    p.edge("port", "hardware", "mmio", start="S", end="N")

    sequence("drivers/master/transfer.svg", "Ведущий вызов MDIO", [
        ("driver", "bvstk_mdio_core", "", "L3", BASE + "drivers/platform.svg#driver", 1.3),
        ("core", "SMI_master_0", "", "PL", BASE + "cores/master/structure.svg#core", 1),
        ("bram", "BRAM результата", "+0x0000", "PL", None, 1),
        ("gic", "GIC", "Без обработчика MDIO", "L0", None, 1.1),
    ], [
        ("ack", "driver", "core", "mmio", "MEM_ADDR, затем IRQ ← 1"),
        ("idle", "driver", "core", "mmio", "Ожидание свободного ядра"),
        ("mark", "driver", "bram", "data", "0x42000090 ← 0x00010000"),
        ("command", "driver", "core", "mmio", "TX FIFO ← 0x00240000"),
        ("result", "core", "bram", "data", "0x42000090 ← 0x00000120"),
        ("irq", "core", "gic", "irq", "IRQ"),
        ("poll", "driver", "core", "mmio", "DATA_VALID и MEM_ADDR"),
        ("read", "bram", "driver", "data", "PHY 1, REG 4: 0x0120"),
        ("release", "driver", "core", "mmio", "MEM_ADDR, затем IRQ ← 1"),
    ], [CORE])

    sequence("drivers/slave/mailbox.svg", "Потребление записи MDIO", [
        ("worker", "Задача mdio", "", "L1", BASE + "drivers/platform.svg#worker", .8),
        ("service", "Сервис MDIO", "service_step", "L3", BASE + "services/write.svg#service", 1.1),
        ("driver", "bvstk_mdio_core", "", "L3", BASE + "drivers/platform.svg#driver", 1.25),
        ("bram", "Банк записей", "+0x1000", "PL", BASE + "cores/slave/structure.svg#writes", 1),
    ], [
        ("step", "worker", "service", "call", "service_step"),
        ("take", "service", "driver", "call", "take_write: PHY 1, REG 4"),
        ("read", "bram", "driver", "data", "0x42001090: 0x00000120"),
        ("mark", "driver", "bram", "data", "0x42001090 ← 0x00010000"),
        ("value", "driver", "service", "data", "PHY 1, REG 4, DATA 0x0120"),
    ], [CORE, SERVICE, PORT], ownership=(("freertos", "FreeRTOS", ("worker",)),))

    p, d = sequence("drivers/slave/overwrite.svg", "Совмещение чтения PS и новой записи PL", [
        ("core", "SMI_slave_0", "", "PL", BASE + "cores/slave/write.svg#core", 1),
        ("bram", "Входящая ячейка", "0x42001090", "PL", BASE + "drivers/slave/mailbox.svg#bram", 1.1),
        ("driver", "bvstk_mdio_core", "take_write", "L3", BASE + "drivers/slave/mailbox.svg#driver", 1.15),
        ("service", "Сервис MDIO", "", "L3", BASE + "services/write.svg#service", 1),
    ], [
        ("old", "core", "bram", "data", "0x00000120"),
        ("read", "bram", "driver", "data", "PS прочитал 0x0120"),
        ("new", "core", "bram", "data", "Новая запись: 0x00000160"),
        ("mark", "driver", "bram", "data", "Метка: 0x00010000"),
        ("return", "driver", "service", "data", "Возвращается 0x0120"),
    ], [CORE] + rtl("slave", "mdio_data_ctrl"))
    p.legend_note = "Показано возможное наложение обращений. Новая запись 0x0160 заменена меткой."

    p, d = sheet("services/step.svg", "Рабочий проход сервиса MDIO", [SERVICE], 1700)
    p.frame("service", 570, 70, 1050, 1040, "bvstk_mdio_service", "L3")
    p.node("scan", 830, 160, 550, 135, "Просмотр ячеек", "До четырёх записей", "L3", parent="service")
    p.node("write", 830, 420, 550, 135, "Обработка записи", "Модель и политика", "L3", parent="service",
           target=BASE + "services/write.svg#service")
    p.node("ack", 830, 670, 550, 110, "Подтверждение IRQ slave", layer="L3", parent="service")
    p.node("probe", 640, 915, 410, 140, "Проверка PHY", "Регистры 2 и 3", "L3", parent="service")
    p.node("poll", 1140, 915, 410, 140, "Фоновое чтение", "Следующий регистр", "L3", parent="service")
    p.node("bram", 60, 160, 360, 135, "Банк записей", "BRAM +0x1000",
           target=BASE + "drivers/slave/mailbox.svg#bram")
    p.node("driver", 860, 1240, 490, 140, "bvstk_mdio_core", layer="L3",
           target=BASE + "drivers/master/transfer.svg#driver")
    p.edge("bram", "scan", "data")
    p.edge("scan", "write", "call")
    p.edge("write", "ack", "call")
    p.edge("ack", "probe", "call", start="S", end="N", a_offset=-150,
           via=((955, 840), (845, 840)), label="Срок проверки", label_xy=(800, 820))
    p.edge("ack", "poll", "call", start="S", end="N", a_offset=150,
           via=((1255, 840), (1345, 840)), label="Срок опроса", label_xy=(1460, 820))
    p.edge("probe", "driver", "call", start="S", end="N", b_offset=-150,
           via=((845, 1170), (955, 1170)))
    p.edge("poll", "driver", "call", start="S", end="N", b_offset=150,
           via=((1345, 1170), (1255, 1170)))
    p.legend_note = "Фоновая часть выбирает не более одного действия за проход."

    sequence("services/write.svg", "Разрешённая запись MDIO", [
        ("service", "Сервис MDIO", "PHY 1, REG 4", "L3", BASE + "drivers/slave/mailbox.svg#service", 1.2),
        ("driver", "bvstk_mdio_core", "", "L3", BASE + "drivers/master/transfer.svg#driver", 1.3),
        ("bram", "Ответ slave", "BRAM +0x2000", "PL", BASE + "cores/slave/read.svg#bram", 1),
    ], [
        ("policy", "service", "service", "call", "Whitelist: 0x0120, маска 0xFFFF"),
        ("invalid", "service", "service", "data", "Кэш: valid = 0"),
        ("placeholder", "service", "bram", "data", "Через драйвер: +0x2090 ← 0xFFFF"),
        ("write", "service", "driver", "call", "write: PHY 1, REG 4, DATA 0x0120"),
        ("sent", "driver", "service", "call", "Кадр отправлен"),
        ("read", "service", "driver", "call", "read: PHY 1, REG 4"),
        ("actual", "driver", "service", "data", "Фактическое значение: 0x0120"),
        ("publish", "service", "bram", "data", "Через драйвер: +0x2090 ← 0x0120"),
        ("cache", "service", "service", "data", "Кэш: 0x0120, valid = 1"),
    ], [SERVICE, CORE], 1800)
