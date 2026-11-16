# Hake GeoDesk – Self-Contained Linux Packaging & Distribution

This document describes the architecture, packaging process, and runtime mechanics for distributing **Hake GeoDesk (Desktop GIS)** as a fully self-contained application on Linux.

---

## 1. Objective & User Experience

The primary objective of the self-contained Linux distribution is **zero-dependency deployment**:

* **No Terminal Commands:** End users do not need to open a terminal or execute shell scripts.
* **No `sudo apt install`:** No system package manager intervention or root privileges are required.
* **No Python / PyQt6 Dependency:** Python 3, PyQt6 (including `QtMultimedia` and `QtPositioning`), SIP, and geospatial libraries are bundled directly inside the package.
* **Double-Click Execution:** The user downloads `Hake-GeoDesk-x86_64.AppImage` (or extracts `Hake-Geodesk-Linux-x86_64.tar.gz`), double-clicks the application icon, and Hake GeoDesk launches immediately on any modern Linux distribution (Ubuntu, Debian, Fedora, Arch, openSUSE, Mint, Rocky Linux, etc.).

---

## 2. Package Architecture

The self-contained package is structured according to the standard **AppDir** specification:

```text
Hake-GeoDesk.AppDir/
├── AppRun                          # Entrypoint launcher configuring pristine isolation
├── hake-geodesk.desktop            # Standard XDG Desktop Entry (Name, Icon, Exec, MIME types)
├── hake-gis.png                    # 512x512 high-resolution application icon
├── hake-gis.svg                    # Scalable vector application icon
├── .DirIcon                        # File manager thumbnail icon
└── usr/
    ├── bin/
    │   ├── hake-geodesk            # Main desktop application binary
    │   ├── hake-geodesk-process    # Headless processing engine CLI
    │   ├── crssync                 # Coordinate reference system synchronization tool
    │   └── python3                 # Bundled Python runtime interpreter
    ├── lib/
    │   ├── libhakegis_app.so*      # Hake application framework library
    │   ├── libhakegis_core.so*     # Hake geospatial core library
    │   ├── libhakegis_gui.so*      # Hake GIS UI library
    │   ├── libhakegis_analysis.so* # Spatial analysis algorithms
    │   ├── libhakegis_3d.so*       # 3D visualization engine
    │   ├── libQt6*.so*             # Bundled Qt6 runtime libraries (Core, Gui, Widgets, Multimedia, etc.)
    │   ├── libgdal.so*             # Geospatial Data Abstraction Library
    │   ├── libproj.so*             # Coordinate transformation engine
    │   ├── libgeos*.so*            # Geometry Engine Open Source
    │   ├── libspatialite.so*       # SpatiaLite SQLite extension
    │   ├── libpython3.*.so*        # Python C library
    │   ├── hake-gis/plugins/*.so   # Built-in data provider plugins (postgres, ogr, gdal, wms, etc.)
    │   └── python3.X/              # Python standard library & lib-dynload C extensions
    │       └── dist-packages/
    │           ├── PyQt6/          # Full PyQt6 binaries (QtCore, QtGui, QtWidgets, QtMultimedia, etc.)
    │           ├── PyQt6.sip       # SIP C-extension runtime
    │           ├── qgis/           # QGIS Python bindings (_core, _gui, _analysis, utils.py)
    │           └── ...             # Plugin dependencies (requests, yaml, jinja2, pyproj, etc.)
    ├── plugins/                    # Qt6 plugins
    │   ├── platforms/              # libqxcb.so, libqwayland-generic.so, libqoffscreen.so
    │   ├── imageformats/           # libqsvg.so, libqjpeg.so, libqico.so, libqwebp.so
    │   ├── xcbglintegrations/      # XCB GLX / EGL integration
    │   ├── sqldrivers/             # SQLite and spatial database drivers
    │   └── tls/                    # TLS / OpenSSL backend
    ├── share/
    │   ├── hake-gis/               # Themes (Hake Night, Clean), SVG symbols, i18n translations
    │   ├── proj/                   # proj.db database for coordinate reference systems
    │   └── gdal/                   # Coordinate system tables and datum shift grids
    └── etc/ssl/certs/
        └── ca-certificates.crt     # Mozilla CA certificates for HTTPS plugin downloads
```

---

## 3. Runtime Environment Isolation (`AppRun`)

When the user launches the AppImage or double-clicks `AppRun`, the launcher script guarantees complete isolation from incompatible host system libraries:

1. **Self-Location (`APPDIR`):** Dynamically resolves its absolute location on disk (or FUSE mountpoint).
2. **Library Precedence (`LD_LIBRARY_PATH`):** Forces the Linux dynamic linker to search `${APPDIR}/usr/lib` and `${APPDIR}/usr/lib/hake-gis` before examining host directories.
3. **Python Isolation (`PYTHONHOME` & `PYTHONNOUSERSITE=1`):**
   * Sets `PYTHONHOME="${APPDIR}/usr"`.
   * Sets `PYTHONNOUSERSITE=1` to prevent broken packages in `~/.local/lib/python3.X` from poisoning the bundled runtime.
   * Configures `PYTHONPATH` to load the bundled `PyQt6` and `qgis` packages first.
4. **Qt6 Platform & Plugins (`QT_PLUGIN_PATH`):** Points Qt to `${APPDIR}/usr/plugins`, enabling both X11 (XCB) and Wayland desktops out-of-the-box.
5. **Geospatial Definitions (`PROJ_DATA` & `GDAL_DATA`):** Points PROJ to `${APPDIR}/usr/share/proj` (containing `proj.db`), ensuring projections and datum transformations work without host PROJ packages.

---

## 4. How to Build the Linux Distributables

### Automated CI/CD (GitHub Actions)
The workflow [`.github/workflows/linux-qt6.yml`](file:///.github/workflows/linux-qt6.yml) automatically:
1. Compiles Hake GeoDesk in Release mode with Ninja and ccache.
2. Invokes [`scripts/package_linux_appimage.sh`](file:///scripts/package_linux_appimage.sh) to assemble the AppDir, bundle all shared libraries, and build:
   * **`Hake-GeoDesk-x86_64.AppImage`**
   * **`Hake-Geodesk-Linux-x86_64.tar.gz`**
3. Runs the clean-environment runtime smoke test [`scripts/ci/linux_runtime_smoke.py`](file:///scripts/ci/linux_runtime_smoke.py).
4. Uploads the distributables as GitHub Actions artifacts and Release assets.

### Building Locally on Linux
On an Ubuntu 24.04 or compatible Linux workstation:

```bash
# 1. Configure and build
cmake -S . -B build -G Ninja \
      -D CMAKE_BUILD_TYPE=Release \
      -D WITH_DESKTOP=ON \
      -D WITH_BINDINGS=ON \
      -D CREATE_ZIP=ON \
      -D CREATE_DEB=ON
cmake --build build --config Release

# 2. Package self-contained AppImage & portable bundle
bash scripts/package_linux_appimage.sh --build-dir build --output-dir dist

# 3. Verify in clean environment
python3 scripts/ci/linux_runtime_smoke.py --prefix build/Hake-GeoDesk.AppDir/usr
```

---

## 5. End-User Instructions

### Using the AppImage (Recommended)
1. Download `Hake-GeoDesk-x86_64.AppImage`.
2. Right-click the file $\rightarrow$ **Properties** $\rightarrow$ **Permissions** $\rightarrow$ Check **"Allow executing file as program"** (or in terminal: `chmod +x Hake-GeoDesk-x86_64.AppImage`).
3. Double-click `Hake-GeoDesk-x86_64.AppImage` to launch.

### Using the Portable Tarball
1. Download and extract `Hake-Geodesk-Linux-x86_64.tar.gz`.
2. Open the extracted `Hake-GeoDesk.AppDir` folder.
3. Double-click **`AppRun`** to launch Hake GeoDesk.
