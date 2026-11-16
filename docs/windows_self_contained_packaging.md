# Hake GeoDesk – Self-Contained Windows Packaging & Distribution

This document describes the architecture, dependency bundling, and runtime mechanics for distributing **Hake GeoDesk (Desktop GIS)** as a fully self-contained application on Microsoft Windows (x86_64).

---

## 1. Objective & User Experience

The primary objective of the self-contained Windows distribution is **zero-dependency, double-click deployment**:

* **No Manual Python Setup:** Users do not need to install Python 3 or configure system `PATH`, `PYTHONHOME`, or `PYTHONPATH`.
* **No `pip install PyQt6`:** All required PyQt6 binaries—including `PyQt6.QtMultimedia`, `PyQt6.QtPositioning`, `PyQt6.QtWidgets`, `PyQt6.QtCore`, `PyQt6.QtGui`, and `PyQt6.sip`—are pre-compiled and bundled inside the distribution.
* **No GDAL / PROJ / OSGeo4W Setup:** `gdal.dll`, `proj.dll`, `proj.db`, and GDAL data tables are fully bundled and discoverable relative to the installation root without machine-global environment variables.
* **Double-Click Execution:**
  * **NSIS Installer (`Hake-Geodesk-Installer-*.exe`):** Silent or GUI installation into `C:\Program Files\Hake GeoDesk` (or custom directory) with desktop and start menu shortcuts.
  * **Portable ZIP (`hake-gis-windows-qt6.zip`):** Unpack anywhere (including non-administrative directories or USB drives) and launch `bin\hake-geodesk.exe` directly.

---

## 2. Package Architecture

The Windows installation and portable package are structured as follows:

```text
Hake GeoDesk/
├── bin/
│   ├── hake-geodesk.exe            # Windows GUI launcher wrapper (mainwin.cpp)
│   ├── hake-geodesk-process.exe    # Headless processing engine CLI
│   ├── hake-geodesk.env            # Environment configuration template ({app}/{prefix})
│   ├── qgis_app.dll                # Core application framework library
│   ├── qgis_core.dll               # Geospatial core library
│   ├── qgis_gui.dll                # Geospatial UI and widgets library
│   ├── qgis_analysis.dll           # Spatial analysis algorithms
│   ├── qgis_3d.dll                 # 3D visualization engine
│   ├── python.exe                  # Bundled Python interpreter
│   ├── python3*.dll                # Bundled Python runtime DLL
│   ├── Qt6Core.dll                 # Qt6 core framework
│   ├── Qt6Gui.dll                  # Qt6 graphics runtime
│   ├── Qt6Widgets.dll              # Qt6 desktop widget toolkit
│   ├── Qt6Multimedia.dll           # Qt6 multimedia framework
│   ├── Qt6MultimediaWidgets.dll    # Qt6 multimedia video/audio widgets
│   ├── Qt6Positioning.dll          # Qt6 positioning library
│   ├── Qt6Svg.dll                  # Qt6 vector graphics renderer
│   ├── gdal*.dll                   # Geospatial Data Abstraction Library
│   ├── proj*.dll                   # PROJ coordinate transformation library
│   ├── geos*.dll                   # Geometry Engine Open Source
│   ├── spatialite*.dll             # SpatiaLite SQLite spatial extension
│   ├── sqlite3.dll                 # SQLite database engine
│   ├── msvcp140.dll / vcruntime*.dll # Microsoft Visual C++ runtime components
│   ├── Qt6/
│   │   └── plugins/
│   │       ├── platforms/          # qwindows.dll, qoffscreen.dll
│   │       ├── multimedia/         # windowsmediaplugin.dll, ffmpegmediaplugin.dll
│   │       ├── styles/             # Windows and modern desktop styles
│   │       ├── imageformats/       # qsvg.dll, qjpeg.dll, qico.dll, qwebp.dll
│   │       ├── iconengines/        # SVG icon engine
│   │       ├── sqldrivers/         # SQLite and PostgreSQL drivers
│   │       ├── tls/                # OpenSSL / Schannel TLS plugins
│   │       └── crypto/             # QCA cryptographic plugins
│   ├── Lib/
│   │   ├── ...                     # Python standard library modules
│   │   └── site-packages/
│   │       ├── PyQt6/              # Compiled PyQt6 extension modules (*.pyd)
│   │       │   ├── QtCore.pyd
│   │       │   ├── QtGui.pyd
│   │       │   ├── QtWidgets.pyd
│   │       │   ├── QtMultimedia.pyd
│   │       │   ├── QtMultimediaWidgets.pyd
│   │       │   ├── QtPositioning.pyd
│   │       │   ├── QtSvg.pyd
│   │       │   └── QtXml.pyd
│   │       ├── PyQt6/sip.pyd       # SIP runtime module
│   │       ├── osgeo/              # GDAL/OGR Python bindings
│   │       ├── pyproj/             # PROJ Python bindings
│   │       ├── requests/           # HTTP library
│   │       ├── yaml/               # PyYAML
│   │       └── jinja2/             # Template engine
│   └── DLLs/                       # Python C-extension DLLs (_socket, _ssl, _sqlite3)
├── python/
│   ├── qgis/                       # QGIS Python bindings
│   │   ├── __init__.py
│   │   ├── _core.pyd               # SIP-compiled C++ core binding
│   │   ├── _gui.pyd                # SIP-compiled C++ GUI binding
│   │   ├── _analysis.pyd           # SIP-compiled C++ analysis binding
│   │   ├── core/                   # Core Python package
│   │   ├── gui/                    # GUI Python package
│   │   └── analysis/               # Analysis Python package
│   └── plugins/                    # Built-in Python plugins (processing, etc.)
├── share/
│   ├── proj/
│   │   └── proj.db                 # PROJ geodetic database
│   ├── gdal/                       # GDAL coordinate system dictionaries & CSVs
│   └── hake-gis/                   # Themes (Hake Night, Clean), icons, translations
└── plugins/                        # Hake native provider plugins (provider_postgres.dll, etc.)
```

---

## 3. Runtime Launcher & Environment Mechanics

When launching `hake-geodesk.exe`, the wrapper executable ([`src/app/mainwin.cpp`](file:///src/app/mainwin.cpp)) initializes a pristine, self-contained environment before loading any application DLLs:

### 1. Dynamic Path Resolution (`{app}` and `{prefix}`)
The launcher dynamically locates itself at runtime:
* `{app}` is resolved to the directory containing the executable (`.../bin`).
* `{prefix}` is resolved to the parent installation root directory (`.../`).

### 2. Environment Configuration (`hake-geodesk.env`)
The launcher reads [`hake-geodesk.env`](file:///ms-windows/installer/hake-gis.env.in) and applies the variables directly to the process environment via `_wputenv`:

```ini
PATH={app}
QGIS_PREFIX_PATH={prefix}
PROJ_DATA={prefix}\share\proj
PROJ_LIB={prefix}\share\proj
GDAL_DATA={prefix}\share\gdal
QT_PLUGIN_PATH={app}\Qt6\plugins;{app}\Qt6\plugins\crypto;{app}\plugins
PYTHONHOME={app}
PYTHONPATH={prefix}\python;{app}\Lib;{app}\Lib\site-packages;{app}\DLLs
```

* **Short PATH Precedence:** Prepends `{app}` to `PATH` so bundled DLLs resolve immediately without overflowing the Windows 32KB environment block limit.
* **PROJ Isolation:** Sets both `PROJ_DATA` and `PROJ_LIB` to `{prefix}\share\proj`, preventing host GIS tools (e.g. PostgreSQL/PostGIS) from corrupting CRS transformations with an incompatible older `proj.db`.
* **Python Isolation:** Sets `PYTHONHOME={app}` and `PYTHONPATH` to load the bundled standard library, site-packages, and QGIS bindings first.

### 3. Windows DLL Search Directory Hardening
The launcher invokes Windows API `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)` and `AddDllDirectory(appDir)`. This ensures that even before system `PATH` is searched, Windows prioritizes the application's `bin\` directory for resolving linked DLLs.

---

## 4. Root Cause Analysis & Fix for PyQt6.QtMultimedia

### Previous Error
```text
RuntimeError: the PyQt6.QtMultimedia module failed to register with the sip module
```
Or:
```text
ModuleNotFoundError: No module named 'PyQt6.QtMultimedia'
```

### Root Cause
1. **Missing Feature in `vcpkg/vcpkg.json`:** The `py-pyqt6` dependency in `vcpkg/vcpkg.json` declared features `[ "qt3d", "qt5compat", "qtcharts", "qtdeclarative", "qtpositioning", "qtserialport" ]`—**`qtmultimedia` was absent**. Consequently, vcpkg never compiled `QtMultimedia.pyd` or `Qt6Multimedia.dll` for Windows.
2. **Incompatible Python Mocking:** Earlier workarounds injected fake Python `types.ModuleType` objects into `sys.modules['PyQt6.QtMultimedia']`. SIP requires compiled C extension structs (`sipExportedModule_...`); pure Python mock modules lack these binary headers, causing SIP to reject the module with a registration failure when `qgis.gui` imported it.

### Permanent Resolution
1. **Enabled `qtmultimedia` Feature in vcpkg:**
   In [`vcpkg/vcpkg.json`](file:///vcpkg/vcpkg.json):
   ```json
   {
     "name": "py-pyqt6",
     "features": [
       "qt3d",
       "qt5compat",
       "qtcharts",
       "qtdeclarative",
       "qtmultimedia",
       "qtpositioning",
       "qtserialport"
     ]
   }
   ```
   This ensures vcpkg compiles genuine binary modules `QtMultimedia.pyd` and `QtMultimediaWidgets.pyd`, and links against `Qt6Multimedia.dll`.

2. **Suppressed Spurious Error Dialogs:**
   Updated [`src/python/qgspythonutilsimpl.cpp`](file:///src/python/qgspythonutilsimpl.cpp) to verify required PyQt6 binary modules safely, suppressing raw Python code popups and returning platform-specific actionable diagnostic messages if any component is missing.

3. **Bundling in `VcpkgInstallDeps.cmake`:**
   `cmake/VcpkgInstallDeps.cmake` automatically bundles:
   * All `${VCPKG_BASE_DIR}/bin/*.dll` into `bin/`.
   * All `${VCPKG_BASE_DIR}/Qt6/plugins/` (including `multimedia/` and `platforms/`) into `bin/Qt6/plugins/`.
   * All `${VCPKG_BASE_DIR}/tools/python3/` (including `Lib/site-packages/PyQt6/`) into `bin/`.

---

## 5. CI/CD & Automated Smoke Tests

The Windows GitHub Actions workflow [`.github/workflows/windows-qt6.yml`](file:///.github/workflows/windows-qt6.yml) builds and validates both distributables on a clean Microsoft Windows runner:

1. **`AssertZip`:** Inspects the generated CPack ZIP archive without extraction, verifying presence of:
   * `bin/hake-geodesk.exe` and `bin/hake-geodesk.env`
   * `bin/python.exe` and `bin/Lib/site-packages/PyQt6/`
   * `bin/Qt6Multimedia.dll` and `bin/Lib/site-packages/PyQt6/QtMultimedia*.pyd`
   * `bin/Lib/site-packages/PyQt6/QtPositioning*.pyd`
   * `bin/Qt6/plugins/platforms/qwindows.dll`
   * `share/proj/proj.db` and `share/gdal/*`
   * `python/qgis/core/__init__.py` and `python/qgis/gui/__init__.py`
2. **`Runtime` Smoke Test:** Silently installs the NSIS installer into `C:\Hake GeoDesk` and executes [`scripts/ci/windows_runtime_smoke.py`](file:///scripts/ci/windows_runtime_smoke.py):
   * Validates `PyQt6` imports: `QtCore`, `QtGui`, `QtWidgets`, `QtMultimedia`, `QtMultimediaWidgets`, `QtPositioning`, `QtSvg`, `QtXml`.
   * Verifies SIP module loading and registration.
   * Initializes `QgsApplication` in headless mode (`initQgis` / `exitQgis`).
   * Validates CRS transformations (`EPSG:4326` to `EPSG:3857`) using bundled `proj.db`.
   * Verifies built-in data providers (`ogr`, `gdal`).
   * Verifies loaded DLLs originate exclusively from `bin/` and `System32`.
   * Runs `gdal_polygonize.bat` to produce and validate an output GeoPackage.
3. **`Portable` Smoke Test:** Extracts the portable ZIP to a clean directory and executes the complete smoke test suite against the unpacked portable tree.
4. **`Process` CLI Test:** Invokes `bin/hake-geodesk-process.exe` to run a headless polygonize workflow.

---

## 6. How to Test on a Clean Windows Environment

To verify that the Windows package runs without dependencies on a completely clean machine (or VM):

1. **Prepare Clean Machine:** Use a fresh Windows 10/11 VM without Python, Qt, Visual Studio, or OSGeo4W installed.
2. **Download Artifacts:**
   * Download `hake-geodesk-installer-win64` (EXE installer) or `hake-gis-windows-qt6` (portable ZIP) from GitHub Actions or Releases.
3. **Option A: Test NSIS Installer**
   * Double-click `Hake-Geodesk-Installer-*.exe` and complete the installation.
   * Launch **Hake GeoDesk** from the Desktop shortcut or Start Menu.
   * Open the Python Console: verify `import PyQt6.QtMultimedia; import PyQt6.QtPositioning` succeeds without popups.
4. **Option B: Test Portable ZIP**
   * Right-click the `.zip` file and extract to any folder (e.g. `C:\GeoDeskPortable`).
   * Navigate to `bin\` and double-click `hake-geodesk.exe`.
   * Verify the application starts normally and opens projects.
