"""Policy commands, snapshots and examples in the established DCP2 style."""
from dcp2_fields import FieldMap
from dcp2_introduction import PAYLOAD
from dcp2_examples import FIGURES, Sequence, wire, request, response, number, named, group

SOURCES = (
    "src/protocols/dcp2/bvstk_dcp2_control.c",
    "src/services/i2c/bvstk_i2c_policy.c",
    "src/services/mdio/bvstk_mdio_service.c",
)


def requests():
    p = FieldMap("policy-requests", "Параметры управления политикой в теле запроса", SOURCES)
    p.strip(90, "I²C или MDIO", "opcode")
    p.panel("body", 295, 800, "body запроса")
    p.reveal(195, 295)
    for y, label, fields in (
        (405, "POLICY_SET = 0x02", ("addr7 или phy", "mode")),
        (565, "POLICY_GET = 0x03", ("addr7 или phy",)),
        (745, "RULE_ADD = 0x04\nRULE_DELETE = 0x05", ("addr7 или phy", "list", "rule")),
        (960, "POLICY_CLEAR = 0x06", ("addr7 или phy", "list")),
    ):
        p.text(p.notes, 105, y+34, label, 25, bold=True)
        p.fields(fields, 550, y, 1140)
    p.finish(1185)


def snapshot():
    p = FieldMap("policy-snapshot", "Режим, списки правил и формат одной записи POLICY_GET", SOURCES)
    p.strip(90, "I²C или MDIO", "0x83 (RESP)")
    p.panel("body", 295, 410, "body успешного ответа POLICY_GET")
    p.reveal(195, 295)
    p.fields(("status", "mode", "whitelist_count", "blacklist_count"), 110, 390)
    p.box("white", 110, 545, 740, 100, "whitelist[]", PAYLOAD, 28)
    p.box("black", 880, 545, 810, 100, "blacklist[]", PAYLOAD, 28)
    p.panel("rule", 900, 345, "Формат одного правила")
    p.reveal(645, 900, 480)
    p.text(p.notes, 110, 1020, "I²C", 28, bold=True)
    p.fields(("reg", "val"), 440, 970)
    p.text(p.notes, 110, 1160, "MDIO", 28, bold=True)
    p.fields(("reg", "val", "mask"), 440, 1110)
    p.finish(1335)


def operation():
    p = Sequence("policy-operation", "Изменение правила и последующее чтение политики",
                 extra="Сервис устройств")
    p.sources.extend(SOURCES)
    p.send(0, 1, "POLICY_RULE_ADD")
    p.send(1, 2, "Проверить параметры", "call")
    p.state(2, "Обновить список в памяти", layer="L3")
    p.send(2, 1, "Результат", "call")
    p.send(1, 0, "Ответ: status = 0")
    p.send(0, 1, "POLICY_GET")
    p.send(1, 2, "Получить снимок политики", "call")
    p.send(2, 1, "Режим и оба списка")
    p.send(1, 0, "Ответ POLICY_GET")
    p.finish()


def examples():
    wire("policy-i2c", "Добавление правила I²C и чтение результата",
         request("I²C POLICY_RULE_ADD", 2, 4, 0x0701,
                 [number("addr7", 0x36, 1), number("list", 0, 1),
                  number("reg", 0x10, 1), number("val", 3, 1)]),
         response("Правило добавлено", 2, 4, 0x0701),
         request("I²C POLICY_GET", 2, 3, 0x0702, [number("addr7", 0x36, 1)]),
         response("Политика устройства", 2, 3, 0x0702,
                  [number("mode", 0, 1), number("whitelist_count", 1, 2),
                   number("blacklist_count", 0, 2), number("reg", 0x10, 1),
                   number("val", 3, 1)]))
    wire("policy-mdio", "Добавление правила MDIO с маской и чтение нормализованного значения",
         request("MDIO POLICY_RULE_ADD", 3, 4, 0x0703,
                 [number("phy", 1, 1), number("list", 1, 1), number("reg", 0, 1),
                  number("val", 0xFFFF, 2), number("mask", 0x8000, 2)]),
         response("Правило добавлено", 3, 4, 0x0703),
         request("MDIO POLICY_GET", 3, 3, 0x0704, [number("phy", 1, 1)]),
         response("Политика PHY", 3, 3, 0x0704,
                  [number("mode", 1, 1), number("whitelist_count", 0, 2),
                   number("blacklist_count", 1, 2), number("reg", 0, 1),
                   number("val", 0x8000, 2), number("mask", 0x8000, 2)]))


def profile_target(subsystem="i2c", device="pcf8563"):
    return [number("policy_version", 1, 1)] + named(subsystem, "subsystem") + named(device, "device")


def profile_rule(resource, value, mask):
    return named(resource, "resource") + [number("value", value, 8), number("mask", mask, 8)]


def profile_page(token, mode, white, black, first, rules, following):
    fields = [number("policy_version", 1, 1), number("mode", mode, 1),
              number("snapshot_id", token, 4), number("whitelist_count", white, 2),
              number("blacklist_count", black, 2), number("first", first, 2),
              number("count", len(rules), 2), number("next", following, 2)]
    if rules:
        records = [field for rule in rules for field in rule]
        fields += [group("rules", records, f"Правила ({len(rules)})")]
    return fields


def profile_fields():
    sources = ("src/protocols/dcp2/bvstk_dcp2_policy.c",)
    p = FieldMap("policy-id-requests", "Параметры POLICY по ID профиля", sources)
    p.strip(90, "0x09", "opcode")
    p.panel("target", 285, 190, "Начало тела каждого запроса")
    p.fields(("policy_version", "subsystem_len", "subsystem", "device_len", "device"), 100, 365)
    p.panel("parameters", 550, 1050, "Поля после адресации профиля")
    for y, label, fields in (
        (660, "SET = 0x02", ("mode",)),
        (820, "GET = 0x03", ("snapshot_id", "first")),
        (980, "ADD = 0x04\nDELETE = 0x05", ("list", "resource_len", "resource", "value:u64", "mask:u64")),
        (1185, "CLEAR = 0x06", ("list",)),
    ):
        p.text(p.notes, 105, y + 30, label, 25, bold=True)
        p.fields(fields, 520, y, 1170)
    p.text(p.notes, 105, 1415, "SAVE = 0x07", 25, bold=True)
    p.text(p.notes, 520, 1415, "После device тело запроса завершено", 24)
    p.finish(1680)

    p = FieldMap("policy-id-page", "Страница согласованного снимка POLICY_GET", sources)
    p.strip(90, "0x09", "0x83 (RESP)")
    p.panel("page", 285, 450, "body ответа POLICY_GET")
    p.fields(("status", "policy_version", "mode", "snapshot_id"), 100, 385)
    p.fields(("whitelist_count", "blacklist_count", "first", "count", "next"), 100, 510)
    p.box("records", 100, 635, 1580, 80, "count целых записей rules", PAYLOAD, 25)
    p.panel("rule", 840, 240, "Одна запись rules")
    p.fields(("resource_len:u8", "resource", "value:u64", "mask:u64"), 100, 940)
    p.finish(1160)


def profile_examples():
    target = profile_target()
    rule = profile_rule("reg_01", 42, 255)
    wire("policy-id-set", "Выбор BLACKLIST по ID профиля",
         request("POLICY_SET", 9, 2, 0xB01, target + [number("mode", 1, 1)]),
         response("Режим изменён", 9, 2, 0xB01))
    wire("policy-id-add", "Добавление правила в BLACKLIST по ID ресурса",
         request("POLICY_RULE_ADD", 9, 4, 0xB02, target + [number("list", 1, 1)] + rule),
         response("Правило добавлено", 9, 4, 0xB02))
    wire("policy-id-get", "Чтение одного правила из согласованного снимка",
         request("POLICY_GET", 9, 3, 0xB03,
                 target + [number("snapshot_id", 0, 4), number("first", 0, 2)]),
         response("Снимок с одним правилом", 9, 3, 0xB03,
                  profile_page(1, 1, 0, 1, 0, [rule], 0xFFFF)))
    wire("policy-id-delete", "Удаление того же правила",
         request("POLICY_RULE_DELETE", 9, 5, 0xB04, target + [number("list", 1, 1)] + rule),
         response("Правило удалено", 9, 5, 0xB04))
    wire("policy-id-save", "Явное сохранение профиля по ID",
         request("POLICY_SAVE", 9, 7, 0xB05, target),
         response("Профиль сохранён", 9, 7, 0xB05))

    target = profile_target("i2c", "sensor")
    rules = [profile_rule("reg_00", value, 255) for value in range(200)]
    wire("policy-id-pages", "Две страницы политики из двухсот правил",
         request("Начальный POLICY_GET", 9, 3, 0xC01,
                 target + [number("snapshot_id", 0, 4), number("first", 0, 2)]),
         response("Первая страница", 9, 3, 0xC01, profile_page(1, 0, 200, 0, 0, rules[:177], 177)),
         request("Продолжение POLICY_GET", 9, 3, 0xC02,
                 target + [number("snapshot_id", 1, 4), number("first", 177, 2)]),
         response("Последняя страница", 9, 3, 0xC02, profile_page(1, 0, 200, 0, 177, rules[177:], 0xFFFF)))
    wire("policy-id-stale", "Истечение снимка и новое начальное чтение",
         request("Прежний снимок после таймаута", 9, 3, 0xC03,
                 target + [number("snapshot_id", 1, 4), number("first", 177, 2)]),
         response("ERR_POLICY_STALE_SNAPSHOT", 9, 3, 0xC03, status=0x301),
         request("Новый начальный POLICY_GET", 9, 3, 0xC04,
                 target + [number("snapshot_id", 0, 4), number("first", 0, 2)]),
         response("Первая страница нового снимка", 9, 3, 0xC04, profile_page(2, 0, 200, 0, 0, rules[:177], 177)))
    p = Sequence("policy-id-pagination", "Срок жизни снимка POLICY и повторное чтение")
    p.send(0, 1, "GET, snapshot_id = 0, first = 0")
    p.send(1, 0, "Снимок 1, count = 177, next = 177")
    p.send(0, 1, "GET, snapshot_id = 1, first = 177")
    p.send(1, 0, "Снимок 1, count = 23, next = 0xFFFF")
    p.state(0, "Пауза больше 30 секунд")
    p.send(0, 1, "GET, snapshot_id = 1, first = 177")
    p.send(1, 0, "ERR_POLICY_STALE_SNAPSHOT")
    p.send(0, 1, "GET, snapshot_id = 0, first = 0")
    p.send(1, 0, "Новый снимок 2, count = 177, next = 177")
    p.state(0, "Сборка политики со страницы 0")
    p.finish()


def generate():
    before = len(FIGURES)
    requests()
    snapshot()
    operation()
    examples()
    profile_fields()
    profile_examples()
    return 4 + len(FIGURES) - before
