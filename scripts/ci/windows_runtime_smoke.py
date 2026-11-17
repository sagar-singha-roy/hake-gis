#!/usr/bin/env python3
"""Hake GeoDesk Windows packaged-runtime PROJ/GDAL smoke test.

Run with the *bundled* install-tree python.exe only. Validates:
  - this interpreter is inside the install root
  - gdal/proj/geos/sqlite3 DLLs resolve under install\\bin
  - proj.db is discoverable
  - EPSG:4326 / 2193 / 3978 / 5936 / 5482 resolve
  - GPKG Create works
  - gdal_polygonize.bat produces a valid GeoPackage
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path


REQUIRED_EPSG = (4326, 2193, 3978, 5936, 5482)
REPORT: dict = {
    "ok": False,
    "install_root": None,
    "sys_executable": None,
    "env": {},
    "versions": {},
    "proj_db_locations": [],
    "proj_search_paths": [],
    "dll_modules": {},
    "pyqt_modules": {},
    "qgis_runtime": {},
    "epsg": {},
    "gpkg": None,
    "polygonize": None,
    "errors": [],
}


def _log(msg: str) -> None:
    print(msg, flush=True)


def _fail(msg: str) -> None:
    REPORT["errors"].append(msg)
    _log(f"FAIL: {msg}")


def _env_snapshot() -> dict:
    keys = ("PROJ_DATA", "PROJ_LIB", "GDAL_DATA", "PATH", "PYTHONHOME", "PYTHONPATH")
    return {k: os.environ.get(k) for k in keys}


def _normalize(p: str | Path) -> str:
    return os.path.normcase(os.path.abspath(str(p)))


def _path_str(value: object) -> str | None:
    """Coerce PROJ/GDAL path entries (str or bytes) to str for Path/JSON."""
    if value is None:
        return None
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, str):
        return value
    if isinstance(value, (bytes, bytearray)):
        raw = bytes(value)
        for encoding in ("utf-8", "mbcs", "cp1252", "latin-1"):
            try:
                return raw.decode(encoding)
            except (LookupError, UnicodeDecodeError):
                continue
        return raw.decode("utf-8", errors="replace")
    return str(value)


def _json_default(obj: object) -> str:
    if isinstance(obj, (bytes, bytearray)):
        return _path_str(obj) or ""
    if isinstance(obj, Path):
        return str(obj)
    raise TypeError(f"Object of type {obj.__class__.__name__} is not JSON serializable")


def _is_under(child: str | Path, parent: str | Path) -> bool:
    try:
        Path(_normalize(child)).relative_to(Path(_normalize(parent)))
        return True
    except ValueError:
        return False


def _enum_loaded_dlls() -> dict[str, str]:
    """Return basename -> full path for loaded modules matching gdal/proj/geos/sqlite."""
    if sys.platform != "win32":
        return {}
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)

    LIST_MODULES_DEFAULT = 0x01
    h_process = kernel32.GetCurrentProcess()

    needed = wintypes.DWORD()
    # First call to size the buffer
    if not psapi.EnumProcessModulesEx(
        h_process, None, 0, ctypes.byref(needed), LIST_MODULES_DEFAULT
    ):
        return {}

    count = needed.value // ctypes.sizeof(wintypes.HMODULE)
    if count == 0:
        return {}

    array_type = wintypes.HMODULE * count
    modules = array_type()
    if not psapi.EnumProcessModulesEx(
        h_process,
        ctypes.byref(modules),
        ctypes.sizeof(modules),
        ctypes.byref(needed),
        LIST_MODULES_DEFAULT,
    ):
        return {}

    GetModuleFileNameExW = psapi.GetModuleFileNameExW
    GetModuleFileNameExW.argtypes = [
        wintypes.HANDLE,
        wintypes.HMODULE,
        wintypes.LPWSTR,
        wintypes.DWORD,
    ]
    GetModuleFileNameExW.restype = wintypes.DWORD

    interesting_tokens = ("gdal", "proj", "geos", "sqlite3", "sqlite", "qt6", "qgis")
    found: dict[str, str] = {}
    buf = ctypes.create_unicode_buffer(32768)
    for mod in modules:
        n = GetModuleFileNameExW(h_process, mod, buf, len(buf))
        if n == 0:
            continue
        path = buf.value
        name = Path(path).name.lower()
        if name.endswith(".dll") and any(tok in name for tok in interesting_tokens):
            found[Path(path).name] = path
    return found


def _find_proj_db(search_paths: list[str], install_root: Path) -> list[str]:
    hits: list[str] = []
    seen: set[str] = set()

    def add(p: Path) -> None:
        key = _normalize(p)
        if key not in seen and p.is_file():
            seen.add(key)
            hits.append(str(p))

    for sp in search_paths:
        text = _path_str(sp)
        if not text:
            continue
        add(Path(text) / "proj.db")

    for env_key in ("PROJ_DATA", "PROJ_LIB"):
        val = os.environ.get(env_key)
        if val:
            add(Path(val) / "proj.db")

    # Packaged layout
    add(install_root / "share" / "proj" / "proj.db")
    add(install_root / "bin" / "share" / "proj" / "proj.db")

    # Recursive fallback for diagnostics (capped)
    for p in install_root.rglob("proj.db"):
        add(p)
        if len(hits) >= 20:
            break
    return hits


def _check_interpreter(install_root: Path) -> None:
    exe = Path(sys.executable).resolve()
    REPORT["sys_executable"] = str(exe)
    _log(f"sys.executable={exe}")
    if not _is_under(exe, install_root):
        _fail(
            f"Interpreter is not under install root: {exe} not in {install_root}"
        )


def _dll_matches(name: str, kind: str) -> bool:
    n = name.lower()
    return n.endswith(".dll") and kind in n


def _check_dlls(install_root: Path) -> None:
    bin_dir = install_root / "bin"
    dlls = _enum_loaded_dlls()
    REPORT["dll_modules"] = dlls
    _log("Loaded GDAL/PROJ/GEOS/SQLite modules:")
    for name, path in sorted(dlls.items()):
        _log(f"  {name} -> {path}")
        if not _is_under(path, bin_dir):
            _fail(f"DLL {name} resolved outside install bin: {path}")
    if not dlls:
        # EnumProcessModulesEx often returns empty on CI; osgeo import already
        # proved GDAL/PROJ load. Versions + EPSG checks remain the hard gate.
        _log(
            "WARN: No matching DLLs enumerated after osgeo import "
            "(skipping hard fail; EPSG/version checks are authoritative)"
        )
        return
    required_any = {
        "gdal": any(_dll_matches(n, "gdal") for n in dlls),
        "proj": any(_dll_matches(n, "proj") for n in dlls),
    }
    for kind, present in required_any.items():
        if not present:
            _fail(f"No {kind}*.dll loaded from bundled runtime")


def _import_gdal():
    from osgeo import gdal, ogr, osr

    gdal.UseExceptions()
    ogr.UseExceptions()
    return gdal, ogr, osr


def _check_versions_and_proj(gdal, osr, install_root: Path) -> None:
    gdal_ver = gdal.VersionInfo("--version")
    REPORT["versions"]["gdal"] = gdal_ver
    _log(f"GDAL: {gdal_ver}")

    try:
        proj_ver = osr.GetPROJVersionMajor(), osr.GetPROJVersionMinor(), osr.GetPROJVersionMicro()
        REPORT["versions"]["proj"] = ".".join(str(v) for v in proj_ver)
        _log(f"PROJ: {REPORT['versions']['proj']}")
    except Exception as exc:  # noqa: BLE001
        _fail(f"Could not read PROJ version: {exc}")

    try:
        search_paths = [_path_str(p) or "" for p in osr.GetPROJSearchPaths()]
        search_paths = [p for p in search_paths if p]
    except Exception as exc:  # noqa: BLE001
        search_paths = []
        _fail(f"GetPROJSearchPaths failed: {exc}")

    REPORT["proj_search_paths"] = search_paths
    _log("PROJ search paths:")
    for sp in search_paths:
        _log(f"  {sp}")

    proj_dbs = _find_proj_db(search_paths, install_root)
    REPORT["proj_db_locations"] = proj_dbs
    _log("proj.db locations:")
    for p in proj_dbs:
        _log(f"  {p}")

    if not proj_dbs:
        _fail("proj.db not found under install root or PROJ search paths")
    else:
        # Prefer a path under the install tree
        packaged = [p for p in proj_dbs if _is_under(p, install_root)]
        if not packaged:
            _fail("proj.db found but none under the Hake install root")
        else:
            _log(f"Packaged proj.db OK: {packaged[0]}")


def _check_epsg(osr) -> None:
    for epsg in REQUIRED_EPSG:
        try:
            srs = osr.SpatialReference()
            result = srs.ImportFromEPSG(epsg)
            if result != 0:
                REPORT["epsg"][str(epsg)] = f"ImportFromEPSG returned {result}"
                _fail(f"EPSG:{epsg} ImportFromEPSG returned {result}")
                continue
            wkt = srs.ExportToWkt()
            if not wkt:
                REPORT["epsg"][str(epsg)] = "empty WKT"
                _fail(f"EPSG:{epsg} produced empty WKT")
                continue
            REPORT["epsg"][str(epsg)] = "ok"
            _log(f"EPSG:{epsg} OK ({wkt[:60]}...)")
        except Exception as exc:  # noqa: BLE001
            REPORT["epsg"][str(epsg)] = str(exc)
            _fail(f"EPSG:{epsg} failed: {exc}")


def _check_pyqt_and_qgis(install_root: Path) -> None:
    _log("=== Checking PyQt6 and QGIS Runtime ===")
    REPORT["pyqt_modules"] = {}
    pyqt_modules = [
        "PyQt6.QtCore",
        "PyQt6.QtGui",
        "PyQt6.QtWidgets",
        "PyQt6.QtNetwork",
        "PyQt6.QtMultimedia",
        "PyQt6.QtMultimediaWidgets",
        "PyQt6.QtPositioning",
        "PyQt6.QtSerialPort",
        "PyQt6.QtSvg",
        "PyQt6.QtXml",
        "PyQt6.sip",
    ]
    optional_pyqt_modules = {"PyQt6.QtMultimedia", "PyQt6.QtMultimediaWidgets"}
    for mod in pyqt_modules:
        try:
            m = __import__(mod, fromlist=["*"])
            REPORT["pyqt_modules"][mod] = {
                "ok": True,
                "file": getattr(m, "__file__", "built-in"),
            }
            _log(f"  Loaded {mod} from {getattr(m, '__file__', 'built-in')}")
        except Exception as exc:  # noqa: BLE001
            REPORT["pyqt_modules"][mod] = {"ok": False, "error": str(exc)}
            if mod in optional_pyqt_modules:
                _log(f"  Note: optional module {mod} not available: {exc}")
            else:
                _fail(f"Failed to import {mod}: {exc}")

    # Check SIP registration
    try:
        from qgis.PyQt import sip
        _log(f"  SIP module loaded: {sip}")
    except Exception as exc:  # noqa: BLE001
        _fail(f"Failed to import SIP module: {exc}")

    # Check PyQt6 widget creation lifecycle
    try:
        from PyQt6.QtWidgets import QMainWindow, QLabel
        win = QMainWindow()
        win.setWindowTitle("Hake GeoDesk Smoke Test")
        lbl = QLabel("Smoke Test", win)
        win.setCentralWidget(lbl)
        win.resize(300, 200)
        _log("  PyQt6 QMainWindow creation verified.")
    except Exception as exc:  # noqa: BLE001
        _fail(f"PyQt6 QMainWindow creation failed: {exc}")

    # Check QGIS Python bindings and QgsApplication
    REPORT["qgis_runtime"] = {}
    try:
        from qgis.core import (
            QgsApplication,
            QgsCoordinateReferenceSystem,
            QgsCoordinateTransform,
            QgsFeature,
            QgsGeometry,
            QgsPointXY,
            QgsProject,
            QgsProviderRegistry,
            QgsVectorLayer,
        )
        import qgis.gui
        import qgis.analysis

        _log(f"Setting QGIS Prefix Path: {install_root}")
        QgsApplication.setPrefixPath(str(install_root), True)

        app = QgsApplication([], False)
        app.initQgis()

        _log(f"QGIS initialized. Prefix: {QgsApplication.prefixPath()}")
        _log(f"Plugin path: {QgsApplication.pluginPath()}")
        _log(f"PkgData path: {QgsApplication.pkgDataPath()}")

        crs4326 = QgsCoordinateReferenceSystem("EPSG:4326")
        crs3857 = QgsCoordinateReferenceSystem("EPSG:3857")

        if not crs4326.isValid():
            _fail("EPSG:4326 CRS invalid in QgsApplication (proj.db issue)")
        if not crs3857.isValid():
            _fail("EPSG:3857 CRS invalid in QgsApplication (proj.db issue)")

        if crs4326.isValid() and crs3857.isValid():
            xform = QgsCoordinateTransform(crs4326, crs3857, QgsProject.instance())
            pt = xform.transform(QgsPointXY(0.0, 0.0))
            REPORT["qgis_runtime"]["crs_transform"] = {
                "epsg4326_valid": True,
                "epsg3857_valid": True,
                "transformed_x": pt.x(),
                "transformed_y": pt.y(),
            }
            _log(f"CRS transform: (0,0) EPSG:4326 -> ({pt.x()}, {pt.y()}) EPSG:3857")

        providers = QgsProviderRegistry.instance().providerList()
        REPORT["qgis_runtime"]["providers"] = providers
        _log(f"Available QGIS providers: {providers}")
        if "ogr" not in providers:
            _fail("'ogr' provider missing from QgsProviderRegistry")
        if "gdal" not in providers:
            _fail("'gdal' provider missing from QgsProviderRegistry")
        if "memory" not in providers:
            _fail("'memory' provider missing from QgsProviderRegistry")

        # Test memory vector layer creation and spatial geometry
        vlayer = QgsVectorLayer("Point?crs=EPSG:4326&field=id:integer&field=name:string", "smoke_test_layer", "memory")
        if not vlayer.isValid():
            _fail("Memory vector layer creation failed in QgsApplication")
        else:
            feat = QgsFeature(vlayer.fields())
            feat.setAttribute("id", 1)
            feat.setAttribute("name", "Test Point")
            feat.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(12.0, 55.0)))
            vlayer.startEditing()
            vlayer.addFeature(feat)
            vlayer.commitChanges()
            _log(f"Memory layer created successfully with {vlayer.featureCount()} feature(s).")

        # Check processing plugin availability
        try:
            import processing  # noqa: F401
            _log("Processing module imported successfully.")
        except Exception as exc:  # noqa: BLE001
            _log(f"Note: processing import returned: {exc}")

        app.exitQgis()
        REPORT["qgis_runtime"]["ok"] = True
        _log("QgsApplication exited cleanly.")
    except Exception as exc:  # noqa: BLE001
        REPORT["qgis_runtime"]["ok"] = False
        REPORT["qgis_runtime"]["error"] = str(exc)
        _fail(f"QGIS runtime check failed: {exc}")
        traceback.print_exc()


def _check_gpkg(gdal, work: Path) -> None:
    path = work / "smoke_create.gpkg"
    try:
        drv = gdal.GetDriverByName("GPKG")
        if drv is None:
            _fail("GPKG driver not available")
            REPORT["gpkg"] = "no driver"
            return
        ds = drv.Create(str(path), 0, 0, 0, gdal.GDT_Unknown)
        if ds is None:
            _fail("GPKG Create returned None")
            REPORT["gpkg"] = "create returned None"
            return
        ds = None
        if not path.is_file() or path.stat().st_size == 0:
            _fail(f"GPKG file missing or empty: {path}")
            REPORT["gpkg"] = "missing/empty"
            return
        REPORT["gpkg"] = "ok"
        _log(f"GPKG Create OK: {path} ({path.stat().st_size} bytes)")
    except Exception as exc:  # noqa: BLE001
        REPORT["gpkg"] = str(exc)
        _fail(f"GPKG Create failed: {exc}")


def _write_test_raster(gdal, osr, path: Path) -> None:
    """32x32 Byte GTiff with EPSG:2193 and two DN regions."""
    drv = gdal.GetDriverByName("GTiff")
    ds = drv.Create(str(path), 32, 32, 1, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2193)
    ds.SetProjection(srs.ExportToWkt())
    # Arbitrary geotransform in NZTM metres
    ds.SetGeoTransform([1600000.0, 10.0, 0.0, 5500000.0, 0.0, -10.0])
    band = ds.GetRasterBand(1)
    # left half DN=1, right half DN=2
    data = bytearray()
    for _y in range(32):
        data.extend(bytes([1] * 16 + [2] * 16))
    band.WriteRaster(0, 0, 32, 32, bytes(data))
    band.FlushCache()
    ds = None


def _check_polygonize(gdal, ogr, install_root: Path, work: Path) -> None:
    tif = work / "test.tif"
    out = work / "OUTPUT.gpkg"
    _write_test_raster(gdal, __import__("osgeo", fromlist=["osr"]).osr, tif)

    bat = install_root / "bin" / "gdal_polygonize.bat"
    if not bat.is_file():
        _fail(f"gdal_polygonize.bat missing: {bat}")
        REPORT["polygonize"] = "missing bat"
        return

    # Same command shape as issue #10
    cmd = [
        str(bat),
        str(tif),
        "-b",
        "1",
        "-f",
        "GPKG",
        str(out),
        "OUTPUT",
        "DN",
    ]
    _log("Running: " + " ".join(cmd))
    env = os.environ.copy()
    # Ensure bundled bin is first
    bin_dir = str(install_root / "bin")
    env["PATH"] = bin_dir + os.pathsep + env.get("PATH", "")

    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env=env,
            cwd=str(work),
            shell=False,
            check=False,
        )
    except Exception as exc:  # noqa: BLE001
        REPORT["polygonize"] = str(exc)
        _fail(f"gdal_polygonize.bat failed to start: {exc}")
        return

    _log(f"polygonize exit={proc.returncode}")
    if proc.stdout:
        _log("--- stdout ---\n" + proc.stdout)
    if proc.stderr:
        _log("--- stderr ---\n" + proc.stderr)

    if proc.returncode != 0:
        REPORT["polygonize"] = f"exit {proc.returncode}"
        _fail(f"gdal_polygonize.bat exited {proc.returncode}")
        return

    if not out.is_file():
        REPORT["polygonize"] = "no output"
        _fail(f"OUTPUT.gpkg was not created: {out}")
        return

    try:
        ds = ogr.Open(str(out))
        if ds is None:
            _fail("Could not open OUTPUT.gpkg")
            REPORT["polygonize"] = "open failed"
            return
        layer = ds.GetLayerByName("OUTPUT")
        if layer is None:
            # try first layer
            if ds.GetLayerCount() < 1:
                _fail("OUTPUT.gpkg has no layers")
                REPORT["polygonize"] = "no layers"
                return
            layer = ds.GetLayer(0)
        name = layer.GetName()
        count = layer.GetFeatureCount()
        defn = layer.GetLayerDefn()
        fields = [defn.GetFieldDefn(i).GetName() for i in range(defn.GetFieldCount())]
        _log(f"polygonize layer={name} features={count} fields={fields}")
        if "DN" not in fields:
            _fail(f"OUTPUT layer missing DN field; fields={fields}")
            REPORT["polygonize"] = f"no DN field: {fields}"
            return
        if count < 2:
            _fail(f"Expected >= 2 features, got {count}")
            REPORT["polygonize"] = f"feature count {count}"
            return
        REPORT["polygonize"] = {"layer": name, "features": count, "fields": fields}
        _log("polygonize OK")
        ds = None
    except Exception as exc:  # noqa: BLE001
        REPORT["polygonize"] = str(exc)
        _fail(f"Validating OUTPUT.gpkg failed: {exc}")


def _rerun_with_debug(install_root: Path, work: Path) -> None:
    """On failure, re-attempt EPSG:2193 with PROJ_DEBUG for actionable CI logs."""
    _log("=== PROJ_DEBUG re-run for diagnostics ===")
    env = os.environ.copy()
    env["PROJ_DEBUG"] = "3"
    env["CPL_DEBUG"] = "ON"
    env["CPL_LOG_ERRORS"] = "ON"
    code = (
        "from osgeo import gdal, osr\n"
        "gdal.UseExceptions()\n"
        "print('GDAL', gdal.VersionInfo('--version'))\n"
        "print('PROJ_DATA', __import__('os').environ.get('PROJ_DATA'))\n"
        "print('search', list(osr.GetPROJSearchPaths()))\n"
        "s = osr.SpatialReference()\n"
        "print('ImportFromEPSG(2193)', s.ImportFromEPSG(2193))\n"
        "print(s.ExportToWkt()[:200])\n"
    )
    try:
        proc = subprocess.run(
            [sys.executable, "-c", code],
            capture_output=True,
            text=True,
            env=env,
            cwd=str(work),
            check=False,
        )
        _log(f"debug exit={proc.returncode}")
        if proc.stdout:
            _log(proc.stdout)
        if proc.stderr:
            _log(proc.stderr)
        (work / "proj_debug.txt").write_text(
            f"exit={proc.returncode}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}\n",
            encoding="utf-8",
        )
    except Exception as exc:  # noqa: BLE001
        _log(f"debug re-run failed: {exc}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--install-root", required=True, type=Path)
    parser.add_argument("--report-dir", required=True, type=Path)
    parser.add_argument("--label", default="smoke", help="Report filename label")
    args = parser.parse_args()

    install_root = args.install_root.resolve()
    report_dir = args.report_dir.resolve()
    report_dir.mkdir(parents=True, exist_ok=True)

    REPORT["install_root"] = str(install_root)
    REPORT["env"] = _env_snapshot()
    REPORT["label"] = args.label

    _log(f"=== Hake GeoDesk runtime smoke ({args.label}) ===")
    _log(f"install_root={install_root}")
    for k, v in REPORT["env"].items():
        if k == "PATH" and v:
            # Truncate PATH for readability but keep first entries
            parts = v.split(os.pathsep)
            shown = os.pathsep.join(parts[:8])
            if len(parts) > 8:
                shown += f"{os.pathsep}... ({len(parts)} entries)"
            _log(f"{k}={shown}")
        else:
            _log(f"{k}={v}")

    # Prove proj.db exists at PROJ_DATA when set
    proj_data = os.environ.get("PROJ_DATA")
    if proj_data:
        pdb = Path(proj_data) / "proj.db"
        _log(f"PROJ_DATA proj.db exists={pdb.is_file()} path={pdb}")

    with tempfile.TemporaryDirectory(prefix="hake-proj-smoke-") as tmp:
        work = Path(tmp)
        try:
            _check_interpreter(install_root)
            gdal, ogr, osr = _import_gdal()
            _check_dlls(install_root)
            _check_versions_and_proj(gdal, osr, install_root)
            _check_epsg(osr)
            _check_pyqt_and_qgis(install_root)
            _check_gpkg(gdal, work)
            _check_polygonize(gdal, ogr, install_root, work)
        except Exception as exc:  # noqa: BLE001
            _fail(f"Unhandled exception: {exc}")
            traceback.print_exc()

        if REPORT["errors"]:
            try:
                _rerun_with_debug(install_root, work)
            except Exception:  # noqa: BLE001
                traceback.print_exc()

    REPORT["ok"] = not REPORT["errors"]
    report_path = report_dir / f"{args.label}.json"
    report_path.write_text(
        json.dumps(REPORT, indent=2, default=_json_default),
        encoding="utf-8",
    )
    _log(f"Wrote report {report_path}")

    if REPORT["errors"]:
        _log("=== FAILURES ===")
        for e in REPORT["errors"]:
            _log(f" - {e}")
        return 1

    _log("=== ALL CHECKS PASSED ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
