#!/usr/bin/env python3
"""Validate Hake icon coverage of the Options dialog sidebar.

Builds the inventory of application-owned Options pages from the sources (the
built-in pages of src/ui/qgsoptionsbase.ui, the tree groups created by the
QgsOptions constructor, the options widget factories registered by QgisApp and
QgsOptions, and the factories registered by bundled Python code), then checks it
against HAKE_OPTIONS_PAGE_ICONS in src/app/qgshakeicons.cpp. Pages registered by
third-party plugins at runtime are not part of the inventory and keep their own
icons. Exit status: 0 PASS, 1 FAIL.
"""

import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from hake_icon_coverage import ICON_DIR, QRC, REGISTRY, lint_svg, parse_qrc  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OPTIONS_UI = ROOT / "src/ui/qgsoptionsbase.ui"
OPTIONS_CPP = ROOT / "src/app/options/qgsoptions.cpp"
QGISAPP_CPP = ROOT / "src/app/qgisapp.cpp"
APP_SRC = ROOT / "src/app"
PYTHON_SRC = [ROOT / "python/console", ROOT / "python/plugins"]
OPTIONS_ICON_DIR = "options"

# Application-owned pages that intentionally keep a non-Hake icon: id -> reason.
EXCEPTIONS = {}


def split_args(text):
    """Splits a C++ argument list at top-level commas."""
    args, depth, current = [], 0, ""
    for ch in text:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        args.append(current.strip())
    return args


def balanced(text, start):
    """Returns the text inside the parentheses opening at text[start]."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start + 1 : i]
    return None


def builtin_pages():
    root = ET.parse(OPTIONS_UI).getroot()
    for widget in root.iter("widget"):
        if widget.get("name") == "mOptionsStackedWidget":
            return [
                (c.get("name"), "built-in page")
                for c in widget
                if c.tag == "widget"
            ]
    return []


def group_keys():
    text = OPTIONS_CPP.read_text(encoding="utf-8")
    ctor = re.search(r"QgsOptions::QgsOptions\(.*?mOptionsTreeView->setModel", text, re.S)
    if not ctor:
        return []
    return [
        (key, "tree group")
        for key in re.findall(r'\w+->setData\(\s*u"([^"]+)"_s\s*\)', ctor.group(0))
    ]


def app_sources():
    return {p: p.read_text(encoding="utf-8", errors="replace") for p in APP_SRC.rglob("*.cpp")}


def resolve_cpp_factory(cls, sources):
    """Returns (id, how) for a C++ options factory, or (None, reason)."""
    ctor_re = re.compile(rf"\b{cls}::{cls}\(\s*\)\s*:\s*QgsOptionsWidgetFactory\(")
    for path, text in sources.items():
        m = ctor_re.search(text)
        if not m:
            continue
        args = split_args(balanced(text, m.end() - 1) or "")
        if len(args) >= 3:
            key = re.fullmatch(r'u"([^"]+)"_s', args[2])
            if key:
                return key.group(1), "factory key"
        # No key: the page is identified by the objectName of the widget it creates.
        create = re.search(rf"\b{cls}::createWidget\(.*?\breturn new (\w+)\(", text, re.S)
        if not create:
            return None, f"{cls}: createWidget() not found"
        widget = create.group(1)
        return resolve_widget_name(widget, path, sources)
    return None, f"{cls}: constructor not found"


def resolve_widget_name(widget, path, sources):
    body = re.search(rf"\b{widget}::{widget}\(.*?\{{(.*?)\n\}}", sources[path], re.S)
    if body:
        name = re.search(r'setObjectName\(\s*u?"([^"]+)"', body.group(1))
        if name:
            return name.group(1), "widget objectName"
    header = path.with_suffix(".h")
    if header.is_file():
        ui = re.search(
            rf"class\s+(?:\w+\s+)?{widget}\b[^{{]*\bUi::(\w+)",
            header.read_text(encoding="utf-8", errors="replace"),
        )
        if ui:
            return ui.group(1), "widget .ui root name"
    return None, f"{widget}: objectName not found"


def app_factories(sources):
    text = QGISAPP_CPP.read_text(encoding="utf-8")
    classes = re.findall(
        r"mOptionWidgetFactories\.emplace_back\(\s*QgsScopedOptionsWidgetFactory\(\s*std::make_unique<(\w+)>",
        text,
    )
    classes += re.findall(r"\b(Qgs\w+OptionsFactory)\s+\w+Factory;", OPTIONS_CPP.read_text(encoding="utf-8"))
    pages, errors = [], []
    for cls in classes:
        page, how = resolve_cpp_factory(cls, sources)
        if page:
            pages.append((page, f"{cls} ({how})"))
        else:
            errors.append(how)
    return pages, errors


def python_factories():
    files = [p for d in PYTHON_SRC for p in d.rglob("*.py") if "test" not in p.parts]
    texts = {p: p.read_text(encoding="utf-8", errors="replace") for p in files}
    pages, errors = [], []
    for path, text in texts.items():
        for m in re.finditer(r"^class (\w+)\(QgsOptionsWidgetFactory\):(.*?)(?=^\S)", text + "\nEOF", re.S | re.M):
            cls, body = m.group(1), m.group(2)
            create = re.search(r"def createWidget\(self, \w+\):\s*return (\w+)\(", body)
            if not create:
                errors.append(f"{cls}: createWidget() not found")
                continue
            widget = create.group(1)
            name = None
            for other in texts.values():
                w = re.search(rf"^class {widget}\(.*?(?=^\S)", other + "\nEOF", re.S | re.M)
                if w:
                    n = re.search(r'self\.setObjectName\("([^"]+)"\)', w.group(0))
                    name = n.group(1) if n else None
                    break
            if name:
                pages.append((name, f"{cls} (Python widget objectName)"))
            else:
                errors.append(f"{cls}: objectName of {widget} not found")
    return pages, errors


def parse_options_table():
    text = REGISTRY.read_text(encoding="utf-8")
    body = re.search(r"HAKE_OPTIONS_PAGE_ICONS\[\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    if not body:
        return None
    return re.findall(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}', body.group(1))


def main():
    failures = []

    def fail(msg):
        failures.append(msg)
        print(f"  FAIL: {msg}")

    print("=== Hake Options sidebar icon coverage ===")
    sources = app_sources()
    inventory = builtin_pages() + group_keys()
    factories, errors = app_factories(sources)
    inventory += factories
    py_pages, py_errors = python_factories()
    inventory += py_pages
    for e in errors + py_errors:
        fail(f"cannot resolve page id: {e}")

    ids = [i for i, _ in inventory]
    for i, n in Counter(ids).items():
        if n > 1:
            fail(f"page id {i} detected {n} times")
    print(f"Options pages detected: {len(set(ids))}")

    table = parse_options_table()
    if table is None:
        fail("HAKE_OPTIONS_PAGE_ICONS table not found in qgshakeicons.cpp")
        table = []
    mapping = dict(table)

    print("\n-- Duplicate mappings")
    dups = [k for k, n in Counter(k for k, _ in table).items() if n > 1]
    for k in dups:
        fail(f"{k} mapped more than once")
    if not dups:
        print("  none")

    print("\n-- Coverage")
    mapped = [i for i in ids if i in mapping]
    preserved = [i for i in ids if i not in mapping and i in EXCEPTIONS]
    missing = [i for i in ids if i not in mapping and i not in EXCEPTIONS]
    for i in preserved:
        print(f"  PRESERVED {i}: {EXCEPTIONS[i]}")
    for i in missing:
        fail(f"{i} has no Hake mapping")
    for i in EXCEPTIONS:
        if i in mapping:
            fail(f"{i} is both mapped and listed as an exception")

    print("\n-- Obsolete mappings (not a production page)")
    obsolete = [k for k in mapping if k not in ids]
    for k in obsolete:
        fail(f"{k} is mapped but no such Options page exists")
    if not obsolete:
        print("  none")

    print("\n-- Resources")
    qrc = set(parse_qrc(QRC))
    on_disk = {p.relative_to(ICON_DIR).as_posix() for p in ICON_DIR.rglob("*.svg")}
    for k, res in table:
        if res not in on_disk:
            fail(f"{k}: resource {res} missing on disk")
        elif res not in qrc:
            fail(f"{k}: resource {res} not in hake_icons.qrc")
    referenced = set()
    for text in sources.values():
        referenced.update(re.findall(rf'"({OPTIONS_ICON_DIR}/hake-[a-z0-9-]+\.svg)"', text))
    own = sorted(r for r in on_disk if r.startswith(f"{OPTIONS_ICON_DIR}/"))
    unused = [r for r in own if r not in referenced]
    for r in unused:
        fail(f"{r} is never referenced")
    reused = sorted({res for _, res in table if not res.startswith(f"{OPTIONS_ICON_DIR}/")})
    print(f"  Options icons in {OPTIONS_ICON_DIR}/: {len(own)}  reused from other groups: {len(reused)}")

    print("\n-- SVG style lint (mapped resources)")
    lint_errors = 0
    for res in sorted({res for _, res in table if res in on_disk}):
        errs, _, _ = lint_svg(ICON_DIR / res)
        for e in errs:
            fail(f"{res}: {e}")
            lint_errors += 1
    print(f"  errors: {lint_errors}")

    print("\n-- Inventory")
    for i, origin in inventory:
        print(f"  {i:<34} {origin:<58} {mapping.get(i, '-')}")

    print()
    print(f"Options pages detected: {len(set(ids))}")
    print(f"Hake mappings:          {len(mapped)}")
    print(f"Preserved exceptions:   {len(preserved)}")
    print("Third-party plugin pages registered at runtime keep their own icons (not part of this inventory).")
    print(f"Missing:                {len(missing)}")
    print(f"Duplicate:              {len(dups)}")
    print(f"Obsolete:               {len(obsolete)}")
    print(f"Orphan resources:       {len(unused)}")
    print()
    if failures:
        print(f"RESULT: FAIL ({len(failures)} problem(s))")
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
