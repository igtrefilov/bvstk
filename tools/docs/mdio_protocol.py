"""Clause 22 wire format and timing, separate from the BVSTK MDIO cores."""

from protocol_drawing import ProtocolFigure, clock_cycles

ALTERA_FRAME = "https://docs.altera.com/r/docs/683402/25.3/triple-speed-ethernet-ip-user-guide/mdio-frame-format?contentId=ADlhHNZTvR9r2gR_oCxdsg"
TI_PHY = "https://www.ti.com/lit/ds/symlink/dp83867ir.pdf"
AMD_TRANSACTIONS = "https://docs.amd.com/r/en-US/pg051-tri-mode-eth-mac/MDIO-Transactions"
AMD_BUS = "https://docs.amd.com/r/en-US/pg051-tri-mode-eth-mac/MDIO-Bus-System"


def interface(d):
    p = d.p
    p.node("master", 60, 270, 330, 340, "Ведущий", layer="L0")
    p.node("phy", 1180, 270, 360, 340, "PHY", "Адрес 1\nРегистры 0…31", "L0")
    path = d.line(((390, 400), (1180, 400)), color=d.blue, width=3.5)
    path.set("marker-start", "url(#data-arrow)")
    path.set("marker-end", "url(#data-arrow)")
    d.text(590, 378, "MDIO", 26, bold=True, anchor="middle", color=d.blue)
    d.wire(390, 1180, 545, "MDC", "clock")
    d.text(900, 230, "VDD", 25, bold=True, anchor="middle")
    d.line(((900, 250), (900, 280)))
    d.el(p.edges, "rect", x=888, y=280, width=24, height=54,
         fill="white", stroke=d.ink, stroke_width=2.5)
    d.line(((900, 334), (900, 400)))
    d.el(p.edges, "circle", cx=900, cy=400, r=5, fill=d.blue)
    d.text(945, 315, "Подтяжка", 23, color=d.ink)


def frame(d):
    labels = ("PRE", "ST", "OP", "PHY", "REG", "TA", "DATA")
    widths = (245, 85, 95, 180, 180, 125, 360)
    counts = ("32", "2", "2", "5", "5", "2", "16 бит")
    for row, read in enumerate((False, True)):
        y, x, positions = 245+row*410, 140, []
        d.heading(y, "Чтение" if read else "Запись")
        values = ("32 × 1", "01", "10" if read else "01", "00001", "00100",
                  "Z 0" if read else "1 0", "0x0120")
        for label, width, count, value in zip(labels, widths, counts, values):
            d.text(x+width/2, y+73, label, 24, bold=True, anchor="middle", color=d.ink)
            d.cell(x, y+100, width, value)
            d.text(x+width/2, y+220, count, 22, anchor="middle", color=d.ink)
            positions.append((x, x+width))
            x += width+12
        if read:
            d.bracket(positions[0][0], positions[4][1], y+266, "Ведущий")
            d.bracket(sum(positions[5])/2, positions[6][1], y+266, "PHY", color=d.blue)
        else:
            d.bracket(positions[0][0], positions[-1][1], y+266, "Ведущий")


def turnaround(d):
    x, period, count = 300, 220, 5
    for index, label in enumerate(("REG₀", "TA₁", "TA₂", "D15", "D14")):
        xx = x+period*index
        d.text(xx+period/2, 265, label, 26, bold=True, anchor="middle", color=d.ink)
        d.line(((xx, 287), (xx, 638)), color="#dbe3e9", width=1.5, dash="5 6")
    d.wave("MDC", 315, clock_cycles(x, count, period, 75, 185), label_x=225)
    d.text(225, 464, "Ведущий", 25, bold=True, anchor="end")
    d.text(225, 574, "PHY", 25, bold=True, anchor="end")
    d.bus(x, x+period, 430, "0")
    d.high_z(x+period, x+count*period, 430)
    d.high_z(x, x+2*period, 540)
    for index, value in enumerate(("0", "D15", "D14"), 2):
        d.bus(x+index*period, x+(index+1)*period, 540, value)
    d.bracket(x+period, x+3*period, 645, "TA")
    d.text(800, 755, "Z — отключённый выход", 25, anchor="middle")

    d.heading(865, "Выдача PHY и приём ведущим")
    d.wave("MDC", 930, [(300, 0), (500, 0), (500, 1), (800, 1), (800, 0),
                        (1100, 0), (1100, 1), (1400, 1), (1400, 0), (1450, 0)], label_x=225)
    d.text(225, 1080, "MDIO", 25, bold=True, anchor="end", color=d.blue)
    d.bus(300, 550, 1046, "Бит n−1")
    d.bus(550, 1150, 1046, "Бит n")
    d.bus(1150, 1450, 1046, "Бит n+1")
    for xx in (500, 1100):
        d.line(((xx, 910), (xx, 1150)), color="#adc6e2", width=1.5, dash="5 6")
    d.line(((550, 1028), (550, 1150)), color="#adc6e2", width=1.5, dash="5 6")
    d.bracket(500, 550, 1150, "tCO")
    d.el(d.p.notes, "circle", cx=1100, cy=1100, r=5.5, fill=d.blue)
    d.text(1100, 1187, "Приём бита n", 24, anchor="middle")


def generate_mdio_protocol(page, element, palette, arrows):
    sheets = (
        ("interface", "MDIO: линии и участники", "Clause 22. Доступ к 16-битным регистрам PHY.", interface),
        ("frame", "MDIO: кадры Clause 22", "PHY с адресом 1, регистр 4. Поля идут слева направо.", frame),
        ("turnaround", "MDIO: передача линии при чтении", "Сверху — кто задаёт биты. Снизу — задержка выдачи PHY.", turnaround),
    )
    for name, title, subtitle, draw in sheets:
        p = page(f"mdio/protocol/{name}.svg", title, subtitle,
                 (ALTERA_FRAME, TI_PHY, AMD_TRANSACTIONS, AMD_BUS))
        draw(ProtocolFigure(p, element, palette, arrows))
