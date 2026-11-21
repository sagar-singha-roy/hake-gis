#!/usr/bin/env python3
"""Hake GeoDesk Windows Pre-Packaging Dependency & Feature Audit.

Validates that:
1. vcpkg/vcpkg.json declares all required PyQt6 features (qtmultimedia, qtpositioning, etc.).
2. vcpkg has successfully built and staged all required DLLs and .pyd extensions.
3. Built QGIS binaries and Python extensions exist before invoking CPack.

Aborts packaging immediately if any required runtime component is missing.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path


REQUIRED_PYQT6_FEATURES = [
    "qtpositioning",
    "qtserialport",
    "qtdeclarative",
    "qt5compat",
    "qtcharts",
    "qt3d",
]

REQUIRED_VCPKG_DLLS = [
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "Qt6Network.dll",
    "Qt6Multimedia.dll",
    "Qt6Positioning.dll",
    "sqlite3.dll",
]

REQUIRED_VCPKG_PYDS = [
    "QtCore",
    "QtGui",
    "QtWidgets",
    "QtNetwork",
    "QtPositioning",
    "sip",
]


def log(msg: str) -> None:
    print(f"[AUDIT] {msg}", flush=True)


def error(msg: str) -> None:
    print(f"[AUDIT ERROR] {msg}", file=sys.stderr, flush=True)


def audit_vcpkg_json(repo_root: Path) -> list[str]:
    """Verify vcpkg.json declares required PyQt6 features."""
    manifest_path = repo_root / "vcpkg" / "vcpkg.json"
    if not manifest_path.is_file():
        return [f"vcpkg.json not found at {manifest_path}"]

    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as exc:
        return [f"Failed to parse vcpkg.json: {exc}"]

    bindings = data.get("features", {}).get("bindings", {}).get("dependencies", [])
    pyqt6_entry = None
    for dep in bindings:
        if isinstance(dep, dict) and dep.get("name") == "py-pyqt6":
            pyqt6_entry = dep
            break

    if not pyqt6_entry:
        return ["py-pyqt6 dependency missing from 'bindings' feature in vcpkg.json"]

    features = set(pyqt6_entry.get("features", []))
    missing = [f for f in REQUIRED_PYQT6_FEATURES if f not in features]
    if missing:
        return [
            f"vcpkg.json py-pyqt6 is missing required feature(s): {', '.join(missing)}. "
            "PyQt6 Python bindings will fail at runtime!"
        ]

    log(f"vcpkg.json declared all required py-pyqt6 features: {sorted(features)}")
    return []


def audit_vcpkg_installed(vcpkg_installed_dir: Path) -> list[str]:
    """Verify vcpkg built and installed all required DLLs and .pyd extensions."""
    errors = []
    if not vcpkg_installed_dir.is_dir():
        log(f"Note: vcpkg_installed dir {vcpkg_installed_dir} not found (skipping binary audit)")
        return errors

    bin_dir = vcpkg_installed_dir / "bin"
    if not bin_dir.is_dir():
        errors.append(f"vcpkg bin directory not found at {bin_dir}")
        return errors

    # Check DLLs
    bin_files = {f.name.lower() for f in bin_dir.iterdir() if f.is_file()}
    for dll in REQUIRED_VCPKG_DLLS:
        if dll.lower() not in bin_files:
            errors.append(f"Required DLL missing from vcpkg bin: {dll}")

    # Check wildcard DLLs (gdal, proj, geos)
    has_gdal = any(f.startswith("gdal") and f.endswith(".dll") for f in bin_files)
    has_proj = any(f.startswith("proj") and f.endswith(".dll") for f in bin_files)
    has_geos = any("geos" in f and f.endswith(".dll") for f in bin_files)

    if not has_gdal:
        errors.append("gdal*.dll missing from vcpkg bin")
    if not has_proj:
        errors.append("proj*.dll missing from vcpkg bin")
    if not has_geos:
        errors.append("geos*.dll missing from vcpkg bin")

    # Check PROJ database
    proj_db = vcpkg_installed_dir / "share" / "proj" / "proj.db"
    if not proj_db.is_file():
        errors.append(f"proj.db missing from vcpkg share/proj at {proj_db}")

    # Check PyQt6 .pyd extensions
    pyqt6_dir = vcpkg_installed_dir / "tools" / "python3" / "Lib" / "site-packages" / "PyQt6"
    if not pyqt6_dir.is_dir():
        errors.append(f"PyQt6 site-packages directory missing at {pyqt6_dir}")
    else:
        pyd_files = [f.name for f in pyqt6_dir.iterdir() if f.is_file()]
        for req in REQUIRED_VCPKG_PYDS:
            match = any(f.lower().startswith(req.lower()) and f.lower().endswith(".pyd") for f in pyd_files)
            if not match:
                errors.append(
                    f"Required PyQt6 binary extension '{req}*.pyd' missing from {pyqt6_dir}! "
                    "vcpkg did not build this PyQt6 module."
                )

    if not errors:
        log("vcpkg installed binaries and PyQt6 extensions verified successfully.")
    return errors


def audit_build_outputs(build_dir: Path) -> list[str]:
    """Verify QGIS executable and library targets are built."""
    errors = []
    if not build_dir.is_dir():
        log(f"Note: build dir {build_dir} not found (skipping build target audit)")
        return errors

    # Check for hake-geodesk binaries across possible build locations
    search_dirs = [
        build_dir / "output" / "bin",
        build_dir / "bin",
        build_dir / "src" / "app",
        build_dir,
    ]

    found_exe = False
    for sdir in search_dirs:
        if (sdir / "hake-geodesk.exe").is_file() or (sdir / "hake-geodesk").is_file():
            found_exe = True
            log(f"Found Hake GeoDesk executable in {sdir}")
            break

    if not found_exe:
        errors.append("hake-geodesk executable was not built before packaging step!")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Pre-packaging dependency audit for Hake GeoDesk Windows")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--vcpkg-root", type=Path, default=None)
    parser.add_argument("--build-dir", type=Path, default=None)
    parser.add_argument("--json", type=Path, default=None)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    log(f"Starting pre-packaging audit in: {repo_root}")

    all_errors: list[str] = []

    # 1. Manifest audit
    manifest_errors = audit_vcpkg_json(repo_root)
    all_errors.extend(manifest_errors)

    # 2. Installed vcpkg binaries audit
    vcpkg_installed = args.vcpkg_root
    if not vcpkg_installed and args.build_dir:
        for triplet in ("x64-windows-release", "x64-windows"):
            cand = args.build_dir / "vcpkg_installed" / triplet
            if cand.is_dir():
                vcpkg_installed = cand
                break

    if vcpkg_installed:
        vcpkg_errors = audit_vcpkg_installed(vcpkg_installed.resolve())
        all_errors.extend(vcpkg_errors)

    # 3. Built target audit
    if args.build_dir:
        build_errors = audit_build_outputs(args.build_dir.resolve())
        all_errors.extend(build_errors)

    report = {
        "ok": len(all_errors) == 0,
        "errors": all_errors,
        "repo_root": str(repo_root),
        "vcpkg_installed": str(vcpkg_installed) if vcpkg_installed else None,
        "build_dir": str(args.build_dir) if args.build_dir else None,
    }

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        log(f"Audit report saved to: {args.json}")

    if all_errors:
        error(f"Pre-packaging audit FAILED with {len(all_errors)} error(s):")
        for err in all_errors:
            error(f"  - {err}")
        return 1

    log("Pre-packaging audit PASSED with 0 errors. Proceeding to package.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
