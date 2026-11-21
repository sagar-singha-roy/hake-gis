#!/usr/bin/env python3
"""
Hake GeoDesk Plugin Mirror
Syncs plugins from the upstream QGIS plugin repository and rewrites
download URLs to point to the local Nginx-served mirror.

Architecture:
  Upstream (do not change query format):
    https://plugins.qgis.org/plugins/plugins.xml?qgis=4.0
  Application-facing repository (HTTP only until TLS is configured):
    http://plugins.haketech.com/plugins.xml?hake-geodesk=2026
    http://localhost/plugins.xml?hake-geodesk=2026
"""

from __future__ import annotations

import logging
import os
import shutil
import sys
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET

# NOTE: Upstream uses ?qgis= because that is what plugins.qgis.org expects.
# Querying QGIS 3.x series (?qgis=3.38) retrieves all 1,400+ ecosystem plugins.
# The local/application selector is ?hake-geodesk= (see installer_data.py urlParams()).
_DEFAULT_UPSTREAM = "https://plugins.qgis.org/plugins/plugins.xml?qgis=3.38"
UPSTREAM_URL = os.environ.get("UPSTREAM_URL", _DEFAULT_UPSTREAM).strip() or _DEFAULT_UPSTREAM

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGINS_DIR = os.path.join(BASE_DIR, "plugins")
XML_PATH = os.path.join(PLUGINS_DIR, "plugins.xml")
XML_TMP_PATH = os.path.join(PLUGINS_DIR, "plugins.xml.tmp")
XSL_SRC = os.path.join(BASE_DIR, "plugins.xsl")
XSL_DEST = os.path.join(PLUGINS_DIR, "plugins.xsl")
LOGO_SRC = os.path.join(BASE_DIR, "logo.png")
LOGO_DEST = os.path.join(PLUGINS_DIR, "logo.png")
LOG_DIR = os.path.join(BASE_DIR, "logs")
LOG_FILE = os.path.join(LOG_DIR, "sync.log")

USER_AGENT = "HakeGeoDesk-PluginSync/2026"
LOCAL_BASE_URL = os.environ.get(
    "PLUGIN_SERVER_URL", "https://plugins.haketech.com"
).rstrip("/")


def _setup_logging() -> logging.Logger:
    os.makedirs(LOG_DIR, exist_ok=True)
    logger = logging.getLogger("hake-geodesk-plugin-sync")
    logger.setLevel(logging.INFO)
    logger.handlers.clear()

    fmt = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
    sh = logging.StreamHandler(sys.stdout)
    sh.setFormatter(fmt)
    logger.addHandler(sh)

    fh = logging.FileHandler(LOG_FILE, encoding="utf-8")
    fh.setFormatter(fmt)
    logger.addHandler(fh)
    return logger


log = _setup_logging()


def fetch_upstream_xml() -> bytes:
    """Download the upstream plugins.xml."""
    log.info("Fetching upstream plugins.xml from %s", UPSTREAM_URL)
    req = urllib.request.Request(UPSTREAM_URL, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=60) as resp:
        data = resp.read()
    if not data:
        raise RuntimeError("Upstream plugins.xml is empty")
    return data


def download_zip(url: str, dest_path: str) -> bool:
    """Download a plugin zip atomically if it does not already exist."""
    if os.path.exists(dest_path):
        # mkstemp historically left mode 0600; ensure Nginx (www-data) can read.
        try:
            os.chmod(dest_path, 0o644)
        except OSError:
            pass
        log.info("  [skip] already exists: %s", os.path.basename(dest_path))
        return True

    partial_path = dest_path + ".partial"
    log.info("  [download] %s", os.path.basename(dest_path))
    try:
        req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        with urllib.request.urlopen(req, timeout=120) as resp:
            data = resp.read()
        if not data:
            raise RuntimeError("empty response")

        # Write to a unique temp then rename into place.
        # mkstemp creates 0600 — chmod after replace so www-data can serve the file.
        fd, tmp_name = tempfile.mkstemp(
            prefix=os.path.basename(dest_path) + ".",
            suffix=".partial",
            dir=PLUGINS_DIR,
        )
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(data)
            os.replace(tmp_name, dest_path)
            os.chmod(dest_path, 0o644)
        except Exception:
            if os.path.exists(tmp_name):
                os.remove(tmp_name)
            raise
        finally:
            if os.path.exists(partial_path):
                try:
                    os.remove(partial_path)
                except OSError:
                    pass
        return True
    except Exception as e:
        log.error("  [error] Failed to download %s: %s", url, e)
        for stale in (partial_path,):
            if os.path.exists(stale):
                try:
                    os.remove(stale)
                except OSError:
                    pass
        return False


def ensure_world_readable(path: str) -> None:
    """Make a file readable by Nginx (www-data) without requiring ownership match."""
    try:
        os.chmod(path, 0o644)
    except OSError as e:
        log.warning("Could not chmod 0644 %s: %s", path, e)


def normalize_mirror_permissions() -> None:
    """Ensure existing ZIPs and metadata are readable by the Nginx worker."""
    if not os.path.isdir(PLUGINS_DIR):
        return
    for name in os.listdir(PLUGINS_DIR):
        path = os.path.join(PLUGINS_DIR, name)
        if not os.path.isfile(path):
            continue
        if name.endswith(".partial") or name.endswith(".tmp"):
            continue
        ensure_world_readable(path)


def serialize_plugins_xml(root: ET.Element) -> str:
    """Serialize plugins XML with stylesheet instruction."""
    tree = ET.ElementTree(root)
    ET.indent(tree, space="    ")
    xml_str = ET.tostring(root, encoding="unicode", xml_declaration=True)
    xml_lines = xml_str.split("\n")
    if xml_lines:
        xml_lines.insert(
            1, '<?xml-stylesheet type="text/xsl" href="plugins.xsl" ?>'
        )
    return "\n".join(xml_lines)


def validate_plugins_xml(xml_text: str) -> ET.Element:
    """Parse and validate generated plugins.xml; raise on failure."""
    root = ET.fromstring(xml_text)
    tag = root.tag.split("}")[-1] if "}" in root.tag else root.tag
    if tag != "plugins":
        raise RuntimeError(f"Unexpected root element: {root.tag}")
    return root


def atomic_write_xml(xml_text: str) -> None:
    """Validate then atomically replace live plugins.xml."""
    validate_plugins_xml(xml_text)

    with open(XML_TMP_PATH, "w", encoding="UTF-8") as f:
        f.write(xml_text)
        f.flush()
        os.fsync(f.fileno())

    # Re-parse from disk before promoting.
    with open(XML_TMP_PATH, encoding="UTF-8") as f:
        validate_plugins_xml(f.read())

    os.replace(XML_TMP_PATH, XML_PATH)
    ensure_world_readable(XML_PATH)
    log.info("XML atomically replaced: %s", XML_PATH)


def sync() -> int:
    """Main sync routine. Returns process exit code."""
    started = time.time()
    log.info("=== sync start ===")
    log.info("PLUGIN_SERVER_URL=%s", LOCAL_BASE_URL)
    log.info("UPSTREAM_URL=%s", UPSTREAM_URL)

    os.makedirs(PLUGINS_DIR, exist_ok=True)
    had_prior_xml = os.path.isfile(XML_PATH)

    try:
        raw_xml = fetch_upstream_xml()
        root = ET.fromstring(raw_xml)
    except Exception as e:
        log.error("Upstream fetch/parse failed: %s", e)
        if had_prior_xml:
            log.error("Retaining previous valid plugins.xml")
        log.info("=== sync failed (%.1fs) ===", time.time() - started)
        return 1

    total = 0
    downloaded = 0
    skipped = 0
    failed = 0

    try:
        for plugin_elem in root.findall("pyqgis_plugin"):
            total += 1
            name = plugin_elem.get("name", "unknown")

            dl_elem = plugin_elem.find("download_url")
            if dl_elem is None or not dl_elem.text:
                log.warning("  [warn] No download_url for: %s", name)
                continue
            upstream_download_url = dl_elem.text.strip()

            fn_elem = plugin_elem.find("file_name")
            if fn_elem is None or not fn_elem.text:
                filename = upstream_download_url.rstrip("/").split("/")[-1]
                if not filename.endswith(".zip"):
                    filename = filename + ".zip"
            else:
                filename = fn_elem.text.strip()

            dest_path = os.path.join(PLUGINS_DIR, filename)
            existed = os.path.exists(dest_path)
            if download_zip(upstream_download_url, dest_path):
                if existed:
                    skipped += 1
                else:
                    downloaded += 1
            else:
                failed += 1
    except KeyboardInterrupt:
        log.warning(
            "Interrupted by user (Ctrl+C); leaving live plugins.xml intact."
        )
        log.info(
            "Partial progress: downloaded=%s skipped=%s failed=%s of %s",
            downloaded,
            skipped,
            failed,
            total,
        )
        log.info("=== sync interrupted (%.1fs) ===", time.time() - started)
        return 130

    # Rewrite ALL download URLs to the local mirror base.
    for plugin_elem in root.findall("pyqgis_plugin"):
        dl_elem = plugin_elem.find("download_url")
        fn_elem = plugin_elem.find("file_name")
        if dl_elem is None or fn_elem is None:
            continue
        filename = fn_elem.text.strip() if fn_elem.text else ""
        if not filename:
            continue
        dl_elem.text = LOCAL_BASE_URL + "/plugins/" + filename

    try:
        xml_text = serialize_plugins_xml(root)
        atomic_write_xml(xml_text)
    except Exception as e:
        log.error("XML generation/validation failed: %s", e)
        if os.path.exists(XML_TMP_PATH):
            try:
                os.remove(XML_TMP_PATH)
            except OSError:
                pass
        if had_prior_xml:
            log.error("Retaining previous valid plugins.xml")
        log.info("=== sync failed (%.1fs) ===", time.time() - started)
        return 1

    if os.path.exists(XSL_SRC):
        shutil.copy2(XSL_SRC, XSL_DEST)
        ensure_world_readable(XSL_DEST)
    if os.path.exists(LOGO_SRC):
        shutil.copy2(LOGO_SRC, LOGO_DEST)
        ensure_world_readable(LOGO_DEST)

    normalize_mirror_permissions()

    duration = time.time() - started
    log.info("Done.")
    log.info("  Total plugins: %s", total)
    log.info("  Newly downloaded: %s", downloaded)
    log.info("  Skipped (cached): %s", skipped)
    log.info("  Failed downloads: %s", failed)
    log.info("=== sync success (%.1fs) ===", duration)
    return 0


if __name__ == "__main__":
    sys.exit(sync())
