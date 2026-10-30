#!/usr/bin/env python3
"""Verify the Hake Geospatial code revision embedded in a packaged/installed build.

Checks, against the commit CI built (GITHUB_SHA):
  - `hake-geodesk-process --version` reports
    "Hake Geospatial code revision <first 8 chars>"
  - a shipped binary under the package root contains the About link
    "<repository>/commit/<full sha>", and no binary links to another commit

Exits non-zero on any mismatch.
"""

from __future__ import annotations

import argparse
import mmap
import os
import re
import subprocess
import sys
from pathlib import Path

DEFAULT_REPOSITORY_URL = "https://github.com/sagar-singha-roy/hake-gis"
SHA_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
BINARY_MAGICS = (
    b"MZ",  # PE
    b"\x7fELF",
    b"\xfe\xed\xfa\xce",
    b"\xce\xfa\xed\xfe",
    b"\xfe\xed\xfa\xcf",
    b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe",  # Mach-O universal
)


def is_binary(path: Path) -> bool:
    try:
        with path.open("rb") as f:
            head = f.read(4)
    except OSError:
        return False
    return any(head.startswith(m) for m in BINARY_MAGICS)


def scan_binaries(root: Path, repository_url: str) -> dict[str, list[Path]]:
    pattern = re.compile(re.escape(repository_url.encode("ascii")) + rb"/commit/([0-9a-f]{40,64})")
    found: dict[str, list[Path]] = {}
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            path = Path(dirpath) / name
            if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
                continue
            if not is_binary(path):
                continue
            with path.open("rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as data:
                for match in pattern.finditer(data):
                    found.setdefault(match.group(1).decode("ascii"), []).append(path)
    return found


def process_revision(process_exe: Path) -> str:
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    result = subprocess.run(
        [str(process_exe.resolve()), "--version"],
        capture_output=True,
        text=True,
        env=env,
        timeout=300,
        check=False,
    )
    output = result.stdout + result.stderr
    print(output)
    if result.returncode != 0:
        raise SystemExit(f"{process_exe} --version exited with {result.returncode}")
    match = re.search(r"^Hake Geospatial code revision (\S+)\s*$", output, re.MULTILINE)
    if not match:
        raise SystemExit("No 'Hake Geospatial code revision' line in --version output")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--expected", required=True, help="full commit SHA the build was made from (GITHUB_SHA)")
    parser.add_argument("--package-root", required=True, type=Path, help="installed or extracted package tree")
    parser.add_argument("--process-exe", required=True, type=Path, help="installed hake-geodesk-process executable")
    parser.add_argument("--repository-url", default=DEFAULT_REPOSITORY_URL)
    args = parser.parse_args()

    expected = args.expected.strip().lower()
    if not SHA_RE.match(expected):
        raise SystemExit(f"--expected is not a full commit SHA: {args.expected!r}")
    expected_short = expected[:8]
    repository_url = args.repository_url.rstrip("/")

    if not args.package_root.is_dir():
        raise SystemExit(f"Package root not found: {args.package_root}")
    if not args.process_exe.is_file():
        raise SystemExit(f"Process executable not found: {args.process_exe}")

    print(f"Expected code revision: {expected_short} ({expected})")

    reported = process_revision(args.process_exe)
    if reported != expected_short:
        raise SystemExit(f"--version reports code revision {reported!r}, expected {expected_short!r}")
    print(f"OK: --version reports code revision {reported}")

    found = scan_binaries(args.package_root, repository_url)
    for sha, paths in sorted(found.items()):
        for path in paths:
            print(f"  {repository_url}/commit/{sha} in {path}")
    if expected not in found:
        raise SystemExit(f"No packaged binary contains {repository_url}/commit/{expected}")
    stale = sorted(sha for sha in found if sha != expected)
    if stale:
        raise SystemExit(f"Packaged binaries link to other commits: {', '.join(stale)}")
    print(f"OK: packaged About link is {repository_url}/commit/{expected}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
