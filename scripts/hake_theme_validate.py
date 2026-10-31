#!/usr/bin/env python3
"""Validate the bundled UI themes, with strict checks for Hake Night.

Source mode (default) checks resources/themes in the repository. With
--installed ROOT it only checks that a packaged/installed tree contains the
Hake Night theme files. Errors in strict themes fail the run; problems in other
themes are reported as warnings. Exit status: 0 PASS, 1 FAIL.
"""

import argparse
import configparser
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
THEMES_DIR = ROOT / "resources/themes"
HAKE_ICON_DIR = ROOT / "resources/icons/hake"
HAKE_QRC = HAKE_ICON_DIR / "hake_icons.qrc"

STRICT_THEMES = {"Hake Night"}
# Themes whose tooltips must not inherit the OS tooltip palette.
TOOLTIP_THEMES = {"Hake Light", "Hake Night", "Hake Dark"}
EXPECTED_THEMES = [
    "Hake Light",
    "Hake Dark",
    "Hake Night",
    "Night Mapping",
    "Blend of Gray",
]
THEME_FILES = ["style.qss", "variables.qss", "palette.txt", "qscintilla.ini"]

# Hake Dark is the saturated brand-blue theme; this guards against it being repurposed.
HAKE_DARK_BACKGROUND = "#0775e3"

# Every token the Hake Light shell and ribbon rely on; Hake Night must define all of them.
REQUIRED_NIGHT_TOKENS = [
    "@background",
    "@itembackground",
    "@itemalternativebackground",
    "@itemdarkbackground",
    "@panelheader",
    "@statusbar",
    "@textlight",
    "@text",
    "@toggleoff",
    "@toggleon",
    "@selection",
    "@hover",
    "@focusdark",
    "@focus",
    "@chromeToolbar",
    "@chromeTextMuted",
    "@chromeHover",
    "@chromeActive",
    "@chromeBorder",
    "@chromeText",
    "@chrome",
    "@ribbonBorder",
    "@ribbonPressed",
    "@ribbonSurface",
    "@ribbonCaption",
    "@ribbonDisabled",
    "@darkgradient",
    "@darkalternativegradient",
    "@gradient",
]

TOGGLE_ICONS = ["app/hake-theme-moon.svg", "app/hake-theme-sun.svg"]
HAKE_ICON_COLOURS = {"#164A73", "#FFFFFF", "#D6E4F4"}

# QPalette::ColorRole values usable in palette.txt (17 is NoRole).
VALID_PALETTE_ROLES = set(range(0, 22)) - {17}

TOKEN_RE = re.compile(r"@[A-Za-z_][A-Za-z0-9_]*")
HEX_RE = re.compile(r"^#(?:[0-9A-Fa-f]{3}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$")
RGB_TUPLE_RE = re.compile(r"^\d{1,3}\s*,\s*\d{1,3}\s*,\s*\d{1,3}(?:\s*,\s*\d{1,3})?$")
URL_RE = re.compile(r"url\(\s*@theme_path/([^)\s]+)\s*\)")
TOOLTIP_RE = re.compile(r"^[ \t]*QToolTip\s*\{([^}]*)\}", re.M)
LAYOUT_TOOLTIP_RE = re.compile(r"^[ \t]*QgsLayoutView\s+QToolTip\s*\{([^}]*)\}", re.M)


class Report:
    def __init__(self):
        self.errors = []
        self.warnings = []

    def issue(self, strict, theme, msg):
        line = f"[{theme}] {msg}"
        if strict:
            self.errors.append(line)
            print(f"  FAIL: {line}")
        else:
            self.warnings.append(line)
            print(f"  WARN: {line}")


def strip_comments(text):
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def valid_colour(value):
    value = value.strip()
    return bool(HEX_RE.match(value) or RGB_TUPLE_RE.match(value))


def parse_variables(path, strict, theme, report):
    variables = {}
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("/*"):
            continue
        if not stripped.startswith("@"):
            report.issue(
                strict,
                theme,
                f"variables.qss:{n}: line is neither a comment nor a variable",
            )
            continue
        name, sep, value = stripped.partition(":")
        if not sep or not TOKEN_RE.fullmatch(name.strip()) or not value.strip():
            report.issue(
                strict,
                theme,
                f"variables.qss:{n}: malformed variable '{stripped[:60]}'",
            )
            continue
        name = name.strip()
        if name in variables:
            report.issue(strict, theme, f"variables.qss:{n}: {name} defined twice")
        variables[name] = value
    return variables


def apply_variables(style, variables, theme_path):
    """Mirror QgsApplication::setUITheme: @theme_path first, then longest names first."""
    out = style.replace("@theme_path", theme_path)
    for name in sorted(variables, key=len, reverse=True):
        out = out.replace(name, variables[name])
    return out


def check_style(theme_dir, variables, strict, theme, report):
    raw = (theme_dir / "style.qss").read_text(encoding="utf-8")
    style = strip_comments(raw)
    refs = set(TOKEN_RE.findall(style)) - {"@theme_path"}
    for ref in sorted(refs - set(variables)):
        report.issue(strict, theme, f"style.qss references undefined variable {ref}")
    unused = sorted(set(variables) - refs)
    if unused:
        print(
            f"  INFO: [{theme}] variables defined but unused in style.qss: {', '.join(unused)}"
        )

    substituted = apply_variables(style, variables, "THEME_PATH")
    leftovers = sorted(set(TOKEN_RE.findall(substituted)))
    for token in leftovers:
        report.issue(
            strict, theme, f"unresolved placeholder {token} remains after substitution"
        )

    for rel in sorted(set(URL_RE.findall(style))):
        if not (theme_dir / rel).is_file():
            report.issue(
                strict, theme, f"style.qss url(@theme_path/{rel}) does not exist"
            )


def block_properties(body):
    props = {}
    for decl in body.split(";"):
        name, sep, value = decl.partition(":")
        if sep:
            props[name.strip().lower()] = value.strip().lower()
    return props


def check_tooltip(theme_dir, variables, theme, report):
    """Tooltips must set their own background; otherwise the OS tooltip palette shows through."""
    style = apply_variables(
        strip_comments((theme_dir / "style.qss").read_text(encoding="utf-8")),
        variables,
        "THEME_PATH",
    )
    base = TOOLTIP_RE.search(style)
    if not base:
        report.issue(True, theme, "style.qss has no top-level QToolTip rule")
        return
    props = block_properties(base.group(1))
    background = props.get("background-color") or props.get("background")
    if not background:
        report.issue(
            True,
            theme,
            "QToolTip rule sets no background; the OS tooltip palette would show through",
        )
        return
    if props.get("color") == background:
        report.issue(True, theme, "QToolTip text color equals its background")
    layout = LAYOUT_TOOLTIP_RE.search(style)
    if layout and block_properties(layout.group(1)).get("color") == background:
        report.issue(
            True, theme, "QgsLayoutView QToolTip text color equals the tooltip background"
        )


def check_palette(path, strict, theme, report):
    seen = set()
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if not stripped:
            continue
        parts = stripped.split(":")
        group = "active"
        if len(parts) == 3 and parts[0].strip() == "disabled":
            group = "disabled"
            parts = parts[1:]
        if len(parts) != 2:
            report.issue(
                strict, theme, f"palette.txt:{n}: malformed entry '{stripped}'"
            )
            continue
        role_text, colour = parts[0].strip(), parts[1].strip()
        if not role_text.isdigit() or int(role_text) not in VALID_PALETTE_ROLES:
            report.issue(
                strict, theme, f"palette.txt:{n}: invalid color role '{role_text}'"
            )
            continue
        if not valid_colour(colour):
            report.issue(strict, theme, f"palette.txt:{n}: invalid color '{colour}'")
            continue
        key = (group, int(role_text))
        if key in seen:
            report.issue(
                strict, theme, f"palette.txt:{n}: role {role_text} ({group}) set twice"
            )
        seen.add(key)
    return seen


def check_qscintilla(path, strict, theme, report):
    parser = configparser.ConfigParser(interpolation=None)
    parser.optionxform = str
    try:
        parser.read(path, encoding="utf-8")
    except configparser.Error as e:
        report.issue(strict, theme, f"qscintilla.ini: parse error {e}")
        return
    if not parser.has_section("general"):
        report.issue(strict, theme, "qscintilla.ini: missing [general] section")
    for section in parser.sections():
        for key, value in parser.items(section):
            if not valid_colour(value):
                report.issue(
                    strict,
                    theme,
                    f"qscintilla.ini: [{section}] {key}={value} is not a color",
                )


def check_theme(theme_dir, report):
    theme = theme_dir.name
    strict = theme in STRICT_THEMES
    print(f"\n-- {theme}{' (strict)' if strict else ''}")
    for name in THEME_FILES:
        if not (theme_dir / name).is_file():
            report.issue(strict, theme, f"missing {name}")
    if not (theme_dir / "icons").is_dir():
        report.issue(strict, theme, "missing icons/ directory")
    if not (theme_dir / "style.qss").is_file():
        return None

    variables = {}
    if (theme_dir / "variables.qss").is_file():
        variables = parse_variables(theme_dir / "variables.qss", strict, theme, report)
    check_style(theme_dir, variables, strict, theme, report)
    if theme in TOOLTIP_THEMES:
        check_tooltip(theme_dir, variables, theme, report)
    if (theme_dir / "palette.txt").is_file():
        roles = check_palette(theme_dir / "palette.txt", strict, theme, report)
        if strict:
            for role in sorted(
                VALID_PALETTE_ROLES - {r for g, r in roles if g == "active"}
            ):
                report.issue(strict, theme, f"palette.txt: role {role} not set")
            for role in (0, 6, 8):
                if ("disabled", role) not in roles:
                    report.issue(
                        strict, theme, f"palette.txt: disabled role {role} not set"
                    )
    if (theme_dir / "qscintilla.ini").is_file():
        check_qscintilla(theme_dir / "qscintilla.ini", strict, theme, report)
    return variables


def check_toggle_icons(report):
    print("\n-- Theme toggle icons")
    qrc = set()
    if HAKE_QRC.is_file():
        for f in ET.parse(HAKE_QRC).getroot().iter("file"):
            qrc.add(f.text.strip())
    for rel in TOGGLE_ICONS:
        path = HAKE_ICON_DIR / rel
        if not path.is_file():
            report.issue(True, "icons", f"{rel} missing")
            continue
        if rel not in qrc:
            report.issue(True, "icons", f"{rel} not listed in hake_icons.qrc")
        for el in ET.parse(path).getroot().iter():
            for attr in ("fill", "stroke"):
                value = el.get(attr)
                if value and value != "none" and value not in HAKE_ICON_COLOURS:
                    report.issue(
                        True,
                        "icons",
                        f"{rel}: {attr} '{value}' outside the Hake icon palette",
                    )


def validate_source():
    report = Report()
    print("=== Hake theme validation (source) ===")
    print("\n-- Inventory")
    present = sorted(p.name for p in THEMES_DIR.iterdir() if p.is_dir())
    print(f"  themes: {', '.join(present)} (+ built-in 'default')")
    for name in EXPECTED_THEMES:
        if name not in present:
            report.issue(True, "inventory", f"theme '{name}' missing")
        elif not (THEMES_DIR / name / "style.qss").is_file():
            report.issue(
                True,
                "inventory",
                f"theme '{name}' has no style.qss and will not be registered",
            )

    results = {}
    for theme_dir in sorted(p for p in THEMES_DIR.iterdir() if p.is_dir()):
        results[theme_dir.name] = check_theme(theme_dir, report)

    night = results.get("Hake Night") or {}
    for token in REQUIRED_NIGHT_TOKENS:
        if token not in night:
            report.issue(True, "Hake Night", f"required token {token} not defined")

    dark = results.get("Hake Dark") or {}
    if dark.get("@background", "").strip().lower() != HAKE_DARK_BACKGROUND:
        report.issue(
            True,
            "Hake Dark",
            f"@background is no longer {HAKE_DARK_BACKGROUND}; Hake Dark must stay the brand-blue theme",
        )

    check_toggle_icons(report)
    return report


def validate_installed(root):
    report = Report()
    print(f"=== Hake theme validation (installed tree: {root}) ===")
    candidates = [
        p
        for p in root.rglob("style.qss")
        if p.parent.name == "Hake Night" and p.parent.parent.name == "themes"
    ]
    if not candidates:
        report.issue(
            True, "installed", "resources/themes/Hake Night/style.qss not found"
        )
        return report
    for theme_dir in {c.parent for c in candidates}:
        print(f"  found {theme_dir}")
        for name in THEME_FILES:
            if not (theme_dir / name).is_file():
                report.issue(True, "installed", f"{theme_dir}/{name} missing")
        source_icons = (
            {p.name for p in (THEMES_DIR / "Hake Night/icons").glob("*.svg")}
            if THEMES_DIR.is_dir()
            else set()
        )
        installed_icons = {p.name for p in (theme_dir / "icons").glob("*.svg")}
        if not installed_icons:
            report.issue(True, "installed", f"{theme_dir}/icons has no SVGs")
        for name in sorted(source_icons - installed_icons):
            report.issue(True, "installed", f"{theme_dir}/icons/{name} missing")
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--installed",
        type=Path,
        help="check an installed/packaged tree instead of the source themes",
    )
    args = ap.parse_args()

    report = validate_installed(args.installed) if args.installed else validate_source()
    print()
    print(f"errors: {len(report.errors)}  warnings: {len(report.warnings)}")
    print("RESULT: PASS" if not report.errors else "RESULT: FAIL")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
