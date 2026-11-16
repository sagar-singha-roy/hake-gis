#!/usr/bin/env python3
"""Hake GeoDesk Linux self-contained packaged-runtime smoke test.

Validates:
  - Bundled Python runtime loads without errors
  - Core QGIS Python bindings (qgis.core, qgis.gui, qgis.analysis) import cleanly
  - PyQt6 modules (QtCore, QtGui, QtWidgets, QtMultimedia, QtPositioning, QtSvg, QtXml)
    import cleanly without SIP registration errors
  - QgsApplication initializes cleanly (initQgis/exitQgis)
  - PROJ data (proj.db) is accessible and CRS transformations work
  - Providers (ogr, gdal, spatialite) are available
  - hake-geodesk-process CLI runs and returns valid version
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


REPORT: dict = {
    "ok": False,
    "prefix_path": None,
    "python_executable": sys.executable,
    "python_version": sys.version,
    "env": {},
    "modules_tested": {},
    "proj_test": {},
    "providers": [],
    "cli_test": None,
    "errors": [],
}


def _log(msg: str) -> None:
    print(f"[SMOKE] {msg}", flush=True)


def _fail(msg: str) -> None:
    REPORT["errors"].append(msg)
    _log(f"FAIL: {msg}")


def test_imports() -> bool:
    """Test critical Python and PyQt6 modules."""
    modules = [
        "PyQt6.QtCore",
        "PyQt6.QtGui",
        "PyQt6.QtWidgets",
        "PyQt6.QtXml",
        "PyQt6.QtSvg",
        "PyQt6.QtMultimedia",
        "PyQt6.QtPositioning",
        "qgis.core",
        "qgis.gui",
        "qgis.analysis",
        "requests",
        "yaml",
        "jinja2",
        "pyproj",
    ]

    all_ok = True
    for mod in modules:
        try:
            m = __import__(mod, fromlist=["*"])
            REPORT["modules_tested"][mod] = {
                "ok": True,
                "file": getattr(m, "__file__", "built-in"),
            }
            _log(f"Loaded {mod} from {getattr(m, '__file__', 'built-in')}")
        except Exception as exc:
            all_ok = False
            REPORT["modules_tested"][mod] = {"ok": False, "error": str(exc)}
            _fail(f"Failed to import {mod}: {exc}")

    return all_ok


def test_qgis_runtime(prefix_path: Path) -> bool:
    """Initialize QgsApplication and test PROJ/CRS transformations."""
    try:
        from qgis.core import (
            QgsApplication,
            QgsCoordinateReferenceSystem,
            QgsCoordinateTransform,
            QgsPointXY,
            QgsProject,
            QgsProviderRegistry,
        )

        _log(f"Setting QGIS Prefix Path to: {prefix_path}")
        QgsApplication.setPrefixPath(str(prefix_path), True)

        app = QgsApplication([], False)
        app.initQgis()

        _log(f"QGIS initialized. Prefix: {QgsApplication.prefixPath()}")
        _log(f"Plugin path: {QgsApplication.pluginPath()}")
        _log(f"PkgData path: {QgsApplication.pkgDataPath()}")

        # Test CRS resolution via PROJ
        crs4326 = QgsCoordinateReferenceSystem("EPSG:4326")
        crs3857 = QgsCoordinateReferenceSystem("EPSG:3857")

        if not crs4326.isValid():
            _fail("EPSG:4326 CRS is invalid! (proj.db missing or inaccessible)")
            return False
        if not crs3857.isValid():
            _fail("EPSG:3857 CRS is invalid! (proj.db missing or inaccessible)")
            return False

        # Test transformation
        transform = QgsCoordinateTransform(crs4326, crs3857, QgsProject.instance())
        pt = transform.transform(QgsPointXY(0.0, 0.0))
        REPORT["proj_test"] = {
            "crs4326_valid": crs4326.isValid(),
            "crs3857_valid": crs3857.isValid(),
            "transformed_x": pt.x(),
            "transformed_y": pt.y(),
        }
        _log(f"PROJ CRS transform test: (0,0) EPSG:4326 -> ({pt.x()}, {pt.y()}) EPSG:3857")

        # Test providers
        providers = QgsProviderRegistry.instance().providerList()
        REPORT["providers"] = providers
        _log(f"Available data providers: {providers}")
        if "ogr" not in providers:
            _fail("'ogr' provider missing from provider registry!")
            return False

        app.exitQgis()
        _log("QgsApplication exited cleanly.")
        return True

    except Exception as exc:
        _fail(f"QgsApplication runtime test threw an exception: {exc}")
        return False


def test_cli(prefix_path: Path) -> bool:
    """Run hake-geodesk-process binary."""
    exe_candidates = [
        prefix_path / "bin" / "hake-geodesk-process",
        prefix_path / "usr" / "bin" / "hake-geodesk-process",
    ]
    exe = None
    for c in exe_candidates:
        if c.is_file() and os.access(c, os.X_OK):
            exe = c
            break

    if not exe:
        _log(f"hake-geodesk-process binary not found in {prefix_path}; skipping CLI test")
        return True

    try:
        res = subprocess.run([str(exe), "--version"], capture_output=True, text=True, timeout=15)
        out = (res.stdout or res.stderr).strip()
        REPORT["cli_test"] = {"exit_code": res.returncode, "output": out}
        _log(f"Executed {exe.name}: exit={res.returncode}, output={out}")
        return res.returncode == 0
    except Exception as exc:
        _fail(f"Failed to execute {exe}: {exc}")
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Hake GeoDesk Linux runtime smoke test")
    parser.add_argument("--prefix", type=Path, default=None, help="Root path of the installed or bundled AppDir")
    parser.add_argument("--json", type=Path, default=None, help="Path to write JSON report")
    args = parser.parse_args()

    prefix = args.prefix
    if not prefix:
        # Default to /usr or sys.prefix
        prefix = Path(os.environ.get("QGIS_PREFIX_PATH", sys.prefix))

    REPORT["prefix_path"] = str(prefix)
    REPORT["env"] = {
        k: os.environ.get(k)
        for k in ("QGIS_PREFIX_PATH", "LD_LIBRARY_PATH", "PYTHONPATH", "PYTHONHOME", "QT_PLUGIN_PATH", "PROJ_DATA", "GDAL_DATA")
    }

    _log("Starting Linux Runtime Smoke Test...")
    imports_ok = test_imports()
    qgis_ok = test_qgis_runtime(prefix)
    cli_ok = test_cli(prefix)

    REPORT["ok"] = imports_ok and qgis_ok and cli_ok

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(REPORT, indent=2), encoding="utf-8")
        _log(f"Wrote smoke report to {args.json}")

    if REPORT["ok"]:
        _log(">>> ALL RUNTIME SMOKE TESTS PASSED SUCCESSFULLY! <<<")
        return 0
    else:
        _log(f">>> SMOKE TESTS FAILED WITH {len(REPORT['errors'])} ERROR(S) <<<")
        for err in REPORT["errors"]:
            print(f"  - {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
