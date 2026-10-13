"""SPI hardware and the active SD driver/service path for the article."""
from i2c_core import CoreDrawing
from transaction_drawing import owned, sequence as draw_sequence


BASE = "spi/"
DRIVER = "src/drivers/pl/sd/bvstk_sd_master.c"
SERVICE = "src/services/sd/bvstk_sd_service.c"
PORT = "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.c"


def generate_spi_details(Page, el, palette, arrows):
    def sheet(name, title, sources, width=1800):
        p = Page(BASE + name, title, title, sources)
        d = CoreDrawing(p, el, palette, arrows)
        d.canvas(width)
        return p, d

    def rtl(role, *names):
        folder = "SPI_master/src" if role == "master" else "SPI_Slave"
        return [f"../hw_platform/fpga/ip_repo/{folder}/{n}.sv" for n in names]

    def sequence(name, title, actors, rows, sources, width=2200, port=None, core=(), core_title=""):
        p, d = sheet(name, title, sources, width)
        owners = (("freertos", "FreeRTOS", (port,)),) if port else ()
        draw_sequence(d, actors, rows, ownership=owners, core=core, core_title=core_title)
        return p, d

    sequence("cores/master/command.svg", "Преобразование дескриптора в CMD17", [
        ("dma", "axi_dma_0", "MM2S", "PL", BASE + "subsystem.svg#dma0-tx", .85),
        ("parser", "spi_dma_2words_parser", "", "PL", BASE + "cores/master/structure.svg#parser", 1.3),
        ("commands", "sd_ini_tx_cmds", "", "PL", BASE + "cores/master/read-phases.svg#core", 1.05),
        ("engine", "spi_master_engine", "", "PL", BASE + "cores/master/structure.svg#engine", 1.15),
        ("card", "Карта SD", "LBA 32", "L0", BASE + "transaction.svg#command", .8),
    ], [
        ("argument", "dma", "parser", "data", "0x20000000: аргумент"),
        ("opcode", "dma", "parser", "data", "0x00003151: команда и CRC7"),
        ("ready", "parser", "commands", "data", "cmd_is_ready; cmds_buf_t"),
        ("bytes", "commands", "engine", "data", "51 00 00 00 20 31"),
        ("wire", "engine", "card", "data", "CMD17: биты MOSI"),
        ("dummy", "commands", "engine", "data", "0xFF: такты для ответа"),
    ], rtl("master", "spi_dma_2words_parser", "sd_ini_tx_cmds", "spi_master_engine") + [DRIVER],
       core=("parser", "commands", "engine"), core_title="SPI_master_0")

    p, d = sheet("cores/master/read-phases.svg", "Фазы аппаратного чтения CMD17",
                 rtl("master", "sd_ini_tx_cmds", "spi_master_ctrl"), 2100)
    widths = [305, 245, 265, 345, 225, 225]
    xpos = []
    x = 210
    for w in widths:
        xpos.append(x)
        x += w + 18
    rows = [
        ("mosi", "MOSI", 135, ["51 00 00 00 20 31", "FF …", "FF …", "FF …", "FF FF", "FF"]),
        ("miso", "MISO", 320, ["—", "FF … 00", "FF … FE", "DE AD BE EF …", "C_H C_L", "FF"]),
    ]
    for key, label, yy, values in rows:
        p.min_y = min(p.min_y, yy)
        p.text(p.notes, 60, yy+55, label, 26, bold=True, color=palette["L0"][1])
        group = el(p.blocks, "g", id=key, data_kind="wire-phases", data_layer="L0")
        for xx, w, value in zip(xpos, widths, values):
            el(group, "rect", x=xx, y=yy, width=w, height=100, rx=8,
               fill=palette["L0"][0], stroke=palette["L0"][1], stroke_width=1.8)
            p.text(group, xx+w/2, yy+55, value, 23, anchor="middle", width=w-28)
    p.used_layers.add("L0")
    p.frame("core", 60, 505, 1980, 365, "SPI_master_0: sd_ini_tx_cmds",
            target=BASE + "cores/master/structure.svg#commands")
    phases = [
        ("command", "Команда", "BYTE_1_…BYTE_6_"),
        ("r1", "R1", "etc_byte_"),
        ("token", "Маркер", "BYTE_FE_"),
        ("data", "512 байтов", "DATA_RX_"),
        ("crc", "CRC16", "DATA_RX_"),
        ("end", "Конец", "BYTE_FF_2"),
    ]
    for (key, label, state), xx, w in zip(phases, xpos, widths):
        p.node(key, xx, 630, w, 170, label, state, parent="core")
        el(p.under, "path", d=f"M{xx+w/2},430 V495 M{xx+w/2},580 V620", stroke="#8495a3",
           stroke_width=1.4, stroke_dasharray="5 7", fill="none")
    p.path("command", "end", ((210, 960), (2010, 960)), "data")
    p.legend_note = "Время слева направо. FF … — ожидание. C_H и C_L — CRC16. Прочерк — данные не используются. Масштаб условный."

    p, d = sheet("cores/master/stream.svg", "Полные слова и хвост блока SD",
                 rtl("master", "spi_master_ctrl", "axi_stream_master", "top_spi_master"), 1800)
    for suffix, yy, bus_data, offset, fields, flags, memory in [
        ("first", 60, "DE AD BE EF", "+0x1000",
         [("EF", "31…24", 165), ("BE", "23…16", 165), ("AD", "15…8", 165), ("DE", "7…0", 165)],
         "TKEEP = 1111; TLAST = 0", "0xEFBEADDE"),
        ("last", 550, "C_H C_L", "+0x1200",
         [("—", "31…16", 330), ("C_L", "15…8", 165), ("C_H", "7…0", 165)],
         "TKEEP = 0011; TLAST = 1", "Два байта CRC16"),
    ]:
        p.node("bus-"+suffix, 60, yy+110, 320, 155, "Карта SD", bus_data, "L0",
               target=BASE + "cores/master/receive.svg#card")
        p.frame("stream-"+suffix, 460, yy, 760, 365, "AXI Stream",
                target=BASE + "cores/master/results.svg#axis")
        d.word("word-"+suffix, yy+125, fields, x=510)
        group = p.blocks.find("{http://www.w3.org/2000/svg}g[@id='word-"+suffix+"']")
        p.blocks.remove(group)
        p.nodes["stream-"+suffix].group.append(group)
        p.text(p.notes, 840, yy+315, flags, 23, anchor="middle", color=palette["PL"][1])
        p.node("bram-"+suffix, 1390, yy+110, 350, 155, "BRAM " + offset, memory,
               target=BASE + "drivers/master/receive.svg#bram")
        p.path("bus-"+suffix, "word-"+suffix, ((380, yy+187.5), (510, yy+187.5)), "data")
        p.path("word-"+suffix, "bram-"+suffix, ((1170, yy+187.5), (1390, yy+187.5)), "data",
               "S2MM", (1305, yy+165))
    p.legend_note = "Показаны первое и последнее слова. Между ними ещё 127 полных слов. C_H и C_L — байты CRC16."

    p, d = sheet("drivers/master/readiness.svg", "Независимые признаки завершения CMD17", [DRIVER, PORT], 2000)
    owned(p, "freertos", 60, 550, 470, 270, "FreeRTOS")
    rows = [
        ("tx", 60, "MM2S", "Дескриптор передан", "PL", 800, "IOC + IDLE", "mmio", None),
        ("reply", 340, "Ответ CMD17", "BRAM +0x003C", "PL", 1080, "Ответ изменился", "data", None),
        ("irq", 630, "Порт SD", "core_isr", "L1", 1330, "irq_count = 1", "irq", "freertos"),
        ("rx", 930, "S2MM", "Блок принят", "PL", 1580, "IOC + IDLE", "mmio", None),
    ]
    for key, yy, label, body, layer, event_x, event, kind, parent in rows:
        p.node(key, 95, yy, 400, 145, label, body, layer, parent=parent,
               target=BASE + ("drivers/irq.svg#port" if key == "irq" else "drivers/master/completion.svg#" + ("reply" if key == "reply" else "dma")))
        y = yy+72.5
        el(p.under, "path", d=f"M555,{y} H1780", stroke="#8495a3", stroke_width=1.5,
           stroke_dasharray="4 8", fill="none")
        color = arrows[kind][0]
        p.used_arrows.add(kind)
        el(p.edges, "path", d=f"M{event_x},{y} H1780", stroke=color, stroke_width=4,
           stroke_dasharray=arrows[kind][2], fill="none")
        el(p.edges, "circle", cx=event_x, cy=y, r=7, fill=color)
        p.text(p.notes, event_x, y-25, event, 24, anchor="middle", color=color)
    el(p.under, "path", d="M1780,80 V1175", stroke=palette["L3"][1],
       stroke_width=2, stroke_dasharray="7 9", fill="none")
    p.text(p.notes, 1780, 45, "Проверка PS", 24, anchor="middle", color=palette["L3"][1])
    p.node("driver", 1510, 1250, 430, 170, "bvstk_sd_master", "Длина, границы, CRC16", "L3",
           target=BASE + "drivers/master/completion.svg#driver")
    p.path("rx", "driver", ((1780, 1175), (1780, 1250)), "call")
    p.text(p.notes, 780, 1200, "Время →", 25, color="#576c7b")
    p.legend_note = "Один из возможных порядков событий. Линия после точки означает, что признак уже наблюдается. Масштаб условный."

    p, d = sheet("cores/master/structure.svg", "Состав SPI master",
                 rtl("master", "top_spi_master", "spi_master_ctrl", "spi_dma_2words_parser",
                     "sd_ini_tx_cmds", "spi_master_engine"), 2000)
    p.node("dma", 120, 60, 560, 145, "axi_dma_0: MM2S",
           target=BASE + "subsystem.svg#dma0-tx")
    p.node("driver", 1080, 60, 560, 145, "bvstk_sd_master", layer="L3",
           target=BASE + "drivers/master/read.svg#driver")
    p.frame("core", 60, 330, 1680, 1190, "SPI_master_0",
            target=BASE + "subsystem.svg#master")
    p.node("fifo", 120, 430, 560, 140, "xpm_fifo_axis", parent="core")
    p.node("regs", 1080, 430, 560, 140, "spi_regs", parent="core")
    p.frame("ctrl", 90, 700, 620, 470, "spi_master_ctrl", parent="core")
    p.node("stream", 130, 790, 540, 120, "Слова и байты", parent="ctrl")
    p.node("parser", 130, 1000, 540, 125, "spi_dma_2words_parser", parent="ctrl")
    p.node("commands", 1080, 1000, 560, 125, "sd_ini_tx_cmds", parent="core")
    p.node("engine", 1080, 1320, 560, 140, "spi_master_engine", parent="core")
    p.node("pins", 1080, 1650, 560, 145, "Карта SD", "MOSI, MISO, SCK, CS", "L0")
    p.node("gic", 1790, 430, 160, 140, "GIC", layer="L0")
    p.edge("dma", "fifo", "data")
    p.edge("driver", "regs", "mmio")
    p.edge("fifo", "stream", "data")
    p.edge("stream", "parser", "data")
    p.edge("parser", "commands", "data", "Дескриптор", start="E", end="W")
    p.edge("regs", "commands", "mmio", "Настройка", start="S", end="N",
           label_xy=(1445, 800))
    p.edge("commands", "engine", "data", bidirectional=True)
    p.edge("engine", "pins", "data", bidirectional=True)
    p.edge("regs", "gic", "irq", "IRQ", start="E", end="W")

    sequence("cores/master/initialization.svg", "Подготовка SD в PL", [
        ("driver", "bvstk_sd_master", "", "L3", BASE + "drivers/master/read.svg#driver", 1.2),
        ("core", "SPI_master_0", "", "PL", BASE + "cores/master/structure.svg#core", 1),
        ("card", "Карта SD", "", "L0", None, .8),
        ("bram", "BRAM", "Ответы инициализации", "PL", None, 1.1),
    ], [
        ("start", "driver", "core", "mmio", "Запуск инициализации"),
        ("clocks", "core", "card", "clock", "spi_sd_ini: начальные такты"),
        ("commands", "core", "card", "data", "CMD0, CMD8, CMD58"),
        ("initial-replies", "card", "core", "data", "Ответы первых команд"),
        ("initial-store", "core", "bram", "data", "Публикация ответов"),
        ("ready", "core", "card", "data", "CMD55 и ACMD41 до готовности"),
        ("ready-replies", "card", "core", "data", "Карта готова"),
        ("ready-store", "core", "bram", "data", "Ответы CMD55 и ACMD41"),
        ("ocr", "core", "card", "data", "Заключительный CMD58"),
        ("ocr-reply", "card", "core", "data", "Ответ с OCR"),
        ("replies", "core", "bram", "data", "Итоговый OCR"),
        ("read", "bram", "driver", "data", "Проверка семи слов"),
    ], rtl("master", "spi_sd_ini", "sd_ini_tx_cmds", "spi_regs") + [DRIVER])

    p, d = sheet("cores/master/results.svg", "Результаты SD-команды",
                 rtl("master", "spi_regs", "spi_master_ctrl", "sd_ini_tx_cmds", "top_spi_master"), 1600)
    p.node("card", 530, 60, 540, 140, "Карта SD", layer="L0")
    p.frame("core", 60, 350, 1480, 785, "SPI_master_0",
            target=BASE + "cores/master/structure.svg#core")
    p.node("command", 170, 470, 480, 155, "sd_ini_tx_cmds", "Ответ команды", parent="core")
    p.node("stream", 950, 470, 480, 155, "spi_master_ctrl", "Блок и CRC16", parent="core")
    p.node("regs", 170, 745, 480, 125, "spi_regs", parent="core")
    p.node("fifo", 950, 745, 480, 125, "RX FIFO", parent="core")
    p.node("axi", 170, 970, 480, 120, "axi_lite_master_native", parent="core")
    p.node("axis", 950, 970, 480, 120, "axi_stream_master", parent="core")
    p.node("dma", 950, 1270, 480, 140, "axi_dma_0: S2MM",
           target=BASE + "subsystem.svg#dma0-rx")
    p.node("mailbox", 170, 1550, 480, 150, "BRAM: ответ команды")
    p.node("block", 950, 1550, 480, 150, "BRAM: блок с CRC", "+0x1000")
    p.edge("card", "command", "data", start="S", end="N", a_offset=-150,
           via=((650, 270), (410, 270)))
    p.edge("card", "stream", "data", start="S", end="N", a_offset=150,
           via=((950, 300), (1190, 300)))
    p.edge("command", "regs", "data")
    p.edge("regs", "axi", "data")
    p.edge("axi", "mailbox", "data")
    p.edge("stream", "fifo", "data")
    p.edge("fifo", "axis", "data")
    p.edge("axis", "dma", "data")
    p.edge("dma", "block", "data")

    p, d = sheet("cores/slave/structure.svg", "Состав SPI slave",
                 rtl("slave", "top_spi_slave", "spi_slave_ctrl", "spi_slave_engine"), 2200)
    p.node("host", 630, 60, 540, 140, "Внешний ведущий SPI", "SCK и CS", "L0")
    p.frame("core", 60, 330, 1680, 1110, "SPI_Slave_0",
            target=BASE + "subsystem.svg#slave")
    p.node("engine", 630, 430, 540, 150, "spi_slave_engine", parent="core")
    p.node("ctrl", 630, 740, 540, 150, "spi_slave_ctrl", parent="core")
    p.node("rx", 190, 1040, 480, 140, "FIFO ответа", "RX в RTL", parent="core")
    p.node("tx", 1130, 1040, 480, 140, "FIFO принятого MOSI", "TX в RTL", parent="core")
    p.node("axis-in", 190, 1260, 480, 120, "axi_stream_slave", parent="core")
    p.node("axis-out", 1130, 1260, 480, 120, "axi_stream_master", parent="core")
    p.node("mm2s", 190, 1570, 480, 140, "axi_dma_1: MM2S",
           target=BASE + "subsystem.svg#dma1-tx")
    p.node("s2mm", 1130, 1570, 480, 140, "axi_dma_1: S2MM",
           target=BASE + "subsystem.svg#dma1-rx")
    p.node("gic", 1820, 740, 320, 150, "GIC", layer="L0")
    p.edge("host", "engine", "data", start="S", end="N", a_offset=-120, b_offset=-120,
           label="MOSI", label_xy=(690, 265))
    p.edge("engine", "host", "data", start="N", end="S", a_offset=120, b_offset=120,
           label="MISO", label_xy=(1110, 265))
    p.edge("engine", "ctrl", "data", start="S", end="N", a_offset=-120, b_offset=-120)
    p.edge("ctrl", "engine", "data", start="N", end="S", a_offset=120, b_offset=120)
    p.edge("ctrl", "gic", "irq", "IRQ", start="E", end="W")
    p.edge("rx", "ctrl", "data", start="N", end="S", b_offset=-120,
           via=((430, 965), (780, 965)))
    p.edge("ctrl", "tx", "data", start="S", end="N", a_offset=120,
           via=((1020, 965), (1370, 965)))
    p.edge("axis-in", "rx", "data")
    p.edge("tx", "axis-out", "data")
    p.edge("mm2s", "axis-in", "data")
    p.edge("axis-out", "s2mm", "data")

    p, d = sheet("cores/slave/packing.svg", "Байты SPI и слово потока",
                 rtl("slave", "spi_slave_ctrl", "axi_stream_master"), 2000)
    p.node("bus", 60, 535, 360, 155, "Байты шины", "B0, B1, B2, B3", "L0")
    p.frame("ctrl", 470, 430, 940, 320, "spi_slave_ctrl",
            target=BASE + "cores/slave/structure.svg#ctrl")
    d.word("word", 550, [(f"B{i}", f"{8*i+7}…{8*i}", 200) for i in (3, 2, 1, 0)], x=530)
    word = p.blocks.find("{http://www.w3.org/2000/svg}g[@id='word']")
    p.blocks.remove(word)
    p.nodes["ctrl"].group.append(word)
    p.node("axis", 1480, 535, 460, 155, "AXI Stream", "32-битное слово")
    p.path("bus", "word", ((420, 612.5), (530, 612.5)), "data", bidirectional=True)
    p.path("word", "axis", ((1330, 612.5), (1480, 612.5)), "data", bidirectional=True)
    p.legend_note = "B0 — первый байт. Показано полное слово из четырёх байтов."

    p, d = sheet("drivers/platform.svg", "Подключение SD к ОС", [DRIVER, SERVICE, PORT], 2200)
    owned(p, "freertos", 60, 70, 650, 940, "FreeRTOS")
    owned(p, "neutrino", 1750, 70, 390, 330, "Нейтрино")
    p.text(p.notes, 1790, 185, "SD через PL\nне подключён", 24, width=310)
    p.node("client", 110, 180, 550, 170, "bvstk_sd_pl", "Файловый доступ", "L1", parent="freertos")
    p.node("service", 900, 180, 650, 170, "bvstk_sd_service", layer="L3",
           target=BASE + "services/storage.svg#service")
    p.node("driver", 900, 480, 650, 150, "bvstk_sd_master", layer="L3",
           target=BASE + "drivers/master/read.svg#driver")
    p.node("interfaces", 900, 800, 650, 160, "MMIO, часы, события", layer="L1")
    p.node("port", 110, 800, 550, 160, "Порт SD", "bvstk_sd_master_freertos", "L1", parent="freertos",
           target=BASE + "drivers/irq.svg#port")
    p.node("kernel", 110, 480, 550, 150, "Ядро FreeRTOS", "Паузы и mutex", "L2", parent="freertos")
    p.node("hardware", 110, 1160, 550, 145, "SPI, DMA и BRAM", layer="PL",
           target=BASE + "subsystem.svg#subsystem")
    p.edge("client", "service", "call", start="E", end="W")
    p.edge("service", "driver", "call")
    p.edge("driver", "interfaces", "call")
    p.edge("interfaces", "port", "call", start="W", end="E")
    p.edge("port", "kernel", "call", start="N", end="S")
    p.edge("port", "hardware", "mmio", start="S", end="N", a_offset=-90, b_offset=-90)
    p.edge("hardware", "port", "irq", "Через GIC", start="N", end="S", a_offset=90, b_offset=90,
           label_xy=(550, 1090))

    sequence("drivers/irq.svg", "Учёт события SD", [
        ("port", "Порт SD", "prepare и core_isr", "L1", BASE + "drivers/platform.svg#port", 1.2),
        ("driver", "bvstk_sd_master", "", "L3", BASE + "drivers/master/read.svg#driver", 1.2),
        ("core", "SPI_master_0", "", "PL", BASE + "cores/master/structure.svg#core", 1),
        ("gic", "GIC", "", "L0", None, .7),
    ], [
        ("prepare", "driver", "port", "call", "prepare"),
        ("ack", "port", "core", "mmio", "Подтверждение старого IRQ"),
        ("arm", "port", "gic", "mmio", "Очистить и разрешить источник"),
        ("irq", "core", "gic", "irq", "Новое событие"),
        ("isr", "gic", "port", "irq", "core_isr"),
        ("mask", "port", "gic", "mmio", "Запретить источник"),
        ("count", "port", "port", "call", "irq_count += 1"),
        ("observe", "port", "driver", "data", "Счётчик события"),
    ], [PORT, DRIVER], port="port")

    p, d = sequence("drivers/master/read.svg", "Чтение блока через SPI master", [
        ("port", "Порт SD", "IRQ", "L1", BASE + "drivers/irq.svg#port", 1),
        ("driver", "bvstk_sd_master", "", "L3", BASE + "drivers/platform.svg#driver", 1.2),
        ("bram", "BRAM банка 0", "", "PL", None, 1),
        ("hardware", "SPI и axi_dma_0", "", "PL", BASE + "subsystem.svg#master", 1.2),
    ], [
        ("prepare", "driver", "port", "call", "Подготовить новое событие"),
        ("descriptor", "driver", "bram", "data", "CMD17: LBA 32; буферы"),
        ("receive", "driver", "hardware", "mmio", "S2MM: 514 байтов"),
        ("send", "driver", "hardware", "mmio", "MM2S: 8 байтов, затем START"),
        ("data", "hardware", "bram", "data", "DE AD BE EF … и CRC16"),
        ("irq", "hardware", "port", "irq", "IRQ через GIC"),
        ("status", "driver", "hardware", "mmio", "Проверка завершения DMA"),
        ("result", "bram", "driver", "data", "Данные для проверки"),
    ], [DRIVER, PORT], port="port")
    p.legend_note = "Время идёт сверху вниз. Порядок IRQ и завершения DMA может различаться."

    p, d = sheet("drivers/master/completion.svg", "Проверка результата SD", [DRIVER, PORT], 1800)
    owned(p, "freertos", 60, 60, 540, 290, "FreeRTOS")
    p.node("irq", 110, 145, 440, 150, "Новый IRQ", "Счётчик порта", "L1", parent="freertos",
           target=BASE + "drivers/irq.svg#port")
    p.node("dma", 680, 145, 440, 150, "MM2S и S2MM", "IOC и IDLE")
    p.node("reply", 1250, 145, 440, 150, "Ответ в BRAM", "Значение изменилось")
    p.frame("driver", 480, 510, 840, 820, "bvstk_sd_master", "L3",
            target=BASE + "drivers/platform.svg#driver")
    p.node("wait", 610, 605, 580, 140, "Все признаки завершения", layer="L3", parent="driver")
    p.node("bounds", 610, 880, 580, 125, "Длина и границы буфера", layer="L3", parent="driver")
    p.node("checks", 610, 1150, 580, 125, "CRC16 и ответ команды", layer="L3", parent="driver")
    p.node("block", 610, 1460, 580, 140, "Буфер вызывающего кода", "512 байтов", "L3")
    p.edge("irq", "wait", "data", start="S", end="W", via=((330, 675),))
    p.edge("dma", "wait", "mmio", start="S", end="N")
    p.edge("reply", "wait", "data", start="S", end="E", via=((1470, 675),))
    p.edge("wait", "bounds", "call")
    p.edge("bounds", "checks", "call")
    p.edge("checks", "block", "data")

    p, d = sequence("drivers/master/write.svg", "Передача блока CMD24", [
        ("port", "Порт SD", "IRQ", "L1", BASE + "drivers/irq.svg#port", 1),
        ("driver", "bvstk_sd_master", "", "L3", BASE + "drivers/platform.svg#driver", 1.2),
        ("bram", "BRAM банка 0", "", "PL", None, 1),
        ("hardware", "SPI и axi_dma_0", "", "PL", BASE + "subsystem.svg#master", 1.2),
    ], [
        ("prepare", "driver", "port", "call", "Подготовить новое событие"),
        ("block", "driver", "bram", "data", "CMD24, блок и CRC16"),
        ("start", "driver", "hardware", "mmio", "MM2S: 522 байта, затем START"),
        ("sent", "bram", "hardware", "data", "Команда и данные"),
        ("reply", "hardware", "bram", "data", "Ответ CMD24"),
        ("irq", "hardware", "port", "irq", "IRQ через GIC"),
        ("check", "driver", "hardware", "mmio", "Проверка MM2S"),
        ("result", "bram", "driver", "data", "Проверка ответа и границ"),
    ], [DRIVER, PORT], port="port")
    p.legend_note = "Время идёт сверху вниз. Порядок IRQ и завершения DMA может различаться."

    p, d = sheet("services/storage.svg", "Файловый доступ к SD",
                 [SERVICE, "src/services/sd/bvstk_sd_volume.c",
                  "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.c"], 1800)
    owned(p, "freertos", 60, 70, 650, 570, "FreeRTOS")
    p.node("fatfs", 110, 170, 550, 130, "FatFs", layer="L3", parent="freertos")
    p.node("port", 110, 450, 550, 140, "bvstk_sd_pl", layer="L1", parent="freertos")
    p.node("volume", 1020, 430, 660, 180, "bvstk_sd_volume", "Границы тома", "L3")
    p.node("service", 1020, 820, 660, 140, "bvstk_sd_service", layer="L3",
           target=BASE + "services/write-verify.svg#service")
    p.node("driver", 1020, 1120, 660, 140, "bvstk_sd_master", layer="L3",
           target=BASE + "drivers/master/read.svg#driver")
    p.node("hardware", 1020, 1430, 660, 150, "SPI, DMA и BRAM",
           target=BASE + "subsystem.svg#subsystem")
    p.edge("fatfs", "port", "call", "Сектор тома", label_xy=(470, 380))
    p.edge("port", "volume", "call", start="E", end="W", a_offset=-35, b_offset=-35,
           label="map", label_xy=(830, 465))
    p.edge("volume", "port", "data", start="W", end="E", a_offset=35, b_offset=35,
           label="Физический LBA", label_xy=(830, 590))
    p.edge("port", "service", "call", start="S", end="W", via=((385, 890),),
           label="Операция с блоком", label_xy=(680, 865))
    p.edge("service", "driver", "call")
    p.edge("driver", "hardware", "mmio", "Через L1", label_xy=(1440, 1350))

    p, d = sequence("services/write-verify.svg", "Запись сектора с проверкой", [
        ("service", "bvstk_sd_service", "", "L3", BASE + "services/storage.svg#service", 1.15),
        ("driver", "bvstk_sd_master", "", "L3", BASE + "drivers/master/write.svg#driver", 1),
    ], [
        ("write", "service", "driver", "call", "CMD24: записать блок"),
        ("sent", "driver", "service", "call", "Запись завершена"),
        ("read", "service", "driver", "call", "CMD17: прочитать тот же LBA"),
        ("block", "driver", "service", "data", "Проверенный блок"),
        ("compare", "service", "service", "call", "Сравнить 512 байтов"),
    ], [SERVICE], 1500)
    p.legend_note = "Mutex сервиса удерживается до окончания сравнения. Время идёт сверху вниз."
