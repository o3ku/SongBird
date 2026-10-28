#!/usr/bin/env python3
"""Port of scripts/check-hardcoded-colors.ps1 -- colours live in a palette file, nowhere else."""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import (  # noqa: E402
    blank_cpp_comments,
    fail,
    fail_with_violations,
    line_of,
    read_text,
    walk_files,
)

# The only files allowed to name a colour. A list rather than a naming rule: "*Palette*" would
# let a new file opt out by being named helpfully.
ALLOWED_FILES = {
    "src/ui/theme/AppThemePalette.cpp",
    "src/auto/AutoWindowPalette.cpp",
}

STRING_LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
HEX_COLOR = re.compile(r"#[0-9a-fA-F]{6}\b")


def main(argv) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path("src")
    if not source_root.exists():
        fail(f"Source root '{source_root}' does not exist; refusing to report a pass.")
    root = source_root.resolve()

    files = walk_files(root, (".cpp", ".h"))
    if not files:
        fail(f"No C++ sources under '{source_root}'; refusing to report a pass on an empty scan.")

    violations: list[str] = []
    for path in files:
        relative = f"src/{path.relative_to(root).as_posix()}"
        if relative in ALLOWED_FILES:
            continue

        text = blank_cpp_comments(read_text(path))
        # Only a hex inside a string literal counts. Scanning the whole file would flag a
        # variable named hexColor and would miss the real question, which is what the string
        # ends up being drawn with.
        for literal in STRING_LITERAL.finditer(text):
            for hex_match in HEX_COLOR.finditer(literal.group(1)):
                line = line_of(text, literal.start())
                violations.append(f"{relative}:{line}  {hex_match.group(0)}")

    if violations:
        fail_with_violations(
            [f"hardcoded colour: {v}" for v in violations],
        )

    print(
        f"Colour check passed: {len(files)} C++ file(s) scanned, no hex colour literal outside "
        f"{len(ALLOWED_FILES)} palette file(s)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
