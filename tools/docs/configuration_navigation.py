#!/usr/bin/env python3
"""Generate one standalone JSON navigation note per configuration format.

The notes use native HTML details and Obsidian internal links. Keep them in
an Obsidian sidebar while reading the corresponding article in the main pane.
The generator updates only the two tree notes; it never rewrites the articles.
Use --install-css VAULT to enable the optional compact style in a local vault.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from html import escape
import json
from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs/configuration"
STYLE = Path(__file__).with_suffix(".css")


def array(item=None):
    return [] if item is None else [item]


RESOURCE_PROPERTIES = {
    "width_bits": None, "access": None, "write_mask": None,
    "cache": {"mode": None}, "verify": {"mode": None},
    "refresh": {"on": array(), "period_ms": None},
    "persist_mask": None,
    "read_effect": {"clear_mask": None, "latch_high_mask": None, "latch_low_mask": None},
    "write_effect": {"one_clear_mask": None, "one_set_mask": None,
                     "self_clear_mask": None, "fixed_mask": None, "fixed_value": None},
    "fields": array(),
}
RULE = {"resource": None, "match": {"value": None, "mask": None}}
PROFILE = {
    "schema_version": None, "id": None,
    "device": {"model": None, "vendor": None, "class": None, "label": None},
    "transport": {
        "bus": None, "controller": None, "address": None, "address_bits": None,
        "parent": None, "channel": None,
        "address_spaces": array({"id": None, "address": None, "register_count": None}),
    },
    "protocol": {
        "driver": None,
        "options": {"max_write_value": None, "capture_cache_on_save": None,
                    "autopoll_enabled": None, "autopoll_order": array(),
                    "autopoll_reg_delay_ms": None, "autopoll_cycle_delay_ms": None},
        "transfer": {
            "address_bytes": None, "address_order": None, "address_base": None,
            "read_restart": None,
            "read_sequence": {"mode": None, "span": None},
            "write_sequence": {"mode": None, "span": None},
            "write_cycle_ms": None, "verify_write": None, "cache_after_write": None,
            "host_read": None, "host_current_read": None,
        },
        "switch": {"resource": None, "select_mask": None, "preserve_mask": None,
                   "channels": array({"select": None, "match_mask": None})},
    },
    "resources": array({"id": None, "kind": None, "address": None, "space": None,
                        **RESOURCE_PROPERTIES}),
    "defaults": {"register": RESOURCE_PROPERTIES},
    "policy": {
        "read": {"mode": None, "rules": array(), "inactive_rules": array()},
        "write": {"mode": None, "rules": array(RULE), "inactive_rules": array(RULE)},
    },
    "settings": {"cache_seed": array({"resource": None, "value": None}),
                 "desired": array(), "apply_on_startup": None},
    "observability": {"telemetry": None, "events": None},
    "register_map": None, "view": None,
}
VALUES = {"код": {"label": None, "tone": None}}
CONDITION = {"field": None, "equals": None}
REGISTER_MAP = {
    "schema_version": None, "kind": None, "id": None, "device": None,
    "bus": None, "width_bits": None, "register_count": None,
    "source": {"title": None, "document": None, "revision": None, "url": None},
    "notes": None,
    "registers": array({
        "id": None, "address": None, "space": None, "name": None,
        "label": None, "section": None, "reserved": None, "note": None,
        "fields": array({
            "id": None, "label": None, "lsb": None, "width": None,
            "type": None, "access": None, "reset": None, "values": VALUES,
            "unit": None, "scale": None, "offset": None,
            "ranges": array({"min": None, "max": None, "scale": None, "offset": None}),
            "attributes": array(), "note": None, "write_value": None,
        }),
    }),
    "summary": array({
        "id": None, "label": None, "field": None, "values": VALUES,
        "cases": array({"when": array(CONDITION), "field": None, "values": VALUES,
                        "label": None, "tone": None}),
        "note": None,
    }),
    "identity": array(CONDITION),
}
FORMATS = {
    "profile": ("Профиль", PROFILE, "device-profile.md"),
    "map": ("Карта регистров", REGISTER_MAP, "register-map.md"),
}


# JSON paths are mapped to the existing article headings. Generation validates
# these names, so a renamed or deleted section cannot silently break navigation.
NAVIGATION = {
    "profile": {
        "title": "Дерево профиля",
        "note": "device-profile-tree.md",
        "sections": {
            "$": "Структура профиля",
            "device": "Описание устройства",
            "transport": "Подключение",
            "transport.address_spaces": "Адресные пространства",
            "transport.address_spaces[]": "Описание пространства",
            "protocol": "Обработчик обмена",
            "protocol.options": "Параметры обработчика",
            "protocol.transfer": "Параметры передачи",
            "protocol.transfer.read_sequence": "Последовательность чтения",
            "protocol.transfer.write_sequence": "Последовательность записи",
            "protocol.switch": "Коммутатор",
            "protocol.switch.channels": "Каналы коммутатора",
            "resources": "Ресурсы",
            "resources[]": "Описание регистра",
            "resources[].cache": "Кэш ресурса",
            "resources[].verify": "Проверка записи ресурса",
            "resources[].refresh": "Обновление ресурса",
            "resources[].read_effect": "Эффекты чтения",
            "resources[].write_effect": "Эффекты записи",
            "defaults": "Общие свойства",
            "defaults.register": "Общие свойства регистра",
            "policy": "Политика",
            "policy.read": "Политика чтения",
            "policy.write": "Политика записи",
            "policy.write.rules": "Активные правила",
            "policy.write.rules[]": "Правило записи",
            "policy.write.rules[].match": "Условие значения",
            "policy.write.inactive_rules": "Неактивные правила",
            "settings": "Начальные настройки",
            "settings.cache_seed": "Начальное заполнение кэша",
            "observability": "Наблюдение",
            "register_map": "Карта регистров",
            "view": "Индивидуальная форма"
        }
    },
    "map": {
        "title": "Дерево карты регистров",
        "note": "register-map-tree.md",
        "sections": {
            "$": "Карта регистров",
            "source": "Основание описания",
            "registers": "Регистры",
            "registers[]": "Описание регистра",
            "registers[].fields": "Битовые поля",
            "registers[].fields[]": "Описание битового поля",
            "registers[].fields[].values": "Подписи кодов",
            "registers[].fields[].ranges": "Диапазоны пересчёта",
            "registers[].fields[].attributes": "Атрибуты поля",
            "summary": "Показатели",
            "summary[].cases": "Варианты показателя",
            "summary[].cases[].when": "Условия варианта",
            "identity": "Проверка модели"
        }
    }
}


@dataclass
class Node:
    path: str
    label: str
    parent: Node | None = None
    children: list[Node] = field(default_factory=list)


def make_node(path, label, value, parent=None):
    suffix = " {}" if isinstance(value, dict) else " []" if isinstance(value, list) else ""
    node = Node(path, label + suffix, parent)
    if isinstance(value, dict):
        node.children = [make_node(key if path == "$" else path + "." + key,
                                   key, child, node) for key, child in value.items()]
    elif isinstance(value, list) and value:
        node.children = [make_node(path + "[]", "элемент массива", value[0], node)]
    return node


def flatten(node):
    yield node
    for child in node.children:
        yield from flatten(child)


def target_path(kind, path):
    # Shared field definitions link to the section that actually explains them.
    if kind == "profile":
        if path.startswith("defaults.register."):
            return "resources[]" + path[len("defaults.register"):]
        if path.startswith("policy.write.inactive_rules[]"):
            return path.replace("inactive_rules", "rules", 1)
    elif path.startswith("summary[].values") or path.startswith("summary[].cases[].values"):
        return "registers[].fields[].values" + path.split(".values", 1)[1]
    return path


def target(node, kind, by_path, headings, filename):
    current = by_path[target_path(kind, node.path)]
    while current.path not in headings:
        if current.parent is None:
            raise ValueError(f"No description for {node.path}")
        current = current.parent
    # Obsidian resolves the heading text, not GitHub's hyphenated slug.
    # Keep readable Unicode and encode spaces in the Markdown destination.
    return filename + "#" + headings[current.path].replace(" ", "%20")


def render(node, links):
    href = links[node.path]
    link = (f'<a class="internal-link" data-href="{escape(href.replace("%20", " "), quote=True)}" '
            f'href="{escape(href, quote=True)}">{escape(node.label)}</a>')
    if not node.children:
        return ["<p>" + link + "</p>"]
    # Anonymous entries expand with their array; named branches stay folded
    # until the reader opens them. There is no automatic scroll synchronisation.
    expanded = " open" if node.path == "$" or node.label in ("элемент массива {}", "код {}") else ""
    result = [f"<details{expanded}><summary>{link}</summary>"]
    for child in node.children:
        result += render(child, links)
    result.append("</details>")
    return result


def generate():
    generated = {}
    for kind, (label, specification, filename) in FORMATS.items():
        navigation = NAVIGATION[kind]
        text = (DOCS / filename).read_text()
        all_headings = re.findall(r"^#{1,6} (.+)$", text, re.M)
        if len(all_headings) != len(set(all_headings)):
            raise ValueError(f"Ambiguous headings in {filename}")
        sections = navigation["sections"]
        missing = set(sections.values()) - set(all_headings)
        if missing:
            raise ValueError(f"Missing article headings in {filename}: {sorted(missing)}")
        tree = make_node("$", label, specification)
        nodes = {node.path: node for node in flatten(tree)}
        unknown = set(sections) - set(nodes)
        if unknown:
            raise ValueError(f"Unknown JSON paths in {kind}: {sorted(unknown)}")
        links = {path: target(node, kind, nodes, sections, filename) for path, node in nodes.items()}
        lines = ["# " + navigation["title"], "",
                 f'<div class="bvstk-json-tree" data-json-tree="{kind}">']
        lines += render(tree, links) + ["</div>", ""]
        lines += ["`{}` — объект, `[]` — массив. Стрелка раскрывает ветку; название открывает описание поля.", ""]
        generated[DOCS / navigation["note"]] = "\n".join(lines)
    for path, content in generated.items():
        path.write_text(content)
    return len(generated)


def install_css(vault):
    metadata = vault / ".obsidian"
    if not metadata.is_dir():
        raise ValueError(f"Not an existing Obsidian vault: {vault}")
    snippets = metadata / "snippets"
    snippets.mkdir(exist_ok=True)
    shutil.copy2(STYLE, snippets / "bvstk-json-tree.css")
    path = metadata / "appearance.json"
    appearance = json.loads(path.read_text()) if path.exists() else {}
    enabled = appearance.setdefault("enabledCssSnippets", [])
    if "bvstk-json-tree" not in enabled:
        enabled.append("bvstk-json-tree")
    path.write_text(json.dumps(appearance, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-css", type=Path, action="append", default=[], metavar="VAULT")
    args = parser.parse_args()
    print(f"Generated {generate()} standalone JSON trees")
    for vault in args.install_css:
        install_css(vault)
        print(f"Enabled compact JSON trees in {vault}")
