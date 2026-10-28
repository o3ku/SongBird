#!/usr/bin/env python3
"""Port of scripts/check-q-os-include-scope.ps1.

Q_OS_* / Q_PROCESSOR_* come from Qt's headers, not from the compiler, so a file that tests
one without reaching any Qt header compiles cleanly and takes the #else path in silence.
That is how CoreAssetPlatform.cpp reported Os::Other everywhere, which made a macOS build
download the Windows core.

The walk is an include-closure BFS per user file, with the resolution rules the compiler
uses: a quoted include first tries the including file's own directory, then the include
roots, then a bare file name anywhere in the tree.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text, walk_files  # noqa: E402

MACRO = re.compile(r"\bQ_(?:OS|PROCESSOR)_[A-Z0-9_]+")
INCLUDE = re.compile(r'^\s*#\s*include\s*(?P<bracket>[<"])(?P<target>[^>"]+)[>"]')
QT_HEADER = re.compile(r"^(?:Qt[A-Z]|Q[A-Z])")


def looks_like_qt_header(target: str) -> bool:
    # A Qt header is recognised by its name, not by being missing from the project:
    # "<QtCore/qstring.h>" and "<QString>" are the same header spelled two ways.
    return any(QT_HEADER.match(segment) for segment in target.replace("\\", "/").split("/"))


def main(argv) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", default="src")
    parser.add_argument("--test-root", default="tests")
    args = parser.parse_args(argv[1:])

    source_root = Path(args.source_root)
    test_root = Path(args.test_root)
    for path, label in ((source_root, "Source"), (test_root, "Test")):
        if not path.exists():
            fail(
                f"{label} root '{path}' does not exist, so the scan would read nothing. "
                "Refusing to report a pass."
            )
    roots = [source_root.resolve(), test_root.resolve()]

    header_index: dict[str, list[Path]] = {}
    for root in roots:
        for header in walk_files(root, (".h", ".hpp")):
            header_index.setdefault(header.name, []).append(header)
    if not header_index:
        fail(
            f"No headers found under '{source_root}' or '{test_root}'; the include resolution "
            "would be vacuous. Refusing to report a pass."
        )

    include_targets_cache: dict[tuple[str, str], list[Path]] = {}
    file_lines_cache: dict[Path, list[str]] = {}

    def file_lines(path: Path) -> list[str]:
        if path not in file_lines_cache:
            file_lines_cache[path] = read_text(path).splitlines()
        return file_lines_cache[path]

    def include_targets(target: str, including_file: Path) -> list[Path]:
        key = (str(including_file), target)
        if key in include_targets_cache:
            return include_targets_cache[key]

        resolved: list[Path] = []
        normalized = target.replace("\\", "/")

        candidate = including_file.parent / normalized
        if candidate.is_file():
            resolved.append(candidate.resolve())
        for root in roots:
            candidate = root / normalized
            if candidate.is_file():
                resolved.append(candidate.resolve())
        # Every candidate for a bare file name is followed, not just the first, so a
        # duplicated header name cannot hide the Qt include behind one of the copies.
        for path in header_index.get(Path(normalized).name, []):
            resolved.append(path.resolve())

        unique = list(dict.fromkeys(resolved))
        include_targets_cache[key] = unique
        return unique

    def qt_headers_reached(entry: Path) -> tuple[list[str], int]:
        visited: set[Path] = set()
        hits: list[str] = []
        pending = [entry]
        while pending:
            current = pending.pop()
            if current in visited:
                continue
            visited.add(current)
            for line in file_lines(current):
                match = INCLUDE.match(line)
                if not match:
                    continue
                target = match.group("target")
                resolved = include_targets(target, current)
                if resolved:
                    pending.extend(resolved)
                    continue
                if looks_like_qt_header(target):
                    hits.append(f"{current} -> <{target}>")
        return hits, len(visited)

    users = [
        path
        for root in roots
        for path in walk_files(root, (".h", ".hpp", ".cpp", ".cc", ".cxx"))
        if MACRO.search(read_text(path))
    ]
    if not users:
        fail(
            f"No file under '{source_root}' or '{test_root}' mentions Q_OS_* or "
            "Q_PROCESSOR_*; the scan is stale or the roots are wrong. Refusing to report a pass."
        )

    violations: list[str] = []
    reported: list[str] = []
    for user in users:
        relative = user.relative_to(source_root.resolve().parent).as_posix()
        hits, closure = qt_headers_reached(user)
        if not hits:
            violations.append(
                f"{relative} tests Q_OS_*/Q_PROCESSOR_* but reaches no Qt header in its include "
                f"closure ({closure} file(s) walked), so every branch it guards is compiled out. "
                "Include <QtGlobal> (or any Qt header) from the file or from a header it includes."
            )
        else:
            reported.append(f"{relative} (closure {closure}, first Qt header via {hits[0]})")

    if violations:
        fail_with_violations(violations)

    print(
        f"Q_OS_* include-scope check passed: {len(users)} file(s) mention Q_OS_*/Q_PROCESSOR_* "
        f"and every one of them reaches a Qt header; {len(header_index)} header name(s) indexed."
    )
    for line in reported:
        print(f"  ok  {line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
