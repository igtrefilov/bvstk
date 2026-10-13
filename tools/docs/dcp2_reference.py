"""Explanatory byte layouts and exchange diagrams for the DCP2 reference."""
from dcp2_introduction import PacketFigure, HEADER, PAYLOAD, NETWORK, BLUE, MUTED
from generate_pl_subsystem import el

EVENT = "#c65b4e"
RELATION = "#788794"


class ProtocolFigure(PacketFigure):
    def __init__(self, name, title, description, sources=()):
        super().__init__(name, title, description)
        self.sources.extend(sources)
        self.protocol_arrows = set()

    def box(self, key, x, y, width, height, label, fill=NETWORK, size=25):
        group = self.cell(key, x, y, width, height, fill)
        count = label.count("\n") + 1
        baseline = y + height/2 - (count-1)*size*1.38/2 + size*.34
        self.text(group, x+width/2, baseline, label, size, anchor="middle")
        return group

    def arrow(self, x1, y1, x2, y2, label="", kind="data", label_xy=None, size=25):
        color = {"data": BLUE, "irq": EVENT, "call": RELATION}[kind]
        self.protocol_arrows.add(kind)
        el(self.edges, "path", d=f"M{x1},{y1} L{x2},{y2}", fill="none",
           stroke=color, stroke_width=3,
           stroke_dasharray="9 6" if kind == "irq" else None,
           marker_end=f"url(#{kind}-arrow)")
        self.min_y = min(self.min_y, y1, y2)
        self.max_y = max(self.max_y, y1, y2)
        if label:
            x, y = label_xy or ((x1+x2)/2, (y1+y2)/2-20)
            self.text(self.notes, x, y, label, size, color=color, anchor="middle")

    def actors(self, bottom, client="Клиент", server="Сервер DCP2"):
        for x, label in ((420, client), (1380, server)):
            self.text(self.notes, x, 85, label, 30, bold=True, anchor="middle")
            el(self.under, "path", d=f"M{x},130 V{bottom}", fill="none",
               stroke="#bdc8d0", stroke_width=2, stroke_dasharray="5 8")
        self.max_y = max(self.max_y, bottom)

    def legend(self, y, time=False):
        x = 60
        for kind, label in (("data", "Запрос или ответ"), ("irq", "Событие"),
                            ("call", "Зависимость формата")):
            if kind not in self.protocol_arrows:
                continue
            self.arrow(x, y-7, x+65, y-7, kind=kind)
            self.text(self.notes, x+85, y, label, 21, color=MUTED)
            x += 465
        if time:
            self.text(self.notes, 1740, y, "Время идёт сверху вниз", 21,
                      color=MUTED, anchor="end")


def operation():
    p = ProtocolFigure("operation", "Биты поля op",
                       "В поле op бит 7 — RESP, бит 6 — EVENT, биты от 5 до 0 "
                       "содержат код операции. Все поля относятся к полезной нагрузке.")
    x, unit, y = 180, 180, 150
    for i in range(8):
        p.text(p.notes, x+(i+.5)*unit, y-35, str(7-i), 25,
               color=MUTED, anchor="middle")
    p.box("response", x, y, unit, 115, "RESP", PAYLOAD, 27)
    p.box("event", x+unit, y, unit, 115, "EVENT", PAYLOAD, 27)
    p.box("opcode", x+2*unit, y, 6*unit, 115, "opcode: код операции", PAYLOAD, 28)
    p.key(60, 345, PAYLOAD, "Поле полезной нагрузки")
    p.text(p.notes, 980, 345, "Вверху — номера битов одного байта", 22, color=MUTED)
    p.save()


def message_types():
    p = ProtocolFigure("message-types", "Тело запроса, ответа и события",
                       "Запрос содержит N байт параметров, ответ — status и M байт "
                       "результата, событие — E байт данных. Только ответ имеет общий status.")
    p.bracket("header", 320, 85, 260, "8 байт", BLUE)
    p.bracket("payload", 580, 85, 1160, "dcp_len байт", BLUE)
    for key, y, name, op, seq, body, length in (
        ("request", 130, "Запрос", "opcode", "seq = q", "Параметры: N байт", "4 + N"),
        ("response", 365, "Ответ", "RESP | opcode", "seq = q", "Данные: M байт", "6 + M"),
        ("event", 600, "Событие", "EVENT | opcode", "seq = 0", "Данные события: E байт", "4 + E"),
    ):
        p.text(p.notes, 60, y+76, name, 27, bold=True)
        p.box(key+"-header", 320, y, 260, 125, "Заголовок", HEADER)
        p.box(key+"-service", 580, y, 110, 125, "srv", PAYLOAD)
        p.box(key+"-operation", 690, y, 300, 125, op, PAYLOAD, 24)
        p.box(key+"-sequence", 990, y, 190, 125, seq, PAYLOAD, 24)
        if key == "response":
            p.box(key+"-status", 1180, y, 170, 125, "status\n2 байта", PAYLOAD, 23)
            p.box(key+"-body", 1350, y, 390, 125, body, PAYLOAD, 23)
        else:
            p.box(key+"-body", 1180, y, 560, 125, body, PAYLOAD, 25)
        p.text(p.notes, 1740, y+173, "dcp_len = "+length, 24,
               anchor="end", color=BLUE)
    p.key(60, 850, HEADER, "Заголовок DCP2")
    p.key(640, 850, PAYLOAD, "Полезная нагрузка DCP2")
    p.save()


def exchange():
    p = ProtocolFigure("exchange", "Запрос, событие и ответ в одном соединении",
                       "Клиент ждёт ответ с номером q. Событие с seq=0 "
                       "обрабатывается отдельно и не завершает ожидание ответа.")
    p.actors(815)
    p.arrow(420, 210, 1380, 210, "Запрос: seq = q")
    p.box("waiting", 215, 265, 410, 90, "Ожидание ответа q")
    p.arrow(1380, 425, 420, 425, "Событие: seq = 0", "irq")
    p.box("event-handler", 150, 470, 540, 100, "Обработать событие\nПродолжить ожидание")
    p.arrow(1380, 655, 420, 655, "Ответ: seq = q")
    p.box("complete", 145, 710, 550, 100, "Проверить ответ\nЗавершить запрос q")
    p.legend(905, time=True)
    p.save()


def command_body():
    p = ProtocolFigure("command-body", "Выбор формата команды",
                       "Пара srv и opcode выбирает формат параметров запроса "
                       "и данных ответа. Номер seq сохраняется отдельно для корреляции.")
    p.box("srv", 400, 80, 400, 105, "srv", PAYLOAD, 29)
    p.box("opcode", 1000, 80, 400, 105, "opcode", PAYLOAD, 29)
    p.box("format", 580, 320, 640, 110, "Формат команды", NETWORK, 28)
    p.arrow(600, 185, 760, 320, kind="call")
    p.arrow(1200, 185, 1040, 320, kind="call")
    p.cell("request-body", 140, 570, 600, 210, "white")
    p.cell("response-body", 980, 570, 660, 210, "white")
    p.text(p.notes, 440, 617, "body запроса", 27, anchor="middle")
    p.text(p.notes, 1310, 617, "body ответа", 27, anchor="middle")
    p.box("parameters", 160, 655, 560, 105, "Параметры", PAYLOAD, 27)
    p.box("status", 1000, 655, 190, 105, "status\n2 байта", PAYLOAD, 24)
    p.box("result", 1190, 655, 430, 105, "Данные результата", PAYLOAD, 25)
    p.arrow(760, 430, 440, 570, kind="call")
    p.arrow(1040, 430, 1310, 570, kind="call")
    p.legend(875)
    p.save()


def subscription():
    p = ProtocolFigure("subscription", "Жизненный цикл подписки NOTIFY или STREAM",
                       "Запрос SUBSCRIBE получает обычный ответ; далее приходят "
                       "события с seq=0. UNSUBSCRIBE отменяет подписку.")
    p.actors(950)
    for y, a, b, label, kind in (
        (215, 420, 1380, "SUBSCRIBE: seq = q", "data"),
        (360, 1380, 420, "Ответ: seq = q, status = 0", "data"),
        (540, 1380, 420, "События: seq = 0", "irq"),
        (750, 420, 1380, "UNSUBSCRIBE: seq = r", "data"),
        (900, 1380, 420, "Ответ: seq = r, status = 0", "data"),
    ):
        p.arrow(a, y, b, y, label, kind)
    p.legend(1030, time=True)
    p.save()


def fs_transfer():
    p = ProtocolFigure("fs-transfer", "Открытие, чтение и закрытие потока FS",
                       "OPEN связывает open_id клиента с дескриптором H сервера. "
                       "READ использует H и последовательные номера блоков. "
                       "EOF завершает содержимое, CLOSE освобождает поток.",
                       ("src/protocols/dcp2/bvstk_dcp2_fs.c",))
    p.actors(1190)
    p.arrow(420, 210, 1380, 210, "OPEN: open_id, путь, вид потока")
    p.arrow(1380, 370, 420, 370, "handle = H, block_size = B")
    el(p.under, "rect", x=220, y=465, width=1360, height=365,
       rx=12, fill="none", stroke=RELATION, stroke_width=2)
    p.text(p.notes, 900, 515, "Блоки k = 0, 1, … до EOF", 25,
           bold=True, anchor="middle")
    p.arrow(420, 620, 1380, 620, "READ: handle = H, block_no = k")
    p.arrow(1380, 775, 420, 775, "Блок k: data, признак EOF")
    p.arrow(420, 990, 1380, 990, "CLOSE: handle = H")
    p.arrow(1380, 1150, 420, 1150, "status = 0")
    p.legend(1270, time=True)
    p.save()


def fs_blocks():
    p = ProtocolFigure("fs-blocks", "Запись TREE на границе блоков FS",
                       "Из каждого ответа FS_READ берутся только байты data. "
                       "Они образуют непрерывный поток: запись A начинается "
                       "в блоке k и заканчивается в блоке k+1.",
                       ("src/services/fs/bvstk_fs_reader.c",))
    for key, x, number in (("first", 120, "k"), ("second", 1020, "k+1")):
        p.text(p.notes, x+330, 110, f"Ответ READ({number})", 28,
               bold=True, anchor="middle")
        p.box(key+"-metadata", x, 155, 340, 135, "status\nМетаданные блока", PAYLOAD, 23)
        p.box(key+"-data", x+340, 155, 320, 135, "data", PAYLOAD, 29)
    p.arrow(620, 290, 540, 505)
    p.arrow(1520, 290, 1260, 505)
    p.box("data-k", 180, 505, 720, 115, "data блока k", PAYLOAD, 27)
    p.box("data-next", 900, 505, 720, 115, "data блока k+1", PAYLOAD, 27)
    # A projection of the transport boundary stops at the complete record.
    # It does not divide that record into separate application objects.
    el(p.under, "path", d="M900,620 V760", fill="none", stroke=RELATION,
       stroke_width=2, stroke_dasharray="5 8")
    p.text(p.notes, 60, 715, "Записи TREE", 26, bold=True)
    p.box("entry-a", 180, 760, 980, 125, "Запись A: entry_len байт", PAYLOAD, 28)
    p.box("entry-b", 1160, 760, 460, 125, "Запись B", PAYLOAD, 28)
    p.key(60, 985, PAYLOAD, "Байты полезной нагрузки DCP2")
    p.arrow(720, 978, 785, 978)
    p.text(p.notes, 805, 985, "Извлечь data", 21, color=MUTED)
    p.text(p.notes, 1300, 985, "Размеры условные", 21, color=MUTED)
    p.save()


def cache_sync():
    p = ProtocolFigure("cache-sync", "Снимок CACHE и последующие изменения",
                       "SUBSCRIBE фиксирует снимок S с ревизией R. GET читает "
                       "его оставшиеся страницы. Только после завершения "
                       "снимка сервер выдаёт события R+1, R+2 из журнала.",
                       ("src/protocols/dcp2/bvstk_dcp2_cache.c",))
    p.actors(1270)
    p.arrow(420, 200, 1380, 200, "CACHE_SUBSCRIBE")
    p.box("snapshot", 1080, 250, 600, 105,
          "Снимок S: revision = R\nПозиция журнала сохранена", size=24)
    p.arrow(1380, 425, 420, 425, "Первая страница S: R, next")
    el(p.under, "rect", x=230, y=505, width=1340, height=335,
       rx=12, fill="none", stroke=RELATION, stroke_width=2)
    p.text(p.notes, 900, 555, "Остальные страницы: пока next ≠ 0xFFFF", 25,
           bold=True, anchor="middle")
    p.arrow(420, 670, 1380, 670, "CACHE_GET: snapshot_id = S, first = next")
    p.arrow(1380, 800, 420, 800, "Страница S: та же R, новый next")
    p.box("snapshot-complete", 170, 910, 500, 90, "Снимок S собран")
    p.arrow(1380, 1100, 420, 1100, "revision = R+1, seq = 0", "irq")
    p.arrow(1380, 1250, 420, 1250, "revision = R+2, seq = 0", "irq")
    p.legend(1350, time=True)
    p.save()


def cache_recovery():
    p = ProtocolFigure("cache-recovery", "Повторная синхронизация CACHE",
                       "После RESYNC_REQUIRED клиент отмечает свою копию как "
                       "несогласованную. Сервер приостанавливает подписку. "
                       "Новый SUBSCRIBE и полный снимок позволяют возобновить "
                       "последовательное применение изменений.",
                       ("src/protocols/dcp2/bvstk_dcp2_cache.c",))
    p.actors(1190)
    p.arrow(1380, 205, 420, 205, "RESYNC_REQUIRED", "irq")
    p.box("invalid", 145, 285, 550, 110, "Копия требует обновления")
    p.box("paused", 1080, 285, 600, 110, "Подписка приостановлена")
    p.arrow(420, 540, 1380, 540, "Новый CACHE_SUBSCRIBE")
    p.arrow(1380, 700, 420, 700, "Первая страница: revision = R′")
    p.box("remaining-pages", 265, 785, 1270, 110,
          "Получение остальных страниц снимка", size=27)
    p.box("ready", 145, 960, 550, 90, "Новая копия собрана")
    p.arrow(1380, 1160, 420, 1160, "revision = R′+1, seq = 0", "irq")
    p.legend(1270, time=True)
    p.save()


def generate():
    figures = (operation, message_types, exchange, command_body,
               subscription, fs_transfer, fs_blocks, cache_sync, cache_recovery)
    for draw in figures:
        draw()
    return len(figures)
