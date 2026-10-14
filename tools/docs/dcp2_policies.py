"""Policy commands, snapshots and examples in the established DCP2 style."""
from dcp2_fields import FieldMap
from dcp2_introduction import PAYLOAD
from dcp2_examples import Sequence, wire, request, response, number

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


def generate():
    requests()
    snapshot()
    operation()
    examples()
    return 5
