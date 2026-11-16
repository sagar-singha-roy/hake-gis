#!/usr/bin/env bash
# ==============================================================================
# Hake GeoDesk (Desktop GIS) - Self-Contained Linux AppImage & Portable Packager
# ==============================================================================
# Bundles:
#   - Hake GeoDesk binaries and core libraries
#   - Qt6 shared libraries and plugins (platforms, imageformats, sqldrivers, etc.)
#   - Full Python runtime and standard library
#   - PyQt6 modules (QtCore, QtGui, QtWidgets, QtMultimedia, QtPositioning, etc.)
#   - SIP runtime and compiled C bindings
#   - Geospatial C libraries (GDAL, PROJ, GEOS, SpatiaLite, QCA, QScintilla, etc.)
#   - PROJ data (proj.db) and GDAL data
#   - Desktop entry, high-res icons, and AppRun launcher
# Produces:
#   - Hake-GeoDesk-x86_64.AppImage
#   - Hake-GeoDesk-Linux-x86_64.tar.gz
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build"
OUTPUT_DIR="${REPO_ROOT}/dist"
ARCH="x86_64"
SKIP_APPIMAGE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --skip-appimage)
            SKIP_APPIMAGE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--build-dir DIR] [--output-dir DIR] [--arch ARCH] [--skip-appimage]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

# Ensure BUILD_DIR and OUTPUT_DIR are absolute paths to prevent tar -C or subshell cd breakage
mkdir -p "${BUILD_DIR}"
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

APPDIR="${BUILD_DIR}/Hake-GeoDesk.AppDir"

echo "======================================================================"
echo " Packaging Hake GeoDesk Self-Contained Linux Distribution"
echo " Build directory:  ${BUILD_DIR}"
echo " Output directory: ${OUTPUT_DIR}"
echo " Staging AppDir:   ${APPDIR}"
echo " Architecture:     ${ARCH}"
echo "======================================================================"

mkdir -p "${OUTPUT_DIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}"

# ------------------------------------------------------------------------------
# 1. Install built binaries and libraries into AppDir staging
# ------------------------------------------------------------------------------
echo "==> [1/8] Installing build artifacts into AppDir..."
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}" --strip

# If files were installed to usr/local instead of usr, normalize them to usr
if [[ -d "${APPDIR}/usr/local" ]]; then
    echo "    Normalizing usr/local hierarchy into usr..."
    cp -rn "${APPDIR}/usr/local"/* "${APPDIR}/usr/" 2>/dev/null || true
    rm -rf "${APPDIR}/usr/local"
fi

# Ensure standard directory layout exists
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/plugins"
mkdir -p "${APPDIR}/usr/share/hake-gis"
mkdir -p "${APPDIR}/usr/share/proj"
mkdir -p "${APPDIR}/usr/share/gdal"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
mkdir -p "${APPDIR}/usr/etc/ssl/certs"

# ------------------------------------------------------------------------------
# 2. Bundle PROJ and GDAL support data (Critical for CRS and raster/vector)
# ------------------------------------------------------------------------------
echo "==> [2/8] Bundling PROJ (proj.db) and GDAL data..."

PROJ_DATA_SRC=""
for p in /usr/share/proj /usr/local/share/proj /opt/share/proj; do
    if [[ -f "${p}/proj.db" ]]; then
        PROJ_DATA_SRC="${p}"
        break
    fi
done

if [[ -n "${PROJ_DATA_SRC}" ]]; then
    echo "    Found PROJ data at: ${PROJ_DATA_SRC}"
    cp -r "${PROJ_DATA_SRC}"/* "${APPDIR}/usr/share/proj/"
else
    echo "WARNING: proj.db not found on host system!" >&2
fi

GDAL_DATA_SRC=""
for g in /usr/share/gdal /usr/share/gdal/* /usr/local/share/gdal; do
    if [[ -d "${g}" && -f "${g}/gcs.csv" || -f "${g}/s57attributes.csv" ]]; then
        GDAL_DATA_SRC="${g}"
        break
    fi
done

if [[ -n "${GDAL_DATA_SRC}" ]]; then
    echo "    Found GDAL data at: ${GDAL_DATA_SRC}"
    cp -r "${GDAL_DATA_SRC}"/* "${APPDIR}/usr/share/gdal/"
fi

# Bundle system SSL certificates
for cert in /etc/ssl/certs/ca-certificates.crt /etc/pki/tls/certs/ca-bundle.crt; do
    if [[ -f "${cert}" ]]; then
        cp "${cert}" "${APPDIR}/usr/etc/ssl/certs/ca-certificates.crt"
        break
    fi
done

# ------------------------------------------------------------------------------
# 3. Bundle Qt6 Plugins (Platforms, ImageFormats, SQL, Multimedia, Wayland)
# ------------------------------------------------------------------------------
echo "==> [3/8] Bundling Qt6 plugins..."

QT_PLUGIN_SRC=""
for qp in \
    /usr/lib/x86_64-linux-gnu/qt6/plugins \
    /usr/lib64/qt6/plugins \
    /usr/lib/qt6/plugins \
    /opt/qt6/plugins; do
    if [[ -d "${qp}/platforms" ]]; then
        QT_PLUGIN_SRC="${qp}"
        break
    fi
done

if [[ -z "${QT_PLUGIN_SRC}" ]] && command -v qmake6 &>/dev/null; then
    QT_PLUGIN_SRC="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi

if [[ -d "${QT_PLUGIN_SRC}" ]]; then
    echo "    Found Qt6 plugins at: ${QT_PLUGIN_SRC}"
    # Copy essential plugin subdirectories (including Wayland, XCB, and Multimedia)
    for plug_dir in platforms imageformats xcbglintegrations wayland-graphics-integration-client wayland-shell-integration wayland-decoration-client sqldrivers multimedia tls; do
        if [[ -d "${QT_PLUGIN_SRC}/${plug_dir}" ]]; then
            mkdir -p "${APPDIR}/usr/plugins/${plug_dir}"
            cp -r "${QT_PLUGIN_SRC}/${plug_dir}"/* "${APPDIR}/usr/plugins/${plug_dir}/"
        fi
    done
    echo "    Verifying bundled Qt6 platform plugins:"
    for p in libqxcb.so libqwayland-generic.so libqwayland-egl.so; do
        if [[ -f "${APPDIR}/usr/plugins/platforms/${p}" ]]; then
            echo "      ✓ ${p} bundled"
        else
            echo "      ! ${p} not found in Qt plugin directory"
        fi
    done
else
    echo "WARNING: Could not locate Qt6 plugins directory!" >&2
fi

# ------------------------------------------------------------------------------
# 4. Bundle Python Runtime and PyQt6 Packages
# ------------------------------------------------------------------------------
echo "==> [4/8] Bundling Python runtime, PyQt6, and dependencies..."

PY_VER="$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
echo "    Host Python version: ${PY_VER}"

PY_STDLIB_DIR="/usr/lib/python${PY_VER}"
DEST_PY_DIR="${APPDIR}/usr/lib/python${PY_VER}"
mkdir -p "${DEST_PY_DIR}"

# Copy standard library (omit tests, cache, ensurepip to save space)
if [[ -d "${PY_STDLIB_DIR}" ]]; then
    echo "    Copying Python ${PY_VER} standard library..."
    rsync -a --exclude="__pycache__" --exclude="test" --exclude="tests" --exclude="idlelib" "${PY_STDLIB_DIR}/" "${DEST_PY_DIR}/"
fi

# Copy dynamic extensions (lib-dynload)
LIB_DYNLOAD="/usr/lib/python${PY_VER}/lib-dynload"
if [[ -d "${LIB_DYNLOAD}" ]]; then
    mkdir -p "${DEST_PY_DIR}/lib-dynload"
    cp -r "${LIB_DYNLOAD}"/* "${DEST_PY_DIR}/lib-dynload/"
fi

# Copy bundled python executable and libpython shared library
if command -v python3 &>/dev/null; then
    cp "$(command -v python3)" "${APPDIR}/usr/bin/python3"
    cp "$(command -v python3)" "${APPDIR}/usr/bin/python${PY_VER}"
fi

# Find and copy libpython shared library
LIBPYTHON_PATH="$(python3 -c 'import sysconfig, os; p = os.path.join(sysconfig.get_config_var("LIBDIR") or "/usr/lib", sysconfig.get_config_var("LDLIBRARY") or ""); print(p if os.path.exists(p) else "")')"
if [[ -n "${LIBPYTHON_PATH}" && -f "${LIBPYTHON_PATH}" ]]; then
    cp -P "${LIBPYTHON_PATH}"* "${APPDIR}/usr/lib/" 2>/dev/null || true
else
    for lp in /usr/lib/x86_64-linux-gnu/libpython${PY_VER}*.so* /usr/lib64/libpython${PY_VER}*.so* /usr/lib/libpython${PY_VER}*.so*; do
        if [[ -f "${lp}" ]]; then
            cp -P "${lp}" "${APPDIR}/usr/lib/" 2>/dev/null || true
        fi
    done
fi

# Copy PyQt6 and Python dependencies into dist-packages
DEST_DIST="${APPDIR}/usr/lib/python3/dist-packages"
mkdir -p "${DEST_DIST}"

PY_PACKAGES=(
    PyQt6
    PyQt6_sip
    sip
    requests
    urllib3
    certifi
    charset_normalizer
    idna
    yaml
    jinja2
    pyproj
    psycopg2
    packaging
)

SEARCH_PYTHON_PATHS=(
    "/usr/lib/python3/dist-packages"
    "/usr/local/lib/python${PY_VER}/dist-packages"
    "/usr/lib/python${PY_VER}/site-packages"
    "/usr/local/lib/python${PY_VER}/site-packages"
)

for pkg in "${PY_PACKAGES[@]}"; do
    FOUND=false
    for sdir in "${SEARCH_PYTHON_PATHS[@]}"; do
        if [[ -d "${sdir}/${pkg}" ]]; then
            echo "    Copying package ${pkg} from ${sdir}..."
            cp -r "${sdir}/${pkg}" "${DEST_DIST}/"
            FOUND=true
            break
        elif [[ -f "${sdir}/${pkg}.py" ]]; then
            echo "    Copying module ${pkg}.py from ${sdir}..."
            cp "${sdir}/${pkg}.py" "${DEST_DIST}/"
            FOUND=true
            break
        fi
    done
    # Also copy any .egg-info or .dist-info for metadata
    for sdir in "${SEARCH_PYTHON_PATHS[@]}"; do
        for info in "${sdir}/${pkg}"*.dist-info "${sdir}/${pkg}"*.egg-info; do
            if [[ -d "${info}" ]]; then
                cp -r "${info}" "${DEST_DIST}/" 2>/dev/null || true
            fi
        done
    done
done

# Verify critical PyQt6 multimedia and positioning modules exist
if [[ -d "${DEST_DIST}/PyQt6" ]]; then
    echo "    Verifying bundled PyQt6 binary submodules..."
    for mod in QtCore QtGui QtWidgets QtMultimedia QtPositioning QtSvg QtXml QtNetwork; do
        if ls "${DEST_DIST}/PyQt6/${mod}"*.so &>/dev/null; then
            echo "      ✓ PyQt6.${mod} bundled"
        else
            echo "      ! PyQt6.${mod} not found in host dist-packages (optional/fallback)"
        fi
    done
fi

# Ensure compiled QGIS python bindings are accessible
if [[ -d "${APPDIR}/usr/share/hake-gis/python" ]]; then
    echo "    Linking QGIS Python bindings to bundled Python path..."
    mkdir -p "${APPDIR}/usr/lib/python3/dist-packages"
    ln -sfn "../../share/hake-gis/python/qgis" "${APPDIR}/usr/lib/python3/dist-packages/qgis"
fi

# ------------------------------------------------------------------------------
# 5. Resolve and Bundle Native Shared Libraries
# ------------------------------------------------------------------------------
echo "==> [5/8] Resolving and bundling native shared libraries..."

# AppImage Excludelist: Libraries that MUST come from the host system
# (e.g. glibc, graphics driver stacks, core X11/wayland communication)
EXCLUDED_PATTERNS=(
    "^ld-linux"
    "^libc\.so"
    "^libm\.so"
    "^libpthread\.so"
    "^libdl\.so"
    "^librt\.so"
    "^libutil\.so"
    "^libresolv\.so"
    "^libnss_"
    "^libGL\.so"
    "^libGLX\.so"
    "^libGLdispatch\.so"
    "^libEGL\.so"
    "^libdrm\.so"
    "^libgbm\.so"
    "^libasound\.so"
    "^libudev\.so"
)

is_excluded() {
    local lib_basename
    lib_basename="$(basename "$1")"
    for pattern in "${EXCLUDED_PATTERNS[@]}"; do
        if [[ "${lib_basename}" =~ ${pattern} ]]; then
            return 0
        fi
    done
    return 1
}

# Collect all ELF binaries, libraries, plugins, and Python extensions to inspect
get_elf_files() {
    find "${APPDIR}/usr/bin" \
         "${APPDIR}/usr/lib" \
         "${APPDIR}/usr/plugins" \
         -type f \( -name "*.so*" -o -perm /111 \) 2>/dev/null || true
}

copy_dependencies() {
    local max_iterations=4
    local iter=0

    while [[ ${iter} -lt ${max_iterations} ]]; do
        ((iter++))
        local newly_copied=0

        while IFS= read -r elf; do
            [[ -f "${elf}" ]] || continue
            # Check if file is an ELF binary/library
            file "${elf}" | grep -q "ELF" || continue

            # Extract unresolved dependencies from ldd
            while IFS= read -r line; do
                if [[ "${line}" =~ =\>[[:space:]]*([^[:space:]]+)[[:space:]]+\(0x ]]; then
                    local dep_path="${BASH_REMATCH[1]}"
                    if [[ -f "${dep_path}" ]]; then
                        local dep_name
                        dep_name="$(basename "${dep_path}")"

                        if ! is_excluded "${dep_name}"; then
                            if [[ ! -f "${APPDIR}/usr/lib/${dep_name}" ]]; then
                                cp -P "${dep_path}" "${APPDIR}/usr/lib/" 2>/dev/null || true
                                newly_copied=$((newly_copied + 1))

                                # Also copy soname symlink targets if symlink
                                if [[ -L "${dep_path}" ]]; then
                                    local target_path
                                    target_path="$(readlink -f "${dep_path}")"
                                    if [[ -f "${target_path}" ]]; then
                                        cp -P "${target_path}" "${APPDIR}/usr/lib/" 2>/dev/null || true
                                    fi
                                fi
                            fi
                        fi
                    fi
                fi
            done < <(ldd "${elf}" 2>/dev/null || true)
        done < <(get_elf_files)

        echo "    Iteration ${iter}: copied ${newly_copied} dependency libraries."
        if [[ ${newly_copied} -eq 0 ]]; then
            break
        fi
    done
}

copy_dependencies

# Flatten or ensure arch-specific lib symlink exists
if [[ -d "${APPDIR}/usr/lib/x86_64-linux-gnu" ]]; then
    echo "    Integrating usr/lib/x86_64-linux-gnu into usr/lib..."
    cp -rn "${APPDIR}/usr/lib/x86_64-linux-gnu"/* "${APPDIR}/usr/lib/" 2>/dev/null || true
fi

# ------------------------------------------------------------------------------
# 6. Setup Desktop Integration and Icons
# ------------------------------------------------------------------------------
echo "==> [6/8] Setting up desktop integration..."

# Install desktop files (both org.hake.geospatial.desktop and hake-geodesk.desktop)
cat <<'EOF' > "${APPDIR}/org.hake.geospatial.desktop"
[Desktop Entry]
Type=Application
Name=Hake GeoDesk (Desktop GIS)
GenericName=Geographic Information System
Comment=Professional GIS for Mapping, Analysis & Spatial Intelligence
Exec=hake-geodesk %F
TryExec=hake-geodesk
Icon=hake-gis
Terminal=false
StartupNotify=false
Categories=Qt;Education;Science;Geography;
MimeType=application/x-qgis-project;application/x-qgis-project-container;application/x-qgis-layer-settings;application/x-qgis-layer-definition;application/x-qgis-composer-template;image/tiff;image/jpeg;image/jp2;application/x-raster-aig;application/x-raster-ecw;application/x-raster-mrsid;application/x-mapinfo-mif;application/x-esri-shape;application/vnd.google-earth.kml+xml;application/vnd.google-earth.kmz;application/geopackage+sqlite3;
Keywords=map;globe;postgis;wms;wfs;ogc;osgeo;
StartupWMClass=org.hake.geospatial
EOF

mkdir -p "${APPDIR}/usr/share/applications"
cp "${APPDIR}/org.hake.geospatial.desktop" "${APPDIR}/usr/share/applications/org.hake.geospatial.desktop"
cp "${APPDIR}/org.hake.geospatial.desktop" "${APPDIR}/usr/share/applications/hake-geodesk.desktop"
# AppImage specification requires EXACTLY ONE .desktop file in AppDir root.
# Remove duplicate desktop file in AppDir root so appimagetool does not fail.
rm -f "${APPDIR}/hake-geodesk.desktop"

# Validate desktop file compliance if desktop-file-validate is available
if command -v desktop-file-validate &>/dev/null; then
    desktop-file-validate "${APPDIR}/org.hake.geospatial.desktop" || true
fi

# Install application icons across standard XDG sizes and names
ICON_512="${REPO_ROOT}/images/icons/hake-gis-icon-512x512.png"
ICON_SVG="${REPO_ROOT}/images/svg/logos/hake-gis-icon.svg"

mkdir -p "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"

if [[ -f "${ICON_512}" ]]; then
    cp "${ICON_512}" "${APPDIR}/hake-gis.png"
    cp "${ICON_512}" "${APPDIR}/org.hake.geospatial.png"
    cp "${ICON_512}" "${APPDIR}/.DirIcon"
    cp "${ICON_512}" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/hake-gis.png"
    cp "${ICON_512}" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/org.hake.geospatial.png"
fi

if [[ -f "${ICON_SVG}" ]]; then
    cp "${ICON_SVG}" "${APPDIR}/hake-gis.svg"
    cp "${ICON_SVG}" "${APPDIR}/org.hake.geospatial.svg"
    cp "${ICON_SVG}" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/hake-gis.svg"
    cp "${ICON_SVG}" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/org.hake.geospatial.svg"
fi

# ------------------------------------------------------------------------------
# 7. Create AppRun Entrypoint Launcher
# ------------------------------------------------------------------------------
echo "==> [7/8] Generating AppRun launcher..."

cat <<'EOF' > "${APPDIR}/AppRun"
#!/usr/bin/env bash
# ==============================================================================
# Hake GeoDesk AppRun - Self-Contained Environment Launcher
# ==============================================================================
set -e

# Resolve absolute path to the directory containing this script
HERE="$(dirname "$(readlink -f "${0}")")"
export APPDIR="${HERE}"

# Binary search path
export PATH="${APPDIR}/usr/bin:${PATH}"

# Shared library resolution
export LD_LIBRARY_PATH="${APPDIR}/usr/lib:${APPDIR}/usr/lib/hake-gis:${APPDIR}/usr/lib/hake-gis/plugins:${APPDIR}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# QGIS application paths
export QGIS_PREFIX_PATH="${APPDIR}/usr"
export QGIS_PLUGINPATH="${APPDIR}/usr/lib/hake-gis/plugins"

# Geospatial data definitions
if [ -d "${APPDIR}/usr/share/proj" ]; then
    export PROJ_DATA="${APPDIR}/usr/share/proj"
    export PROJ_LIB="${APPDIR}/usr/share/proj"
fi

if [ -d "${APPDIR}/usr/share/gdal" ]; then
    export GDAL_DATA="${APPDIR}/usr/share/gdal"
fi

# Qt6 plugins and QML
export QT_PLUGIN_PATH="${APPDIR}/usr/plugins:${APPDIR}/usr/lib/x86_64-linux-gnu/qt6/plugins"
export QML2_IMPORT_PATH="${APPDIR}/usr/qml:${APPDIR}/usr/lib/x86_64-linux-gnu/qt6/qml"

# ==============================================================================
# Display Platform Backend Detection (Wayland / XWayland / X11)
# ==============================================================================
# Honor explicit user choice via QT_QPA_PLATFORM if already defined
if [ -z "${QT_QPA_PLATFORM}" ]; then
    if [ "${HAKE_USE_XWAYLAND:-0}" = "1" ] && [ -n "${DISPLAY}" ]; then
        # Explicit user preference for XWayland compatibility mode
        export QT_QPA_PLATFORM="xcb"
    elif [ -n "${WAYLAND_DISPLAY}" ]; then
        # Wayland desktop session detected (Ubuntu Wayland, GNOME, KDE Plasma, etc.)
        # Check if native Wayland platform plugin is bundled
        WAYLAND_PLUGIN="${APPDIR}/usr/plugins/platforms/libqwayland-generic.so"
        if [ ! -f "${WAYLAND_PLUGIN}" ]; then
            WAYLAND_PLUGIN="${APPDIR}/usr/plugins/platforms/libqwayland-egl.so"
        fi

        if [ -f "${WAYLAND_PLUGIN}" ]; then
            # Both native Wayland and XWayland (xcb) are available.
            # Configure Qt with "wayland;xcb" so it initializes native Wayland
            # while providing seamless, automatic fallback to XWayland if the
            # compositor socket connection fails.
            export QT_QPA_PLATFORM="wayland;xcb"
        elif [ -n "${DISPLAY}" ]; then
            # Wayland plugin not present but XWayland is available
            export QT_QPA_PLATFORM="xcb"
        else
            export QT_QPA_PLATFORM="wayland"
        fi
    elif [ -n "${DISPLAY}" ]; then
        # Traditional X11 session
        export QT_QPA_PLATFORM="xcb"
    else
        # Headless environment (CI, automated pipelines, Docker)
        export QT_QPA_PLATFORM="offscreen"
    fi
fi

# High-DPI and fractional scaling configuration for pristine rendering
export QT_ENABLE_HIGHDPI_SCALING=1
export QT_AUTO_SCREEN_SCALE_FACTOR=1
export QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough

# Prevent host Python user-site packages (~/.local) from conflicting with bundled libraries
export PYTHONNOUSERSITE=1
export PYTHONHOME="${APPDIR}/usr"

# Determine bundled python version directory
PYDIR="$(find "${APPDIR}/usr/lib" -maxdepth 1 -type d -name "python3.*" 2>/dev/null | head -n 1)"
if [ -n "${PYDIR}" ]; then
    export PYTHONPATH="${APPDIR}/usr/share/hake-gis/python:${PYDIR}:${PYDIR}/lib-dynload:${PYDIR}/dist-packages:${PYDIR}/site-packages:${APPDIR}/usr/lib/python3/dist-packages"
else
    export PYTHONPATH="${APPDIR}/usr/share/hake-gis/python:${APPDIR}/usr/lib/python3/dist-packages"
fi

# SSL Certificates for network requests / plugin repository
if [ -f "${APPDIR}/usr/etc/ssl/certs/ca-certificates.crt" ]; then
    export SSL_CERT_FILE="${APPDIR}/usr/etc/ssl/certs/ca-certificates.crt"
    export REQUESTS_CA_BUNDLE="${APPDIR}/usr/etc/ssl/certs/ca-certificates.crt"
fi

# Execute main Hake GeoDesk binary
exec "${APPDIR}/usr/bin/hake-geodesk" "$@"
EOF

chmod +x "${APPDIR}/AppRun"

# ------------------------------------------------------------------------------
# 8. Produce Final Distributables (AppImage & Portable Tarball)
# ------------------------------------------------------------------------------
echo "==> [8/8] Generating self-contained distribution archives..."

TARBALL_NAME="Hake-Geodesk-Linux-${ARCH}.tar.gz"
echo "    Creating portable self-contained archive: ${TARBALL_NAME}..."
tar -czf "${OUTPUT_DIR}/${TARBALL_NAME}" -C "${BUILD_DIR}" "Hake-GeoDesk.AppDir"

if [[ "${SKIP_APPIMAGE}" == "false" ]]; then
    APPIMAGETOOL_BIN="$(command -v appimagetool || true)"
    if [[ -z "${APPIMAGETOOL_BIN}" && -f "${BUILD_DIR}/appimagetool-x86_64.AppImage" ]]; then
        APPIMAGETOOL_BIN="${BUILD_DIR}/appimagetool-x86_64.AppImage"
    fi

    if [[ -z "${APPIMAGETOOL_BIN}" ]]; then
        echo "    Downloading appimagetool..."
        APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
        if curl -fsSL -o "${BUILD_DIR}/appimagetool-x86_64.AppImage" "${APPIMAGETOOL_URL}"; then
            chmod +x "${BUILD_DIR}/appimagetool-x86_64.AppImage"
            APPIMAGETOOL_BIN="${BUILD_DIR}/appimagetool-x86_64.AppImage"
        fi
    fi

    if [[ -n "${APPIMAGETOOL_BIN}" ]]; then
        APPIMAGE_NAME="Hake-GeoDesk-${ARCH}.AppImage"
        echo "    Building AppImage via ${APPIMAGETOOL_BIN}..."

        # Extract appimagetool if it is an AppImage to avoid requiring FUSE on modern runners
        export APPIMAGE_EXTRACT_AND_RUN=1
        APPIMAGETOOL_EXTRACTED="${BUILD_DIR}/appimagetool_extracted"
        if [[ "${APPIMAGETOOL_BIN}" == *.AppImage* && ! -x "${APPIMAGETOOL_EXTRACTED}/AppRun" ]]; then
            rm -rf "${APPIMAGETOOL_EXTRACTED}" "${BUILD_DIR}/squashfs-root"
            echo "    Extracting appimagetool to bypass FUSE requirements..."
            if command -v unsquashfs &>/dev/null; then
                unsquashfs -f -d "${APPIMAGETOOL_EXTRACTED}" "${APPIMAGETOOL_BIN}" || true
            fi
            if [[ ! -x "${APPIMAGETOOL_EXTRACTED}/AppRun" ]]; then
                (cd "${BUILD_DIR}" && "${APPIMAGETOOL_BIN}" --appimage-extract && mv "${BUILD_DIR}/squashfs-root" "${APPIMAGETOOL_EXTRACTED}") || true
            fi
        fi

        APPIMAGE_BUILT=false
        if [[ -x "${APPIMAGETOOL_EXTRACTED}/AppRun" ]]; then
            echo "    Executing extracted appimagetool (no FUSE required)..."
            if ARCH="${ARCH}" "${APPIMAGETOOL_EXTRACTED}/AppRun" --no-appstream "${APPDIR}" "${OUTPUT_DIR}/${APPIMAGE_NAME}"; then
                APPIMAGE_BUILT=true
            fi
        fi

        if [[ "${APPIMAGE_BUILT}" != "true" ]]; then
            echo "    Attempting fallback appimagetool execution..."
            if ARCH="${ARCH}" APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGETOOL_BIN}" --no-appstream "${APPDIR}" "${OUTPUT_DIR}/${APPIMAGE_NAME}"; then
                APPIMAGE_BUILT=true
            fi
        fi

        if [[ -f "${OUTPUT_DIR}/${APPIMAGE_NAME}" ]]; then
            chmod +x "${OUTPUT_DIR}/${APPIMAGE_NAME}"
            echo "    ✓ Successfully generated AppImage: ${OUTPUT_DIR}/${APPIMAGE_NAME}"
        else
            echo "WARNING: AppImage generation did not produce an image; self-contained portable tarball is available: ${OUTPUT_DIR}/${TARBALL_NAME}" >&2
        fi
    else
        echo "    Notice: appimagetool unavailable in this environment; portable bundle generated."
    fi
fi

echo "======================================================================"
echo " Hake GeoDesk Linux Packaging Complete!"
echo " Distributable files created in: ${OUTPUT_DIR}"
ls -lh "${OUTPUT_DIR}"
echo "======================================================================"
