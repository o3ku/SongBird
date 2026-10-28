#!/usr/bin/env python3
"""Port of scripts/check-readme-assets.ps1 -- every image a README references must exist.

Nothing compiles a README, so a moved screenshot leaves a broken icon on the project's
front page with no build failure. Both READMEs are scanned: the translated page shares the
screenshots and would otherwise have no protection of its own.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text  # noqa: E402

# Markdown image syntax: ![alt](path). The alt text may be empty.
IMAGE = re.compile(r"!\[[^\]]*\]\(([^)\s]+)")
ABSOLUTE_URL = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*://")


def main(argv) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parents[2]
    if not root.is_dir():
        fail(f"Repository root not found at '{root}'; refusing to report a pass on a tree it did not read.")

    readmes = sorted(p for p in root.iterdir() if p.is_file() and p.name.startswith("README") and p.suffix == ".md")
    if not readmes:
        fail(f"No README*.md found under '{root}'. A tree with no README must not report a pass.")

    violations: list[str] = []
    checked = 0

    for readme in readmes:
        content = read_text(readme)
        for match in IMAGE.finditer(content):
            target = match.group(1)
            # An absolute URL is someone else's asset and cannot be checked from here.
            if ABSOLUTE_URL.match(target):
                continue
            checked += 1
            resolved = readme.parent / target
            if not resolved.exists():
                violations.append(
                    f"{readme.name} references '{target}', which does not exist. A missing image "
                    "renders as a broken icon on the project's front page."
                )
                continue
            if resolved.stat().st_size <= 0:
                violations.append(f"{readme.name} references '{target}', which exists but is empty.")

    if checked == 0:
        # A README with no local images is legitimate; a regex that stopped matching is not.
        # Say which case this is rather than passing silently.
        print(
            f"README asset check: no local image references found across {len(readmes)} README "
            "file(s); nothing to verify."
        )
        return 0

    if violations:
        fail_with_violations(violations)

    print(
        f"README asset check passed: {checked} local image reference(s) across {len(readmes)} "
        "README file(s) all resolve to a non-empty file."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
