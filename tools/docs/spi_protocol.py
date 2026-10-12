"""Introductory SPI illustrations, independent of BVSTK's RTL capabilities."""

from protocol_drawing import ProtocolFigure, clock_cycles, data_bits

ADI_INTRO = "https://www.analog.com/en/resources/analog-dialogue/articles/introduction-to-spi-interface.html"
ADI_TIMING = "https://www.analog.com/en/resources/app-notes/an-1248.html"
MICROCHIP_SPI = "https://ww1.microchip.com/downloads/en/AppNotes/01496A.pdf"


def interface(d):
    p = d.p
    p.node("master", 60, 260, 330, 430, "Ведущий", layer="L0")
    p.node("slave", 1180, 260, 360, 430, "Ведомое устройство", layer="L0")
    d.wire(390, 1180, 365, "SCK", "clock")
    d.wire(390, 1180, 465, "MOSI")
    d.wire(390, 1180, 565, "MISO", reverse=True)
    d.wire(390, 1180, 665, "CS", "select")


def transaction(d):
    start, period, end = 300, 138, 1490
    bits_out = [int(bit) for bit in f"{0x7B:08b}"]
    bits_in = [int(bit) for bit in f"{0x34:08b}"]
    d.wave("CS", 270, [(240, 1), (270, 1), (270, 0), (1450, 0), (1450, 1), (end, 1)])
    clock = clock_cycles(start, 8, period, 42, 105, end=end)
    d.wave("SCK", 415, [(240, 0), *clock])
    d.wave("MOSI", 590, [(240, bits_out[0]), *data_bits(start, bits_out, period, end, 105-period)], color=d.blue)
    d.wave("MISO", 755, data_bits(start, bits_in, period, 1450, 105-period), color=d.blue)
    d.high_z(240, start, 751)
    d.high_z(1450, end, 751)
    for index in range(8):
        sample = start + index * period + 42
        d.text(sample, 387, f"D{7-index}", 24, anchor="middle", color=d.ink)
        for y, bits in ((590, bits_out), (755, bits_in)):
            d.sample(sample, 407, y + (1-bits[index])*46)
            d.text(sample+24, y+95, str(bits[index]), 23, anchor="middle", color=d.blue)
    d.text(240, 955, "MOSI: 0x7B", 27, bold=True, color=d.blue)
    d.text(660, 955, "MISO: 0x34", 27, bold=True, color=d.blue)
    d.el(d.p.notes, "circle", cx=1140, cy=947, r=5.5, fill=d.blue)
    d.text(1160, 955, "Приём бита", 24)


def modes(d):
    first, half, end = 610, 110, 1310
    for mode, (polarity, phase) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
        y = 265 + mode * 240
        d.text(60, y+20, f"Режим {mode}", 28, bold=True)
        d.text(60, y+62, f"CPOL = {polarity}", 23, color=d.ink)
        d.text(60, y+98, f"CPHA = {phase}", 23, color=d.ink)
        d.wave("SCK", y, clock_cycles(480, 3, 2*half, first-480,
               first-480+half, idle=polarity, end=end), label_x=425)
        d.text(425, y+135, "Данные", 25, bold=True, anchor="end", color=d.blue)
        boundaries = [480, first+half, first+3*half, first+5*half] if not phase else [first, first+2*half, first+4*half, first+6*half]
        if phase:
            d.bus(480, first, y+100, "…", muted=True)
        for bit in range(3):
            d.bus(boundaries[bit], boundaries[bit+1], y+100, f"D{7-bit}")
            sample = first + (2*bit+phase)*half
            d.sample(sample, y-18, y+154)
        if not phase:
            d.bus(boundaries[-1], end, y+100, "…", muted=True)
        if mode != 3:
            d.line(((60, y+200), (1540, y+200)), color="#dbe3e9", width=1.5)
    d.el(d.p.notes, "circle", cx=490, cy=1240, r=5.5, fill=d.blue)
    d.text(510, 1248, "Приём бита", 24)


def transfers(d):
    x, width, end = 290, 390, 1460
    for index, read in enumerate((False, True)):
        y = 245 + index * 590
        d.heading(y, "Чтение" if read else "Запись")
        d.wave("CS", y+50, [(240, 1), (265, 1), (265, 0), (1490, 0), (1490, 1), (1520, 1)])
        clock = clock_cycles(x, 24, width/8, 12, 36, end=1520)
        d.wave("SCK", y+142, [(240, 0), *clock])
        d.text(205, y+273, "MOSI", 25, bold=True, anchor="end", color=d.blue)
        d.text(205, y+383, "MISO", 25, bold=True, anchor="end", color=d.blue)
        d.cell(x, y+230, width-12, "READ" if read else "WRITE", "Команда")
        d.cell(x+width, y+230, width-12, "0x10", "Регистр")
        d.cell(x+2*width, y+230, width, "Заполнитель" if read else "0x7B",
               "8 бит" if read else "Значение", muted=read)
        if read:
            d.cell(x, y+340, 2*width-12, "Не используется", muted=True)
            d.cell(x+2*width, y+340, width, "0x34", "Значение")
        else:
            d.cell(x, y+340, 3*width, "Не используется", muted=True)
        d.bracket(x, end, y+465, "24 такта SCK")


def generate_spi_protocol(page, element, palette, arrows):
    sheets = (
        ("interface", "SPI: линии и участники", "Четыре сигнала. Выбор устройства активным нулём.", interface),
        ("transaction", "SPI: два байта за восемь тактов", "Режим 0. Старший бит передаётся первым. Время слева направо.", transaction),
        ("modes", "SPI: четыре режима тактирования", "CPOL задаёт уровень покоя. CPHA выбирает фронт приёма.", modes),
        ("transfers", "SPI: чтение и запись регистра", "Условное устройство: байт команды, байт адреса, байт данных.", transfers),
    )
    for name, title, subtitle, draw in sheets:
        p = page(f"spi/protocol/{name}.svg", title, subtitle,
                 (ADI_INTRO, ADI_TIMING, MICROCHIP_SPI))
        draw(ProtocolFigure(p, element, palette, arrows))
