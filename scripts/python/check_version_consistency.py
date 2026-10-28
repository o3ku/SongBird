#!/usr/bin/env python3
"""Port of scripts/check-version-consistency.ps1.

The version has exactly one declaration -- `project(SongBird VERSION x.y.z)` -- and several
consumers: the compile definition, the macOS bundle, the release tag. Nothing compared them,
so a hardcoded second literal, or a tag that disagreed with the declaration, published a
binary whose About dialog, update check and release page all claimed different numbers.

Usage:
    check_version_consistency.py [--tag v2.7.0] [--repo-root DIR]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text  # noqa: E402

PROJECT_VERSION = re.compile(r"project\s*\(\s*SongBird\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)")
VERSION_SUFFIX = re.compile(r'set\s*\(\s*SONGBIRD_VERSION_SUFFIX\s+"([^"]*)"\s*\)')
DISPLAY_VERSION = re.compile(r'set\s*\(\s*SONGBIRD_DISPLAY_VERSION\s+"([^"]*)"\s*\)')
SCAN_SUFFIXES = (".txt", ".cmake", ".cpp", ".h", ".ps1", ".json", ".py")


def scan_files(root: Path, path: Path) -> list[Path]:
    if path.is_file():
        return [path]
    found = []
    for candidate in sorted(path.rglob("*")):
        if candidate.is_file() and candidate.suffix in SCAN_SUFFIXES:
            found.append(candidate)
    return found


def main(argv) -> int:
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("--tag", default="")
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[2]))
    args = parser.parse_args(argv[1:])

    repo_root = Path(args.repo_root).resolve()
    cmake_lists = repo_root / "CMakeLists.txt"
    src_cmake_lists = repo_root / "src" / "CMakeLists.txt"
    if not cmake_lists.is_file():
        fail(f"CMakeLists.txt not found at '{cmake_lists}'; refusing to report a pass.")
    if not src_cmake_lists.is_file():
        fail(f"src/CMakeLists.txt not found at '{src_cmake_lists}'; refusing to report a pass.")

    root_text = read_text(cmake_lists)

    match = PROJECT_VERSION.search(root_text)
    if not match:
        fail(f"Could not read project(SongBird VERSION x.y.z) from '{cmake_lists}'.")
    declared = match.group(1)

    match = VERSION_SUFFIX.search(root_text)
    if not match:
        fail(f"Could not read SONGBIRD_VERSION_SUFFIX from '{cmake_lists}'.")
    suffix = match.group(1)

    match = DISPLAY_VERSION.search(root_text)
    if not match:
        fail(f"Could not read SONGBIRD_DISPLAY_VERSION from '{cmake_lists}'.")
    display_definition = match.group(1)
    expected_display = f"{declared}{suffix}"

    violations: list[str] = []

    # The composition itself must be the literal the file claims. If someone hardcodes the
    # display version, project(VERSION) stops being the source and the two can drift.
    if display_definition != "${PROJECT_VERSION}${SONGBIRD_VERSION_SUFFIX}":
        violations.append(
            f"SONGBIRD_DISPLAY_VERSION is '{display_definition}' instead of "
            "'${PROJECT_VERSION}${SONGBIRD_VERSION_SUFFIX}'; project(VERSION) is then no longer "
            "the single source and the two can drift apart."
        )

    # This script is excluded from its own scan: it names the version it reads in its
    # diagnostics, and a port of it does the same. tests/ is exempt because a version
    # comparison test needs concrete numbers as test data; CHANGELOG.md records past releases.
    self_paths = {Path(__file__).resolve(), (Path(__file__).resolve().parent / "check_version_consistency.ps1")}
    scan_roots = [cmake_lists, repo_root / "src", repo_root / "scripts"]
    literal_hits: list[str] = []
    for scan_root in scan_roots:
        if not scan_root.exists():
            continue
        for file in scan_files(repo_root, scan_root):
            if file.resolve() == cmake_lists.resolve() or file.resolve() in self_paths:
                continue
            if declared in read_text(file):
                literal_hits.append(file.relative_to(repo_root).as_posix())

    for hit in literal_hits:
        violations.append(
            f"Hardcoded version '{declared}' at {hit} -- the version must come from "
            "project(VERSION) in CMakeLists.txt, not from a second literal."
        )

    if "SONGBIRD_DISPLAY_VERSION" not in read_text(src_cmake_lists):
        violations.append(
            "src/CMakeLists.txt never references SONGBIRD_DISPLAY_VERSION; the executable's "
            "version would come from somewhere other than project(VERSION)."
        )

    # Every #ifndef SONGBIRD_APP_VERSION fallback must be "unknown". The literal scan above
    # compares files against project(VERSION), so a fallback spelling an *older* release could
    # never be caught by it -- and one did: both main.cpp files carried "2.5.0" while the
    # project had moved two versions on. A fallback that names a real release is worse than no
    # fallback, because a build that bypassed CMake then reports a plausible version.
    fallback_definition = re.compile(r'(?m)^\s*#\s*ifndef\s+SONGBIRD_APP_VERSION\b')
    fallback_value = re.compile(r'#\s*define\s+SONGBIRD_APP_VERSION\s+"([^"]*)"')
    fallback_hits: list[str] = []
    declared_fallbacks = 0
    main_files = sorted((repo_root / "src").rglob("*.cpp"))
    mentions_any = False
    for file in main_files:
        content = read_text(file)
        if "SONGBIRD_APP_VERSION" in content:
            mentions_any = True
        if not fallback_definition.search(content):
            continue
        declared_fallbacks += 1
        value_match = fallback_value.search(content)
        relative = file.relative_to(repo_root).as_posix()
        if not value_match:
            fallback_hits.append(
                f"{relative}: the SONGBIRD_APP_VERSION fallback defines no string"
            )
            continue
        if value_match.group(1) != "unknown":
            fallback_hits.append(
                f"{relative}: the SONGBIRD_APP_VERSION fallback is '{value_match.group(1)}'; it "
                "must be 'unknown' so a build that bypassed CMake cannot report a plausible version"
            )
    if not fallback_hits and main_files and not mentions_any:
        violations.append(
            "No file under src/ mentions SONGBIRD_APP_VERSION; the executables cannot be "
            "reporting the composed version at all."
        )
    violations.extend(fallback_hits)

    if args.tag.strip():
        tag_version = re.sub(r"^v", "", args.tag.strip())
        if tag_version != expected_display:
            violations.append(
                f"Tag '{args.tag}' implies version '{tag_version}', but CMakeLists.txt declares "
                f"'{expected_display}'. Bump project(VERSION)/SONGBIRD_VERSION_SUFFIX in the "
                "tagged commit."
            )

    if violations:
        fail_with_violations(violations)

    # Positive evidence the check actually read something, so a green run cannot be confused
    # with a run that scanned nothing.
    print(
        f"Version consistency: declared '{declared}', suffix '{suffix}', display "
        f"'{expected_display}'; scanned {len(scan_roots)} root(s), {len(literal_hits)} stray "
        f"literal(s), {declared_fallbacks} version fallback(s)"
        + (f", tag '{args.tag}' verified" if args.tag.strip() else "")
        + "."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
