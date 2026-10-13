"""Article figures for the common I2C drivers and their operating-system ports."""
from i2c_core import CoreDrawing
from transaction_drawing import owned, sequence


BASE = "i2c/drivers/"
MASTER = "src/drivers/pl/i2c/bvstk_i2c_master.c"
SLAVE = "src/drivers/pl/i2c/bvstk_i2c_slave.c"
F_PORT = "src/ports/freertos-xilinx/os/"
N_PORT = "src/ports/neutrino-zynq7000/os/"


def platform(d):
    p = d.p
    d.canvas(2200)
    p.frame("drivers", 460, 100, 1280, 260, "Драйверы I²C", "L3")
    p.node("master", 510, 185, 550, 130, "Ведущая часть",
           "bvstk_i2c_master", "L3", parent="drivers",
           target=BASE + "master/transfer.svg#driver")
    p.node("slave", 1140, 185, 550, 130, "Ведомая часть",
           "bvstk_i2c_slave", "L3", parent="drivers",
           target=BASE + "slave/write.svg#driver")

    owned(p, "freertos", 60, 425, 590, 410, "FreeRTOS")
    owned(p, "neutrino", 1550, 425, 590, 465, "Нейтрино")
    p.node("mmio", 800, 535, 600, 145, "bvstk_mmio",
           "read32, write32", "L1",
           target="@architecture_bvstk_l1.svg#platform-access--mmio")
    p.edge("drivers", "mmio", "call")
    p.node("f-mmio", 105, 535, 500, 180, "bvstk_platform_freertos",
           "Xil_In32, Xil_Out32", "L1", parent="freertos",
           ownership="freertos", target="@architecture_bvstk_l1.svg#f-port--mmio")
    p.node("n-mmio", 1595, 535, 500, 230, "bvstk_platform_neutrino",
           "mmap_device_memory\nPROT_NOCACHE", "L1", parent="neutrino",
           ownership="neutrino", target="@architecture_bvstk_l1.svg#n-port")
    p.edge("mmio", "f-mmio", "call", start="W", end="E", b_offset=-17.5)
    p.edge("mmio", "n-mmio", "call", start="E", end="W", b_offset=-42.5)

    p.frame("hardware", 360, 1050, 1480, 265, "Регистры и память I²C", "PL")
    p.node("master-core", 410, 1140, 450, 125, "I2C_master_0",
           "CSR, IRQ, TX FIFO", parent="hardware",
           target="i2c/cores/master/structure.svg#core")
    p.node("slave-core", 935, 1140, 450, 125, "I2C_slave_0",
           "CSR, IRQ, REQ", parent="hardware",
           target="i2c/cores/slave/structure.svg#core")
    p.node("bram", 1460, 1140, 330, 125, "BRAM I²C",
           "", parent="hardware")
    p.edge("f-mmio", "hardware", "mmio", start="S", end="N", b_offset=-460,
           via=((355, 975), (640, 975)))
    p.edge("n-mmio", "hardware", "mmio", start="S", end="N", b_offset=460,
           via=((1845, 975), (1560, 975)))


def master_transfer(d):
    y = d.actors([
        ("service", "bvstk_i2c_master_service", "", "L3", "i2c/services/master-operations.svg#service", 1.4),
        ("driver", "bvstk_i2c_master", "hw_transfer", "L3", BASE + "master/completion.svg#driver", 1.25),
        ("core", "I2C_master_0", "TX FIFO и регистры", "PL", "i2c/cores/master/commands.svg#core", 1.1),
    ], y=120)
    d.sequence(y, [
        ("request", "service", "driver", "call", "read_reg: 0x2A, регистр 0x10"),
        ("clear-irq", "driver", "core", "mmio", "Сброс старых IRQ"),
        ("write-header", "driver", "core", "mmio", "Заголовок записи: restart = 1"),
        ("pointer", "driver", "core", "data", "TX FIFO: 0x10"),
        ("read-header", "driver", "core", "mmio", "Заголовок чтения: один байт"),
        ("start", "driver", "core", "mmio", "CSR: START"),
    ])


def master_completion(d):
    d.canvas(2200)
    y = d.actors([
        ("driver", "bvstk_i2c_master", "wait_completion", "L3", BASE + "master/transfer.svg#driver", 1.3),
        ("core", "I2C_master_0", "CSR и регистр IRQ", "PL", "i2c/cores/master/completion.svg#regs", 1),
        ("bram", "BRAM I²C", "Результат: +0x0000", "PL", "i2c/cores/master/memory-format.svg", 1),
        ("gic", "GIC", "Без обработчика master", "L0", "@architecture_bvstk_ps_pl.svg#irq-i2c-master", 1),
    ], y=120)
    d.sequence(y, [
        ("publish", "core", "bram", "data", "Результат: 0x7B"),
        ("irq", "core", "gic", "irq", "IRQ master: DATA_VALID"),
        ("poll", "driver", "core", "mmio", "Опрос CSR и регистра IRQ"),
        ("ready", "core", "driver", "mmio", "Свободен, DATA_VALID = 1"),
        ("result", "bram", "driver", "data", "0x7B"),
    ])


def slave_write(d):
    d.canvas(2200)
    sequence(d, [
        ("port", "Порт I²C slave", "ISR и slave_task", "L1", BASE + "events.svg", 1.1),
        ("driver", "bvstk_i2c_slave", "", "L3", BASE + "platform.svg#slave", 1.1),
        ("core", "I2C_slave_0", "IRQ и REQ", "PL", "i2c/cores/slave/write.svg#core", .9),
        ("bram", "BRAM I²C", "+0x1000", "PL", "i2c/cores/slave/write.svg#bram", .85),
        ("service", "Сервис slave", "bvstk_i2c_slave_service", "L3", "i2c/services/write.svg#slave", 1.25),
    ], [
        ("irq", "core", "port", "irq", "IRQ через GIC: запись готова"),
        ("dispatch", "port", "driver", "call", "hw_capture_irq"),
        ("capture", "driver", "core", "mmio", "IRQ и REQ, затем сброс IRQ"),
        ("dispatch-task", "port", "port", "irq", "Очередь → slave_task"),
        ("read", "port", "driver", "call", "hw_read_frame"),
        ("copy", "bram", "driver", "data", "Заголовок 0x0000012A; 10 7B"),
        ("frame", "driver", "port", "data", "Адрес 0x2A; frame = {10, 7B}"),
        ("handle", "port", "service", "call", "handle_frame: 10 7B"),
    ], ownership=(("freertos", "FreeRTOS", ("port",)),))


def slave_read(d):
    d.canvas(2200)
    sequence(d, [
        ("port", "Порт I²C slave", "ISR и slave_task", "L1", BASE + "events.svg", 1.1),
        ("service", "bvstk_i2c_slave_service", "Подготовка окна", "L3", "i2c/services/read-state.svg#service", 1.4),
        ("driver", "bvstk_i2c_slave", "", "L3", BASE + "platform.svg#slave", 1.25),
        ("bram", "BRAM I²C", "Ответ: +0x2000", "PL", "i2c/cores/slave/buffers.svg#rd-window", 1),
        ("core", "I2C_slave_0", "", "PL", "i2c/cores/slave/read.svg#core", 1),
    ], [
        ("irq", "core", "port", "irq", "IRQ slave: RD_REQUEST"),
        ("event", "port", "port", "irq", "Захват события → slave_task"),
        ("dispatch", "port", "service", "call", "handle_frame: подготовить окно"),
        ("window", "service", "port", "data", "Окно ответа: первый байт 0x7B"),
        ("publish", "port", "driver", "call", "write_read_window, затем accept_read"),
        ("write", "driver", "bram", "data", "write_read_window: заголовок и байты"),
        ("valid", "driver", "core", "mmio", "accept_read: CSR.rd_valid"),
        ("fetch", "bram", "core", "data", "Окно ответа: первый байт 0x7B"),
    ], ownership=(("freertos", "FreeRTOS", ("port",)),))


def events(d):
    p = d.p
    d.canvas(2200)
    p.node("core", 800, 100, 600, 155, "I2C_slave_0",
           "DATA_VALID, RD_REQUEST", target="i2c/cores/slave/completion.svg#regs")
    owned(p, "freertos", 60, 325, 600, 1085, "FreeRTOS")
    owned(p, "neutrino", 1540, 285, 600, 1395, "Нейтрино")
    p.node("f-isr", 100, 610, 520, 160, "slave_isr",
           "Захват события", "L1", parent="freertos", ownership="freertos",
           source=F_PORT + "i2c/bvstk_i2c_slave_freertos.c")
    p.node("capture", 790, 610, 620, 160, "bvstk_i2c_slave",
           "hw_capture_irq", "L3", target=BASE + "slave/write.svg#capture")
    p.node("f-queue", 100, 930, 520, 150, "Очередь событий",
           "xQueueSendFromISR", "L2", parent="freertos", ownership="freertos",
           target="@architecture_bvstk_l1.svg#f-interrupt-processing--event")
    p.node("f-worker", 100, 1200, 520, 160, "slave_task",
           "xQueueReceive", "L1", parent="freertos", ownership="freertos",
           target="@architecture_bvstk_l1.svg#f-interrupt-processing--worker")
    p.node("n-wait", 1580, 365, 520, 150, "Ожидание IRQ",
           "SIGEV_INTR, InterruptWait", "L2", parent="neutrino", ownership="neutrino",
           target="@architecture_bvstk_l1.svg#n-interrupt-processing--event")
    p.node("n-capture", 1580, 610, 520, 160, "interrupt_thread",
           "Захват события", "L1", parent="neutrino", ownership="neutrino",
           source=N_PORT + "i2c/bvstk_i2c_slave_neutrino.c")
    p.node("n-process", 1580, 1200, 520, 160, "Обработка события",
           "Под state_mutex", "L1", parent="neutrino", ownership="neutrino",
           target="@architecture_bvstk_l1.svg#n-interrupt-processing--worker")
    p.node("n-unmask", 1580, 1490, 520, 140, "Размаскирование IRQ",
           "InterruptUnmask", "L2", parent="neutrino", ownership="neutrino")
    p.node("service", 790, 1200, 620, 160, "bvstk_i2c_slave_service",
           "handle_frame", "L3", target=BASE + "slave/read.svg#service")

    p.edge("core", "f-isr", "irq", start="W", end="N",
           via=((360, 177.5),))
    p.edge("core", "n-wait", "irq", start="E", end="N",
           via=((1840, 177.5),))
    p.edge("f-isr", "capture", "call", start="E", end="W")
    p.edge("n-capture", "capture", "call", start="W", end="E")
    p.edge("capture", "core", "mmio", "IRQ, REQ, сброс", start="N", end="S",
           label_xy=(1240, 460))
    p.edge("f-isr", "f-queue", "irq")
    p.edge("f-queue", "f-worker", "irq")
    p.edge("n-wait", "n-capture", "irq")
    p.edge("n-capture", "n-process", "call")
    p.edge("f-worker", "service", "call", start="E", end="W")
    p.edge("n-process", "service", "call", start="W", end="E")
    p.edge("n-process", "n-unmask", "call")


def generate_i2c_drivers(page, element, palette, arrows):
    definitions = [
        ("platform.svg", "I²C: общий драйвер и доступ к PL",
         "Две части драйвера используют один интерфейс MMIO. В каждой сборке работает свой порт ОС.",
         platform, [MASTER, SLAVE, F_PORT + "bvstk_platform_freertos.c", N_PORT + "bvstk_platform_neutrino.c"]),
        ("master/transfer.svg", "Драйвер master: чтение регистра",
         "Сервис запрашивает чтение у устройства 0x2A. Драйвер готовит запись указателя 0x10 и чтение одного байта до START.",
         master_transfer, [MASTER, "src/hardware/pl/i2c/bvstk_i2c_regs.h"]),
        ("master/completion.svg", "Драйвер master: IRQ и получение результата",
         "После публикации результата ядро поднимает IRQ. Драйвер ожидает опросом CSR и регистра IRQ; обработчик master не подключён.",
         master_completion, [MASTER, "../hw_platform/fpga/ip_repo/i2c_master/src/regs.sv", "../hw_platform/fpga/Burevestnik_21.tcl"]),
        ("slave/write.svg", "Драйвер slave: получение принятой записи",
         "После публикации фрагмента IRQ slave передаёт работу порту ОС. Порт вызывает общий драйвер для захвата события и чтения BRAM.",
         slave_write, [SLAVE, F_PORT + "i2c/bvstk_i2c_slave_freertos.c", N_PORT + "i2c/bvstk_i2c_slave_neutrino.c"]),
        ("slave/read.svg", "Драйвер slave: публикация окна ответа",
         "IRQ slave с RD_REQUEST начинает обработку в PS. Порт вызывает сервис, затем передаёт окно драйверу. Запись BRAM предшествует rd_valid.",
         slave_read, [SLAVE, "src/services/i2c/bvstk_i2c_slave_service.c"]),
        ("events.svg", "I²C slave: доставка и обработка события",
         "В FreeRTOS состояние захватывает ISR, в Нейтрино — поток после InterruptWait. Обе реализации вызывают общий драйвер и сервис.",
         events, [SLAVE, F_PORT + "i2c/bvstk_i2c_slave_freertos.c", N_PORT + "i2c/bvstk_i2c_slave_neutrino.c"]),
    ]
    for name, title, description, draw, sources in definitions:
        p = page(BASE + name, title, description, sources)
        drawing = CoreDrawing(p, element, palette, arrows)
        p.root.attrib.pop("data-layer", None)
        draw(drawing)
