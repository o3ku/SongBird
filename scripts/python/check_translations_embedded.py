#!/usr/bin/env python3
"""Port of scripts/check-translations-embedded.ps1.

Looks at what the build produced, not at what was configured. The embedding step used to
live inside `if(SONGBIRD_HAS_LINGUISTTOOLS)`, and the Qt that CI builds against has no
LinguistTools, so every released binary was English-only while the build stayed green.

The QM header test is the magic followed by 0xa7000000. The bare 16-byte magic is NOT enough:
a static Qt links QTranslator's own `static const uchar magic[16]` comparison constant into
the binary, which matches the magic on its own -- observed on the released v2.4.3 executable,
which had no .qm at all and still matched.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text  # noqa: E402

QM_HEADER = bytes(
    [
        0x3C, 0xB8, 0x64, 0x18, 0xCA, 0xEF, 0x9C, 0x95,
        0xCD, 0x21, 0x1C, 0xBF, 0x60, 0xA1, 0xBD, 0xDD, 0xA7, 0x00, 0x00, 0x00,
    ]
)
REQUIRED_QM_NAMES = ("SongBird_zh_CN.qm", "qt_zh_CN.qm")
QRC_ENTRY = re.compile(r'<file\s+alias="(?P<alias>[^"]+)"\s*>(?P<path>[^<]+)</file>')


def count_occurrences(haystack: bytes, needle: bytes) -> int:
    if not needle:
        return 0
    count = 0
    index = haystack.find(needle)
    while index >= 0:
        count += 1
        index = haystack.find(needle, index + 1)
    return count


def is_upx_packed(data: bytes) -> bool:
    return b"UPX!" in data and b"UPX0" in data


def main(argv) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-directory", required=True)
    parser.add_argument("--executable", default="")
    args = parser.parse_args(argv[1:])

    build_directory = Path(args.build_directory)
    executable = Path(args.executable) if args.executable else None
    violations: list[str] = []
    notes: list[str] = []

    qrc_path = build_directory / "translations.qrc"
    if not qrc_path.is_file():
        fail(
            f"{qrc_path} does not exist, so no translation is embedded in this build. "
            "src/CMakeLists.txt generates it whenever it finds either Qt LinguistTools or a "
            "compiled .qm under translations/compiled/."
        )

    qrc_text = read_text(qrc_path)
    entries = list(QRC_ENTRY.finditer(qrc_text))
    if not entries:
        fail(f"{qrc_path} lists no translation files; refusing to report a pass.")

    aliases = {entry.group("alias"): entry.group("path").strip() for entry in entries}

    validated = 0
    for name in REQUIRED_QM_NAMES:
        if name not in aliases:
            violations.append(
                f"{qrc_path} does not embed {name} (found: {', '.join(sorted(aliases))})."
            )
            continue
        qm_path = Path(aliases[name])
        if not qm_path.is_file():
            violations.append(f"{qrc_path} points {name} at {qm_path}, which does not exist.")
            continue
        data = qm_path.read_bytes()
        if len(data) < len(QM_HEADER):
            violations.append(
                f"{qm_path} is too small ({len(data)} bytes) to be a compiled translation."
            )
            continue
        if count_occurrences(data, QM_HEADER) != 1:
            violations.append(
                f"{qm_path} does not start with a Qt .qm header, so Qt would refuse to load it."
            )
            continue
        validated += 1

    if executable is not None:
        if not executable.is_file():
            violations.append(f"Executable not found: {executable}")
        else:
            inspect_path = executable
            unpacked_path = None
            if is_upx_packed(executable.read_bytes()):
                upx = shutil.which("upx")
                if not upx:
                    notes.append(
                        f"NOTE: {executable} is UPX-packed and upx was not found, so the linked"
                    )
                    notes.append("      binary was NOT inspected. The .qrc and .qm checks still ran.")
                else:
                    with tempfile.NamedTemporaryFile(suffix=".exe", delete=False) as handle:
                        unpacked_path = Path(handle.name)
                    result = subprocess.run(
                        [upx, "-d", "-o", str(unpacked_path), str(executable)],
                        capture_output=True,
                        check=False,
                    )
                    if result.returncode == 0 and unpacked_path.is_file():
                        inspect_path = unpacked_path
                        notes.append("Unpacked the UPX-compressed executable for inspection.")
                    else:
                        violations.append(
                            f"{executable} is UPX-packed and could not be unpacked (upx exit code "
                            f"{result.returncode}), so its embedded translations could not be verified."
                        )
                        inspect_path = None

            try:
                if inspect_path is not None:
                    data = inspect_path.read_bytes()
                    embedded = count_occurrences(data, QM_HEADER)
                    if embedded < len(REQUIRED_QM_NAMES):
                        violations.append(
                            f"{executable} contains {embedded} embedded .qm file(s) but "
                            f"{len(REQUIRED_QM_NAMES)} are required; the UI will not be localized."
                        )
                    # A concrete string from Qt's own translation, so the check also proves the
                    # embedded data is readable text rather than a header with nothing behind it.
                    ok_button = "确定".encode("utf-16-be")
                    if count_occurrences(data, ok_button) == 0:
                        violations.append(
                            f"{executable} does not contain the Chinese label for OK, so the "
                            "embedded translation is empty or corrupt."
                        )
                    notes.append(
                        f"Executable: {len(data)} bytes scanned, {embedded} embedded .qm header(s) found."
                    )
            finally:
                if unpacked_path is not None:
                    unpacked_path.unlink(missing_ok=True)
    else:
        notes.append(
            "NOTE: no --executable was given, so the linked binary was not inspected. Only the"
        )
        notes.append("      generated .qrc and the .qm files it references were validated.")

    for note in notes:
        print(note)

    if violations:
        for violation in violations:
            print(f"  - {violation}")
        fail_with_violations(
            [], f"Translation embedding is incomplete ({len(violations)} problem(s))."
        )

    print(
        f"Checked translation embedding: {qrc_path} lists {len(entries)} file(s); validated "
        f"{validated} required .qm file(s)."
    )
    print("Translation embedding check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
