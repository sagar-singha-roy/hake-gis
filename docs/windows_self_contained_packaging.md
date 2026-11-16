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

## 4. Why the Previously Working Build Started Failing (Root Cause Analysis)

### The Failing Error
```text
RuntimeError: the PyQt6.QtMultimedia module failed to register with the sip module
```
Or:
```text
ModuleNotFoundError: No module named 'PyQt6.QtMultimedia'
```

### Deep Root Cause Investigation
1. **Initial Missing Feature in `vcpkg/vcpkg.json`:**
   When the project was initially configured, `py-pyqt6` in `vcpkg/vcpkg.json` declared features `[ "qt3d", "qt5compat", "qtcharts", "qtdeclarative", "qtpositioning", "qtserialport" ]`. Although C++ `qtmultimedia` was listed under root dependencies, **`qtmultimedia` was omitted from `py-pyqt6`**. Consequently, vcpkg never compiled `QtMultimedia.pyd` or `QtMultimediaWidgets.pyd` for Windows.

2. **Why Earlier Builds Appeared to Pass:**
   In earlier Windows CI runs, `windows_runtime_smoke.py` only validated `gdal_polygonize.bat` via CLI and tested basic coordinate transforms in `osgeo`. It **never imported `qgis.core` or `qgis.gui`**, and never imported PyQt6 modules in CI. Because `hake-geodesk-process.exe` operates without initializing the GUI Python runtime, the CI reported green even though `QtMultimedia.pyd` was completely missing from the package.

3. **The Incompatible Mock Injection:**
   When Linux testing surfaced `ModuleNotFoundError: No module named 'PyQt6.QtMultimedia'`, an attempt was made to stub the module using `_fake_mm = types.ModuleType('PyQt6.QtMultimedia')` in `python/PyQt6/core/__init__.py.in`, `gui/__init__.py.in`, and `src/python/qgspythonutilsimpl.cpp`.
   However, SIP (`sipbuild`) generates C extension code (`sipExportedModule_...`) that expects imported Qt modules to be compiled C++ SIP modules with registered type tables. When `qgis._gui` or `qgis._core` imported the pure-Python mock `types.ModuleType`, SIP failed type registration and threw:
   `RuntimeError: the PyQt6.QtMultimedia module failed to register with the sip module`.
   Additionally, in `src/app/qgisapp.cpp`, when Python initialization failed, the application attempted to execute Python commands on an uninitialized/partially-unwound interpreter, leading to:
   `SystemError: globals must be a real dict`.

4. **CI Artifact Upload Gap:**
   In `.github/workflows/windows-qt6.yml`, the artifact upload steps used `if: always()`, which uploaded broken installers and portable ZIPs even when tests or smoke verifications failed.

---

## 5. Permanent Architecture & Prevention of Future Regressions

To ensure this issue and similar dependency gaps can never recur silently, the deployment architecture is now protected by a multi-layered gate system:

### Layer 1: Manifest Definition in `vcpkg/vcpkg.json`
`vcpkg/vcpkg.json` explicitly pins all required PyQt6 features:
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

### Layer 2: Pre-Packaging Audit Gate (`scripts/ci/windows_prepackage_audit.py`)
Runs on the runner **before** `cmake --build build --target bundle`. It verifies:
- `vcpkg.json` contains `qtmultimedia`, `qtpositioning`, `qtserialport`, `qtdeclarative`, `qt5compat`, `qtcharts`, `qt3d`.
- `vcpkg_installed/x64-windows/tools/python3/Lib/site-packages/PyQt6/` contains `QtMultimedia*.pyd`, `QtPositioning*.pyd`, `QtCore*.pyd`, `QtGui*.pyd`, `QtWidgets*.pyd`, `sip*.pyd`.
- `vcpkg_installed/x64-windows/bin/` contains `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Multimedia.dll`, `Qt6Positioning.dll`.
- If any component is missing, the build **fails immediately with an explicit error**, preventing CPack from packaging an incomplete bundle.

### Layer 3: Package Structure & ZIP Assertions (`scripts/ci/windows_runtime_smoke.ps1`)
Inspects the generated CPack ZIP archive directly without extraction:
- Asserts all required Qt6 DLLs, QGIS DLLs, and executables (`hake-geodesk.exe`, `hake-geodesk-process.exe`).
- Asserts all required `.pyd` extensions (`QtCore`, `QtGui`, `QtWidgets`, `QtMultimedia`, `QtPositioning`, `sip`, `_core`, `_gui`).
- Asserts Qt platform plugins (`qwindows.dll`) and multimedia plugins.
- Asserts `proj.db`, GDAL data, and `hake-geodesk.env`.

### Layer 4: Clean-Machine Isolated Runtime Smoke Test (`scripts/ci/windows_runtime_smoke.py`)
Tests both the silent NSIS install (`C:\Hake GeoDesk`) and the extracted portable ZIP:
- **Executable Version Invocations:** Runs `hake-geodesk.exe --version` and `hake-geodesk-process.exe --version` to prove dynamic linking and runtime DLL loading succeed without Windows DLL load errors.
- **Isolated Environment:** Purges machine `PATH`, unsets host `PROJ_DATA`/`PROJ_LIB`/`GDAL_DATA`, and forces the bundled `python.exe` to resolve everything relative to `bin\`.
- **PyQt6 & SIP Validation:** Imports `PyQt6.QtCore`, `QtGui`, `QtWidgets`, `QtMultimedia`, `QtMultimediaWidgets`, `QtPositioning`, `QtSerialPort`, `QtSvg`, `QtXml`, and `sip`.
- **UI Lifecycle:** Creates a `QMainWindow` and central widgets to ensure Qt desktop widgets initialize cleanly.
- **QGIS Runtime Initialization:** Initializes `QgsApplication` in headless mode, tests coordinate transforms (`EPSG:4326 -> EPSG:3857`), verifies providers (`ogr`, `gdal`, `memory`), and creates in-memory vector layers with geometry.
- **Processing Validation:** Validates `import processing` and runs `hake-geodesk-process.exe run gdal:polygonize`.

### Layer 5: Release Gating (`if: success()`)
In `.github/workflows/windows-qt6.yml`, artifact uploads and release asset publishing are strictly gated on `if: success()`. If any audit or smoke test step fails, the workflow uploads runtime smoke diagnostics for debugging, but **no installer or ZIP is published**.

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
