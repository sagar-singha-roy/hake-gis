#!/usr/bin/env python3
"""Validate Hake icon coverage against the menu icon catalog and the sources.

Checks that every catalog row is mapped in the HAKE_ICONS registry in
src/app/qgshakeicons.cpp, that the snapping, shape, annotation, property page
and widget targets derived from the sources are mapped (or listed as
exceptions), that every mapped SVG exists and is compiled into
resources/icons/hake/hake_icons.qrc, that every Hake SVG follows the icon
style rules, and runs the Options sidebar check. The catalog is not tracked in
git; without it the catalog section is reported as NOT RUN and the remaining
checks still decide the result. Exit status: 0 PASS, 1 FAIL, 2 catalog missing
with --require-catalog.
"""

import argparse
import contextlib
import io
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CATALOG = ROOT / "docs/hake-archive/hake-menu-icon-prompt-catalog.md"
REGISTRY = ROOT / "src/app/qgshakeicons.cpp"
ICON_DIR = ROOT / "resources/icons/hake"
QRC = ICON_DIR / "hake_icons.qrc"
APP_CMAKE = ROOT / "src/app/CMakeLists.txt"

EXPECTED_CATALOG_ROWS = 228

# Catalog rows with no production counterpart; they are not counted as missing.
EXCLUDED = {
    "dsm:sqlite": "NOT PRESENT IN PRODUCTION (no SQLite source tab is registered; provider not re-enabled)",
}

# Catalog identifiers that differ from the production identifier.
CORRECTED = {
    "native:qgis:randompointsinlayerbounds": "qgis:randompointsinlayerbounds",
}

# Data Source Manager catalog ids -> provider key stored in the dialog items.
DSM_KEYS = {
    "dsm:browser": "browser",
    "dsm:vector": "ogr",
    "dsm:raster": "gdal",
    "dsm:mesh": "mdal",
    "dsm:point-cloud": "pointcloud",
    "dsm:delimited-text": "delimitedtext",
    "dsm:geopackage": "GeoPackage",
    "dsm:gps": "gpx",
    "dsm:spatialite": "spatialite",
    "dsm:postgresql": "postgres",
    "dsm:ms-sql-server": "mssql",
    "dsm:oracle": "oracle",
    "dsm:sap-hana": "hana",
    "dsm:virtual-layer": "virtual",
    "dsm:wms-wmts": "wms",
    "dsm:wfs-ogc-api-features": "WFS",
    "dsm:wcs": "wcs",
    "dsm:xyz": "xyz",
    "dsm:vector-tile": "vectortile",
    "dsm:scene": "tiledscene",
    "dsm:arcgis-rest-server": "arcgisfeatureserver",
    "dsm:sensorthings": "sensorthings",
    "dsm:stac": "stac",
    "dsm:metadata-search": "layermetadata",
}

SECTION_KINDS = {
    "Ribbon panels": "DOCK",
    "Data Source Manager": "DSM",
    "Processing, Database, and Web": "PLUGIN",
}

ALLOWED_COLOURS = {"#164A73", "#FFFFFF", "#D6E4F4"}
FORBIDDEN_TAGS = {
    "text",
    "tspan",
    "textPath",
    "image",
    "filter",
    "mask",
    "linearGradient",
    "radialGradient",
    "pattern",
    "style",
    "font",
    "font-face",
    "foreignObject",
    "script",
    "use",
    "symbol",
    "clipPath",
}
STROKE_WIDTH_RANGE = (0.9, 2.6)

# Registry kinds validated against source-derived inventories instead of the catalog.
SOURCE_KINDS = {
    "SNAP": "snapping widget objects",
    "SNAPTYPE": "snapping types",
    "SHAPE": "shape digitizing tools",
    "ANNOT": "annotation item types",
    "W": "widget icon keys",
}
REGISTRY_KINDS = ["A", "ALG", "PLUGIN", "DOCK", "DSM", "MENU", *SOURCE_KINDS]

SNAPPING_WIDGET = ROOT / "src/app/qgssnappingwidget.cpp"
SHAPE_TOOLS = ROOT / "src/app/maptools"
ANNOTATION_REGISTRY = ROOT / "src/gui/annotations/qgsannotationitemguiregistry.cpp"
QGIS_H = ROOT / "src/core/qgis.h"
PROJECT_UI = ROOT / "src/ui/qgsprojectpropertiesbase.ui"
QGISAPP = ROOT / "src/app/qgisapp.cpp"
QML_DIR = ROOT / "src/app/qml"
LAYER_PROPERTIES_UI = [
    "src/ui/qgsvectorlayerpropertiesbase.ui",
    "src/ui/qgsrasterlayerpropertiesbase.ui",
    "src/ui/mesh/qgsmeshlayerpropertiesbase.ui",
    "src/ui/qgsvectortilelayerpropertiesbase.ui",
    "src/ui/qgspointcloudlayerpropertiesbase.ui",
    "src/ui/qgstiledscenelayerpropertiesbase.ui",
    "src/ui/annotations/qgsannotationlayerpropertiesbase.ui",
]

# Application-owned targets that intentionally have no Hake mapping: target -> reason.
EXCEPTIONS = {
    "snap:SnappingOptionToolBar": "toolbar container, no icon",
    "snap:SnappingOptionDialog": "dialog container, no icon",
    "snap:avoidIntersectionsModeMenu": "menu container, no icon",
    "snap:tracingMenu": "menu container, no icon",
    "snap:openDialogAction": "text-only menu entry",
    "snap:mModeAction": "toolbar widget holder",
    "snap:mEditAdvancedConfigAction": "toolbar widget holder",
    "snap:mTypeAction": "toolbar widget holder",
    "snap:mToleranceAction": "toolbar widget holder",
    "snap:mUnitAction": "toolbar widget holder",
    "snap:mAvoidIntersectionsModeAction": "toolbar widget holder",
    "snap:tracingWidgetAction": "toolbar widget holder",
    "snap:SnappingModeButton": "shows its default action's (mapped) icon",
    "snap:SnappingTypeButton": "shows its default action's (mapped) icon",
    "snap:AvoidIntersectionsModeButton": "shows its default action's (mapped) icon",
    "snap:SnappingScaleModeButton": "shows its default action's (mapped) icon",
    "snap:SnappingToleranceSpinBox": "input widget, no icon",
    "snap:SnappingUnitComboBox": "input widget, no icon",
    "snap:SnappingMinScaleSpinBox": "input widget, no icon",
    "snap:SnappingMaxScaleSpinBox": "input widget, no icon",
}

# Layer properties pages added at runtime (not in the .ui files); each must appear in the sources.
RUNTIME_LAYER_PAGES = {
    "mOptsPage_3DView": "3D view page added by the 3D renderer factory",
    "mOptsPage_Digitizing": "vector digitizing page added at runtime",
    "mOptsPage_Elevation": "elevation page added at runtime",
    "QgsPointCloudRendererPropsDialogBase": "point cloud symbology page, matched by class name",
    "QgsTiledSceneRendererPropsDialogBase": "tiled scene symbology page, matched by class name",
}


def parse_catalog(path):
    rows = []
    part = 1
    section = None
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# Part 2"):
            part = 2
        elif line.startswith("## "):
            section = line[3:].strip()
        elif line.startswith("### "):
            pass
        m = re.match(r"^- action: `([^`]+)`", line)
        if m:
            current = {"id": m.group(1), "part": part, "section": section}
            rows.append(current)
            continue
        m = re.match(r"^  - (menu|label|stock icon|subject): (.*)$", line)
        if m and current is not None:
            current[m.group(1)] = m.group(2).strip()
    return rows


def expected_mapping(row):
    cid = row["id"]
    kind = SECTION_KINDS.get(row["section"])
    if kind is None:
        kind = "ALG" if row["part"] == 2 else "A"
    key = CORRECTED.get(cid, cid)
    if kind == "DSM":
        key = DSM_KEYS.get(cid)
    return kind, key


def parse_registry(path):
    text = path.read_text(encoding="utf-8")
    body = re.search(r"HAKE_ICONS\[\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    if not body:
        return None
    return [
        (m.group(1), m.group(2), m.group(3))
        for m in re.finditer(
            rf'\{{\s*({"|".join(REGISTRY_KINDS)})\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}}',
            body.group(1),
        )
    ]


def parse_page_table(path, table):
    text = path.read_text(encoding="utf-8")
    body = re.search(rf"{table}\[\]\s*=\s*\{{(.*?)\n\s*\}};", text, re.S)
    if not body:
        return []
    return re.findall(r'\{\s*"([A-Za-z0-9_]+)"\s*,\s*"([^"]+)"\s*\}', body.group(1))


def parse_property_pages(path):
    return parse_page_table(path, "HAKE_PROPERTY_PAGE_ICONS")


def parse_geometry_icons(path):
    text = path.read_text(encoding="utf-8")
    body = re.search(r"HAKE_GEOMETRY_ICONS\[\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    actions = re.search(r"GEOMETRY_ACTIONS\[\]\s*=\s*\{(.*?)\};", text, re.S)
    entries = (
        re.findall(r'\{\s*"(\w+)"\s*,\s*Qgis::GeometryType::(\w+)\s*,\s*"([^"]+)"\s*\}', body.group(1))
        if body
        else []
    )
    return entries, re.findall(r'"(\w+)"', actions.group(1)) if actions else []


def stacked_pages(ui):
    root = ET.parse(ui).getroot()
    for widget in root.iter("widget"):
        if widget.get("name") == "mOptionsStackedWidget":
            return [c.get("name") for c in widget if c.tag == "widget"]
    return []


def source_inventories():
    """Returns {kind: {key: origin}} for the targets derived from the sources."""
    inv = {}
    snap = re.findall(r'setObjectName\(\s*u"([^"]+)"_s\s*\)', SNAPPING_WIDGET.read_text(encoding="utf-8"))
    inv["SNAP"] = {k: "qgssnappingwidget.cpp" for k in snap}
    enum = re.search(r"enum class SnappingType\b.*?\{(.*?)\};", QGIS_H.read_text(encoding="utf-8"), re.S)
    types = re.findall(r"^\s*(\w+)\s+SIP_MONKEYPATCH", enum.group(1), re.M) if enum else []
    inv["SNAPTYPE"] = {t: "Qgis::SnappingType" for t in types if t != "NoSnap"}
    shapes = {}
    for f in sorted(SHAPE_TOOLS.glob("qgsmaptoolshape*.[ch]*")):
        for tid in re.findall(r'TOOL_ID\w*\s*=\s*u"([^"]+)"_s', f.read_text(encoding="utf-8")):
            shapes[tid] = f.name
    inv["SHAPE"] = shapes
    annot = re.findall(
        r'new QgsAnnotationItemGuiMetadata\(\s*u"([^"]+)"_s', ANNOTATION_REGISTRY.read_text(encoding="utf-8")
    )
    inv["ANNOT"] = {a: "QgsAnnotationItemGuiRegistry::addDefaultItems" for a in annot}
    widget_keys = {}
    for src in sorted((ROOT / "src/app").rglob("*.cpp")):
        text = src.read_text(encoding="utf-8", errors="replace")
        keys = re.findall(r'iconFor\(\s*u"([^"]+)"_s', text) + re.findall(
            r'withHakeIcon\([^;]*?,\s*u"([^"]+)"_s\s*\)', text
        )
        if src != REGISTRY and '#include "qgshakeicons.h"' in text:
            # Widget keys passed through containers (e.g. the About sidebar list).
            keys += re.findall(r'u"((?:about|statusbar|welcome|layertree):[a-z-]+)"_s', text)
        for key in keys:
            widget_keys[key] = src.name
    for qml in sorted(QML_DIR.rglob("*.qml")):
        for key in re.findall(r'image://hakeicon/([^/"]+)/', qml.read_text(encoding="utf-8")):
            widget_keys[key] = qml.name
    inv["W"] = widget_keys
    return inv


def project_page_inventory():
    pages = {p: "qgsprojectpropertiesbase.ui" for p in stacked_pages(PROJECT_UI)}
    for factory in re.findall(
        r"registerProjectPropertiesWidgetFactory\(\s*new (\w+)Factory\(", QGISAPP.read_text(encoding="utf-8")
    ):
        pages[factory] = "project properties factory (widget class name)"
    return pages


def layer_page_inventory():
    pages = {}
    for ui in LAYER_PROPERTIES_UI:
        for p in stacked_pages(ROOT / ui):
            pages.setdefault(p, Path(ui).name)
    return pages


def source_has_literal(key):
    pattern = re.compile(rf'"{re.escape(key)}"|\b{re.escape(key)}\b')
    for d in ("src/app", "src/gui", "src/ui"):
        for f in (ROOT / d).rglob("*"):
            if f.suffix in (".cpp", ".h", ".ui") and f != REGISTRY:
                if pattern.search(f.read_text(encoding="utf-8", errors="replace")):
                    return True
    return False


def parse_qrc(path):
    root = ET.parse(path).getroot()
    entries = []
    for res in root.iter("qresource"):
        if res.get("prefix") != "/hake/icons":
            continue
        entries.extend(f.text.strip() for f in res.iter("file"))
    return entries


def source_references():
    refs = set()
    for src in (ROOT / "src/app").rglob("*.cpp"):
        text = src.read_text(encoding="utf-8", errors="replace")
        if src == REGISTRY:
            # Only the presentation tables outside HAKE_ICONS (Browser roots) count here.
            text = re.sub(r"HAKE_ICONS\[\]\s*=\s*\{.*?\n\s*\};", "", text, flags=re.S)
        refs.update(re.findall(r'"((?:[a-z]+)/hake-[a-z0-9-]+\.svg)"', text))
    return refs


def local(tag):
    return tag.rsplit("}", 1)[-1]


def lint_svg(path):
    errors, warnings, widths = [], [], []
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as e:
        return [f"XML parse error: {e}"], warnings, widths
    if local(root.tag) != "svg":
        errors.append("root element is not <svg>")
    if root.get("width") != "24" or root.get("height") != "24":
        errors.append(f"size {root.get('width')}x{root.get('height')}, expected 24x24")
    if (root.get("viewBox") or "").split() != ["0", "0", "24", "24"]:
        errors.append(f"viewBox '{root.get('viewBox')}', expected '0 0 24 24'")
    caps, joins = set(), set()
    for el in root.iter():
        tag = local(el.tag)
        if tag in FORBIDDEN_TAGS:
            errors.append(f"forbidden element <{tag}>")
        for attr, value in el.attrib.items():
            name = local(attr)
            if (
                name in ("href", "style", "class")
                or "data:" in value
                or "url(" in value
            ):
                errors.append(f"forbidden attribute {name}='{value[:40]}'")
            if name in ("fill", "stroke") and value != "none":
                if value.upper() in ALLOWED_COLOURS and value not in ALLOWED_COLOURS:
                    errors.append(
                        f"{name} '{value}' must be uppercase for state recolouring"
                    )
                elif value not in ALLOWED_COLOURS:
                    errors.append(f"{name} colour '{value}' outside palette")
            if name in ("opacity", "fill-opacity", "stroke-opacity"):
                errors.append(f"{name} not allowed")
            if name == "stroke-linecap":
                caps.add(value)
            if name == "stroke-linejoin":
                joins.add(value)
            if name == "stroke-width":
                try:
                    w = float(value)
                    widths.append(w)
                    if not STROKE_WIDTH_RANGE[0] <= w <= STROKE_WIDTH_RANGE[1]:
                        warnings.append(
                            f"stroke-width {w} outside {STROKE_WIDTH_RANGE}"
                        )
                except ValueError:
                    errors.append(f"stroke-width '{value}' not numeric")
            if name == "transform" and not re.fullmatch(
                r"\s*(translate|rotate|scale)\([^)]*\)\s*", value
            ):
                warnings.append(f"transform '{value}'")
    if caps - {"round"}:
        errors.append(f"stroke-linecap {sorted(caps)} (must be round)")
    if joins - {"round"}:
        errors.append(f"stroke-linejoin {sorted(joins)} (must be round)")
    if "round" not in caps or "round" not in joins:
        errors.append("round caps/joins not declared")
    if "1.6" not in {f"{w:g}" for w in widths}:
        warnings.append("primary stroke-width 1.6 not used")
    return errors, warnings, widths


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    ap.add_argument(
        "--verbose",
        action="store_true",
        help="list every mapping and every lint warning",
    )
    ap.add_argument(
        "--require-catalog",
        action="store_true",
        help="exit with status 2 when the (untracked) catalog is missing",
    )
    args = ap.parse_args()

    has_catalog = args.catalog.is_file()
    if not has_catalog and args.require_catalog:
        print(f"Catalog not found: {args.catalog}")
        print("RESULT: NOT RUN (catalog missing)")
        return 2

    failures = []

    def fail(msg):
        failures.append(msg)
        print(f"  FAIL: {msg}")

    print("=== Hake icon coverage ===")
    rows = parse_catalog(args.catalog) if has_catalog else []
    if has_catalog:
        print(
            f"Catalog: {args.catalog.relative_to(ROOT) if args.catalog.is_relative_to(ROOT) else args.catalog}"
        )
        print(f"Catalog rows: {len(rows)} (expected {EXPECTED_CATALOG_ROWS})")
        if len(rows) != EXPECTED_CATALOG_ROWS:
            fail(f"catalog row count {len(rows)} != {EXPECTED_CATALOG_ROWS}")
    else:
        print(f"Catalog: NOT RUN ({args.catalog.name} is not tracked in git; source-derived checks still apply)")
    dup_ids = [i for i, n in Counter(r["id"] for r in rows).items() if n > 1]
    for i in dup_ids:
        fail(f"duplicate catalog id {i}")

    registry = parse_registry(REGISTRY)
    if registry is None:
        fail("HAKE_ICONS table not found in qgshakeicons.cpp")
        registry = []
    by_key = defaultdict(list)
    for kind, key, res in registry:
        by_key[(kind, key)].append(res)
    print(f"Registry entries: {len(registry)}")

    print("\n-- Duplicate registry entries")
    dups = {k: v for k, v in by_key.items() if len(v) > 1}
    for (kind, key), res in sorted(dups.items()):
        fail(f"{kind} {key} mapped {len(res)} times")
    if not dups:
        print("  none")

    print("\n-- Catalog coverage")
    covered, missing, excluded = [], [], []
    catalog_keys = set()
    for row in rows:
        cid = row["id"]
        if cid in EXCLUDED:
            excluded.append(row)
            continue
        kind, key = expected_mapping(row)
        if key is None:
            fail(f"{cid}: no production key known for this catalog id")
            missing.append(row)
            continue
        catalog_keys.add((kind, key))
        if (kind, key) in by_key:
            covered.append((row, kind, key, by_key[(kind, key)][0]))
        else:
            missing.append(row)
            fail(f"{cid}: no {kind} mapping for '{key}'")
    print(f"  production mappings required: {len(rows) - len(excluded)}")
    print(
        f"  covered: {len(covered)}  missing: {len(missing)}  excluded: {len(excluded)}"
    )
    for row in excluded:
        print(f"  EXCLUDED {row['id']}: {EXCLUDED[row['id']]}")
    for bad, good in CORRECTED.items():
        state = (
            "applied" if any(r["id"] == bad for r in rows) else "catalog row not found"
        )
        print(f"  CORRECTED {bad} -> {good} ({state})")

    print("\n-- Orphan registry entries (not in catalog)")
    reference = [(k, key, r) for k, key, r in registry if (k, key) not in catalog_keys]
    # Ribbon menu drop-downs are not menu commands, so the catalog does not list them;
    # source-derived kinds are checked against their inventories below.
    orphans = [e for e in reference if e[0] not in ("A", "MENU", *SOURCE_KINDS)] if has_catalog else []
    for kind, key, _ in orphans:
        fail(f"{kind} {key} is registered but not in the catalog")
    print(
        f"  pre-existing Action mappings outside the catalog (reference set): {sum(1 for e in reference if e[0] == 'A')}"
    )
    print(
        f"  ribbon menu drop-down mappings (MENU): {sum(1 for e in reference if e[0] == 'MENU')}"
    )
    if not orphans:
        print("  no non-Action orphans" if has_catalog else "  NOT RUN (no catalog)")

    print("\n-- Source-derived targets")
    inventories = source_inventories()
    registered = {kind: {key for k, key, _ in registry if k == kind} for kind in REGISTRY_KINDS}
    target_counts = {}
    for kind, label in SOURCE_KINDS.items():
        targets = inventories[kind]
        mapped, excepted, missing_t = 0, 0, 0
        for key, origin in sorted(targets.items()):
            if key in registered[kind] or (kind == "W" and key in registered["A"]):
                mapped += 1
            elif f"{kind.lower()}:{key}" in EXCEPTIONS:
                excepted += 1
            else:
                missing_t += 1
                fail(f"{label}: '{key}' ({origin}) has no {kind} mapping")
        for key in sorted(registered[kind] - set(targets)):
            fail(f"{kind} {key} is registered but not found in the sources")
        for exc in EXCEPTIONS:
            k, _, key = exc.partition(":")
            if k == kind.lower() and key not in targets:
                fail(f"exception {exc} does not match a source target")
            if k == kind.lower() and key in registered[kind]:
                fail(f"exception {exc} is also mapped")
        target_counts[kind] = (len(targets), mapped, excepted, missing_t)
        print(f"  {label:<26} targets: {len(targets):>3}  mapped: {mapped:>3}  exceptions: {excepted:>3}  missing: {missing_t}")

    print("\n-- Resources")
    qrc = parse_qrc(QRC)
    qrc_set = set(qrc)
    on_disk = {p.relative_to(ICON_DIR).as_posix() for p in ICON_DIR.rglob("*.svg")}
    for f in sorted(n for n, c in Counter(qrc).items() if c > 1):
        fail(f"qrc lists {f} more than once")
    for f in sorted(qrc_set - on_disk):
        fail(f"qrc entry {f} does not exist on disk")
    for f in sorted(on_disk - qrc_set):
        fail(f"{f} exists on disk but is not in hake_icons.qrc")
    for kind, key, res in registry:
        if res not in on_disk:
            fail(f"{kind} {key}: resource {res} missing on disk")
        elif res not in qrc_set:
            fail(f"{kind} {key}: resource {res} not in qrc")
    property_pages = parse_property_pages(REGISTRY)
    project_pages = parse_page_table(REGISTRY, "HAKE_PROJECT_PAGE_ICONS")
    geometry_icons, geometry_actions = parse_geometry_icons(REGISTRY)
    for label, table in (("layer properties page", property_pages), ("project properties page", project_pages)):
        for page, n in Counter(p for p, _ in table).items():
            if n > 1:
                fail(f"{label} {page} mapped {n} times")
        for page, res in table:
            if res not in on_disk:
                fail(f"{label} {page}: resource {res} missing on disk")
            elif res not in qrc_set:
                fail(f"{label} {page}: resource {res} not in qrc")
    for action, geometry, res in geometry_icons:
        if res not in on_disk:
            fail(f"geometry icon {action}/{geometry}: resource {res} missing on disk")
        if action not in geometry_actions:
            fail(f"geometry icon {action} is not listed in GEOMETRY_ACTIONS")
    for action in geometry_actions:
        if action not in registered["A"]:
            fail(f"geometry action {action} has no A mapping for its default icon")
    extra_refs = source_references()
    for res in sorted(extra_refs - on_disk):
        fail(f"source references {res} which does not exist")
    used = {r for _, _, r in registry} | extra_refs | {r for _, r in property_pages}
    unused = sorted(qrc_set - used)
    for f in unused:
        fail(f"{f} is compiled but never referenced")
    if "hake_icons.qrc" not in APP_CMAKE.read_text(encoding="utf-8"):
        fail("hake_icons.qrc is not listed in src/app/CMakeLists.txt")
    print(
        f"  qrc entries: {len(qrc)}  SVGs on disk: {len(on_disk)}  referenced outside registry: {len(extra_refs)}"
        f"  layer properties pages: {len(property_pages)}  project properties pages: {len(project_pages)}"
        f"  geometry variants: {len(geometry_icons)}"
    )

    print("\n-- Properties dialog pages")
    page_counts = {}
    for label, table, inventory, runtime in (
        ("Layer Properties", property_pages, layer_page_inventory(), RUNTIME_LAYER_PAGES),
        ("Project Properties", project_pages, project_page_inventory(), {}),
    ):
        mapping = dict(table)
        for page, origin in sorted(inventory.items()):
            if page not in mapping:
                fail(f"{label}: page {page} ({origin}) has no Hake mapping")
        for page in sorted(set(mapping) - set(inventory)):
            if page not in runtime:
                fail(f"{label}: {page} is mapped but no such page exists")
            elif not source_has_literal(page):
                fail(f"{label}: runtime page {page} not found in the sources")
        page_counts[label] = (len(inventory), sum(1 for p in inventory if p in mapping), len(set(mapping) & set(runtime)))
        print(f"  {label:<20} .ui/factory pages: {page_counts[label][0]:>3}  mapped: {page_counts[label][1]:>3}  runtime pages mapped: {page_counts[label][2]}")

    print("\n-- Reuse (one SVG serving several mappings)")
    users = defaultdict(list)
    for kind, key, res in registry:
        users[res].append(f"{kind}:{key}")
    for res in extra_refs:
        users[res].append("source")
    catalog_resources = {c[3] for c in covered}
    reused = {r: u for r, u in users.items() if len(u) > 1 and r in catalog_resources}
    for res, u in sorted(reused.items()):
        print(f"  {res}: {', '.join(sorted(u))}")
    new_catalog_svgs = {c[3] for c in covered if len(users[c[3]]) == 1}
    print(
        f"  catalog mappings using a shared SVG: {sum(1 for c in covered if len(users[c[3]]) > 1)}"
    )
    print(f"  catalog mappings with a dedicated SVG: {len(new_catalog_svgs)}")

    print("\n-- SVG style lint")
    lint_fail = 0
    warn_count = 0
    all_widths = Counter()
    for rel in sorted(on_disk):
        errors, warnings, widths = lint_svg(ICON_DIR / rel)
        all_widths.update(f"{w:g}" for w in widths)
        for e in errors:
            fail(f"{rel}: {e}")
            lint_fail += 1
        warn_count += len(warnings)
        if args.verbose:
            for w in warnings:
                print(f"  WARN: {rel}: {w}")
    print(f"  files: {len(on_disk)}  errors: {lint_fail}  warnings: {warn_count}")
    print(
        f"  stroke widths used: {', '.join(f'{w} x{n}' for w, n in sorted(all_widths.items(), key=lambda x: (
                    float(x[0])
                )))}"
    )

    if args.verbose:
        print("\n-- Mappings")
        for row, kind, key, res in covered:
            print(f"  {row['section']:<32} {kind:<6} {key:<48} {res}")

    print("\n-- Options sidebar (scripts/hake_options_icon_coverage.py)")
    import hake_options_icon_coverage as options_cov

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        options_status = options_cov.main()
    options_lines = buffer.getvalue().splitlines()
    for line in options_lines if args.verbose else [l for l in options_lines if "FAIL" in l or "RESULT" in l]:
        print(f"  {line.strip()}")
    if options_status != 0:
        fail("Options sidebar coverage failed")

    print("\n-- Generated report")
    kinds = Counter(k for k, _, _ in registry)
    print(f"  HAKE_ICONS entries: {len(registry)} ({', '.join(f'{k} {kinds[k]}' for k in REGISTRY_KINDS if kinds[k])})")
    print(f"  catalog rows covered: {len(covered)} of {len(rows) - len(excluded)}" if has_catalog else "  catalog rows covered: NOT RUN")
    for kind, (total, mapped, excepted, missing_t) in target_counts.items():
        print(f"  {SOURCE_KINDS[kind]}: {mapped} mapped, {excepted} exceptions, {missing_t} missing (of {total})")
    for label, (total, mapped, runtime) in page_counts.items():
        print(f"  {label} pages: {mapped} of {total} mapped, plus {runtime} runtime pages")
    print(f"  geometry-specific variants: {len(geometry_icons)} across {len(geometry_actions)} actions")
    print(f"  SVGs: {len(on_disk)} on disk, {len(qrc)} in qrc, {len(unused)} unreferenced, {lint_fail} lint errors")
    print(f"  documented exceptions: {len(EXCEPTIONS)}")

    print()
    if failures:
        print(f"RESULT: FAIL ({len(failures)} problem(s))")
        return 1
    print(
        f"RESULT: PASS ({len(covered)} catalog mapped, {len(excluded)} excluded, {len(CORRECTED)} corrected,"
        f" {sum(c[1] for c in target_counts.values())} source targets mapped)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
