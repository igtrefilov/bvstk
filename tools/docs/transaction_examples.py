"""Concrete transactions connecting cores, drivers and services on three buses."""
from i2c_core import CoreDrawing
from protocol_drawing import ProtocolFigure
from transaction_drawing import sequence, wire_row


def generate_transaction_examples(Page, el, palette, arrows):
    def sheet(name, title, sources, width=2200):
        p = Page(name, title, title, sources)
        d = CoreDrawing(p, el, palette, arrows)
        d.canvas(width)
        return p, d

    def wire(name, title, sources):
        p = Page(name, title, title, sources)
        d = ProtocolFigure(p, el, palette, arrows)
        p.width = 1800
        p.root.set("width", "1800")
        p.background.set("width", "1800")
        return p, d

    p, d = wire("i2c/transaction.svg", "Запись 0x7B в регистр 0x10 устройства 0x2A",
                ["src/services/i2c/bvstk_i2c_slave_service.c"])
    wire_row(d, "frame", 130, [
        ("S", "", .5), ("0x54", "0x2A + W", 1.8), ("ACK", "", .7),
        ("0x10", "Регистр", 1.6), ("ACK", "", .7),
        ("0x7B", "Значение", 1.6), ("ACK", "", .7), ("P", "", .5),
    ], "Внешний ведущий → I2C_slave_0")
    p.legend_note = "S — START. P — STOP. ACK выдаёт I2C_slave_0. Поля идут слева направо."

    p, d = wire("mdio/transaction.svg", "Запись 0x0120 в регистр 4 PHY 1",
                ["../hw_platform/fpga/ip_repo/SMI_slave/src/mdio_slave_engine.sv"])
    wire_row(d, "frame", 130, [
        ("32 × 1", "PRE", 1.4), ("01", "ST", .7), ("01", "OP: запись", 1.1),
        ("00001", "PHY 1", 1.2), ("00100", "REG 4", 1.2),
        ("10", "TA", .7), ("0x0120", "DATA: 16 бит", 1.7),
    ], "Внешний ведущий → SMI_slave_0")
    p.legend_note = "Все биты кадра записи задаёт внешний ведущий. Поля идут слева направо."

    p, d = wire("spi/transaction.svg", "Чтение блока SD с LBA 32",
                ["src/drivers/pl/sd/bvstk_sd_master.c",
                 "../hw_platform/fpga/ip_repo/SPI_master/src/sd_ini_tx_cmds.sv"])
    wire_row(d, "command", 130, [
        ("0x51", "CMD17", 1), ("00 00 00 20", "LBA 32", 2),
        ("0x31", "CRC7, конец = 1", 1.2),
    ], "SPI_master_0 → карта: MOSI")
    wire_row(d, "reply", 460, [
        ("0x00", "R1", .8), ("0xFE", "Начало блока", 1.2),
        ("DE AD BE EF …", "512 байтов", 2.7), ("CRC16", "2 байта", 1.2),
    ], "Карта → SPI_master_0: MISO")
    p.legend_note = "Поля идут слева направо. Паузы опущены; ширина полей не задаёт длительность."

    p, d = sheet("i2c/services/write.svg", "Обработка принятой записи I²C",
                 ["src/services/i2c/bvstk_i2c_slave_service.c",
                  "src/services/i2c/bvstk_i2c_master_service.c"])
    sequence(d, [
        ("slave", "Сервис slave", "bvstk_i2c_slave_service", "L3", "i2c/drivers/slave/write.svg#service", 1.3),
        ("service", "Сервис master", "bvstk_i2c_master_service", "L3", "i2c/services/master-operations.svg#service", 1.3),
        ("driver", "bvstk_i2c_master", "", "L3", "i2c/drivers/master/transfer.svg#driver", 1.1),
        ("cache", "Кэш I²C", "Устройство 0x2A", "L3", "overview/cache.svg#cache", .9),
    ], [
        ("parse", "slave", "slave", "data", "10 7B → регистр 0x10, значение 0x7B"),
        ("write", "slave", "service", "call", "write: 0x10 ← 0x7B"),
        ("policy", "service", "service", "call", "Whitelist: (0x10, 0x7B)"),
        ("send", "service", "driver", "call", "write_reg: 0x2A, 0x10, 0x7B"),
        ("done", "driver", "service", "call", "BVSTK_OK"),
        ("cache-write", "service", "cache", "data", "Регистр 0x10: 0x7B, valid = 1"),
        ("return", "service", "slave", "call", "BVSTK_OK"),
    ])

    p, d = sheet("spi/cores/master/receive.svg", "Приём блока 32 от SD в PL",
                 ["../hw_platform/fpga/ip_repo/SPI_master/src/spi_master_ctrl.sv",
                  "../hw_platform/fpga/ip_repo/SPI_master/src/sd_ini_tx_cmds.sv",
                  "../hw_platform/fpga/ip_repo/SPI_master/src/spi_regs.sv"])
    sequence(d, [
        ("card", "Карта SD", "LBA 32", "L0", "spi/transaction.svg#reply", .9),
        ("core", "SPI_master_0", "", "PL", "spi/cores/master/structure.svg#core", 1.1),
        ("dma", "axi_dma_0", "S2MM", "PL", "spi/subsystem.svg#dma0-rx", .9),
        ("bram", "BRAM банка 0", "", "PL", "spi/drivers/master/receive.svg#bram", 1.1),
    ], [
        ("response", "card", "core", "data", "R1 = 0x00"),
        ("mailbox", "core", "bram", "data", "Ответ CMD17: +0x003C"),
        ("token", "card", "core", "data", "Маркер 0xFE"),
        ("bytes", "card", "core", "data", "DE AD BE EF … и CRC16"),
        ("stream", "core", "dma", "data", "AXI Stream: 514 байтов"),
        ("store", "dma", "bram", "data", "+0x1000: DE AD BE EF … и CRC16"),
    ])
    p.legend_note = "Поток передаётся порциями. Время идёт сверху вниз; масштаб условный."

    p, d = sheet("spi/drivers/master/receive.svg", "Проверка принятого блока 32",
                 ["src/drivers/pl/sd/bvstk_sd_master.c",
                  "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_master_freertos.c"])
    sequence(d, [
        ("port", "Порт SD", "core_isr", "L1", "spi/drivers/irq.svg#port", .85),
        ("driver", "bvstk_sd_master", "", "L3", "spi/drivers/master/completion.svg#driver", 1.1),
        ("hardware", "SPI и axi_dma_0", "", "PL", "spi/cores/master/receive.svg#core", 1),
        ("bram", "BRAM банка 0", "", "PL", "spi/cores/master/receive.svg#bram", .95),
        ("service", "Сервис SD", "bvstk_sd_service", "L3", "spi/services/read.svg#service", 1.15),
    ], [
        ("irq", "hardware", "port", "irq", "IRQ через GIC"),
        ("event", "port", "driver", "data", "irq_count > 0"),
        ("dma", "driver", "hardware", "mmio", "MM2S и S2MM: IOC, IDLE, без ошибок"),
        ("response", "bram", "driver", "data", "Ответ CMD17 изменился"),
        ("block", "bram", "driver", "data", "DE AD BE EF … и CRC16"),
        ("check", "driver", "driver", "call", "Длина 514, границы, CRC16, ответ"),
        ("result", "driver", "service", "data", "BVSTK_OK; буфер: 512 байтов"),
    ], ownership=(("freertos", "FreeRTOS", ("port",)),))
    p.legend_note = "IRQ и завершение DMA могут прийти в разном порядке. Показана проверка результата."

    p, d = sheet("spi/services/read.svg", "Завершение чтения блока 32 сервисом SD",
                 ["src/services/sd/bvstk_sd_service.c",
                  "src/ports/freertos-xilinx/storage/sd-pl/bvstk_sd_pl.c"])
    sequence(d, [
        ("fatfs", "FatFs", "", "L3", "spi/services/storage.svg#fatfs", .7),
        ("port", "bvstk_sd_pl", "LBA 32", "L1", "spi/services/storage.svg#port", .95),
        ("service", "bvstk_sd_service", "", "L3", "spi/drivers/master/receive.svg#service", 1.2),
        ("driver", "bvstk_sd_master", "", "L3", "spi/drivers/master/receive.svg#driver", 1.1),
    ], [
        ("receive", "driver", "service", "data", "BVSTK_OK; буфер: DE AD BE EF …"),
        ("report", "service", "service", "call", "last: LBA 32, BVSTK_OK"),
        ("unlock", "service", "service", "call", "Освободить mutex"),
        ("return", "service", "port", "call", "BVSTK_OK"),
        ("sector", "port", "fatfs", "data", "Сектор доступен в буфере"),
    ], ownership=(("freertos", "FreeRTOS", ("fatfs", "port")),))
