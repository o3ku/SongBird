#!/usr/bin/env python3
"""Port of scripts/check-macos-frameworks.ps1.

The macOS secret store uses CoreFoundation types and calls (CFDictionaryCreateMutable,
CFDataCreate, CFRelease), which live in CoreFoundation, not Security. The macOS sources
compile with only "-framework Security" linked, so the missing framework is invisible
until the link step -- and on this Linux machine the APPLE block never runs at all, so
nothing here could catch it. The link then fails with undefined symbols (_CFDataCreate,
_CFDictionaryCreateMutable) on both architectures, which is exactly what happened after
PR #6: the GCM private-SPI fix compiled, and the link broke instead.

Usage:
    check_macos_frameworks.py [--repo-root DIR]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import blank_cpp_comments, fail, fail_with_violations, read_text  # noqa: E402

# Any CoreFoundation identifier: CF-prefixed calls and types, and the kCF-prefixed
# constants (kCFBooleanTrue, kCFTypeDictionaryKeyCallBacks). The Security constants the
# same files use (kSecClass, kSecAttrService) do not match either arm.
CF_SYMBOL = re.compile(r"\b(?:CF|kCF)[A-Z][A-Za-z0-9]*\b")
APPLE_BLOCK = re.compile(r"if\s*\(\s*APPLE\s*\)(.*?)endif\s*\(\s*\)", re.DOTALL)
CORE_FOUNDATION_LINK = re.compile(r"-framework\s+CoreFoundation\b")


def main(argv) -> int:
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[2]))
    args = parser.parse_args(argv[1:])

    repo_root = Path(args.repo_root).resolve()
    src_cmake_lists = repo_root / "src" / "CMakeLists.txt"
    macos_root = repo_root / "src" / "platform" / "macos"
    if not src_cmake_lists.is_file():
        fail(f"src/CMakeLists.txt not found at '{src_cmake_lists}'; refusing to report a pass.")
    if not macos_root.is_dir():
        fail(f"macOS platform sources not found at '{macos_root}'; refusing to report a pass.")

    cmake_text = read_text(src_cmake_lists)
    # There are several if(APPLE) blocks (source lists, the bundle, the frameworks), so
    # every one is scanned: the link counts wherever it sits, as long as it is Apple-only.
    apple_blocks = APPLE_BLOCK.findall(cmake_text)
    if not apple_blocks:
        fail("src/CMakeLists.txt has no if(APPLE) block; refusing to report a pass.")
    core_foundation_linked = any(
        CORE_FOUNDATION_LINK.search(block) for block in apple_blocks
    )

    # First use of each symbol, in file order, so the violation names the place a reader
    # can open. Comments are blanked first: a CF name in a comment is explanation, not a
    # link dependency, and counting it would train people to stop explaining the code.
    symbols: dict[str, str] = {}
    files_scanned = 0
    for source in sorted(macos_root.glob("*")):
        if source.suffix not in (".cpp", ".h") or not source.is_file():
            continue
        files_scanned += 1
        text = blank_cpp_comments(read_text(source))
        for match in CF_SYMBOL.finditer(text):
            symbol = match.group(0)
            if symbol in symbols:
                continue
            line = text.count("\n", 0, match.start()) + 1
            symbols[symbol] = f"{source.relative_to(repo_root).as_posix()}:{line}"

    if not symbols:
        fail(
            "No CoreFoundation symbol found under src/platform/macos/; the scan pattern "
            "may be stale. Either the macOS sources stopped using CoreFoundation (then "
            "remove this guard and the link) or the pattern no longer matches what they use."
        )

    if not core_foundation_linked:
        violations = [
            f"{location}: {symbol} is used here, but src/CMakeLists.txt does not link "
            '"-framework CoreFoundation" in any if(APPLE) block; the macOS link then fails '
            "with undefined symbols."
            for symbol, location in sorted(symbols.items(), key=lambda item: item[1])
        ]
        fail_with_violations(violations)

    # Positive evidence the check actually read something, so a green run cannot be confused
    # with a run that scanned nothing.
    print(
        f"macOS frameworks: {len(symbols)} CoreFoundation symbol(s) in {files_scanned} "
        'file(s) under src/platform/macos/, "-framework CoreFoundation" linked in '
        "src/CMakeLists.txt."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
