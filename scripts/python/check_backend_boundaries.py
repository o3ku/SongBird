#!/usr/bin/env python3
"""Port of scripts/check-backend-boundaries.ps1 -- the layering rules.

See that script for why each rule exists; the comments here only record where the port
had to make a decision the PowerShell version could leave implicit.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, line_of, read_text, walk_files  # noqa: E402

INCLUDE_PATTERN = re.compile(r'#include\s+"')


def find_includes(roots, pattern: str) -> list[str]:
    """`"path:line:content"` hits for `pattern`, in the shape the PS script built.

    ripgrep is not used. rg is a convenience tool, not a dependency, and the fallback scan
    in the original already implements the rule; a port that depended on rg would fail on
    a clean checkout of the very image this is meant to run in.
    """
    hits = []
    for root in roots:
        for path in walk_files(root, (".h", ".cpp")):
            for number, line in enumerate(read_text(path).splitlines(), start=1):
                if re.search(pattern, line):
                    hits.append(f"{path}:{number}:{line}")
    return hits


def main(argv) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path("src")
    if not source_root.exists():
        fail(f"Source root '{source_root}' does not exist; refusing to report a pass.")
    root = source_root.resolve()

    scanned_roots = 0
    scanned_files = 0
    violations: list[str] = []

    for legacy in (root / "runtime/core/xray", root / "runtime/core/singbox"):
        if legacy.exists():
            violations.append(f"Legacy concrete backend directory must stay out of runtime: {legacy}")

    consumer_names = ["app", "appcore", "auto", "ui", "services", "runtime"]
    consumers = [root / name for name in consumer_names if (root / name).is_dir()]
    if not consumers:
        fail(
            f"None of the consumer directories ({', '.join(consumer_names)}) exist under "
            f"'{source_root}'; the include rule would scan nothing. Refusing to report a pass."
        )
    scanned_roots += 1
    scanned_files += sum(len(walk_files(c, (".h", ".cpp"))) for c in consumers)

    # A directory holding a main.cpp is a front end. The coverage rule below is what makes a
    # new front end keep obeying the concrete-backend rule without being listed anywhere.
    front_ends = sorted(d for d in root.iterdir() if d.is_dir() and (d / "main.cpp").is_file())
    if not front_ends:
        fail(
            f"No directory under '{source_root}' contains a main.cpp; the front-end coverage "
            "guard would pass vacuously. Refusing to report a pass."
        )
    consumer_paths = {c.resolve() for c in consumers}
    for front_end in front_ends:
        if front_end.resolve() not in consumer_paths:
            violations.append(
                f"Front-end directory '{front_end}' is not covered by the concrete-backend "
                "include rule; add it to the consumer list in this script."
            )

    for hit in find_includes(consumers, r'#include\s+"backends/'):
        violations.append(f"Concrete backend include from common/application layer: {hit}")

    non_front_ends = [
        d for d in sorted(root.iterdir()) if d.is_dir() and d.resolve() not in {f.resolve() for f in front_ends}
    ]
    if not non_front_ends:
        fail(
            f"No non-front-end directory exists under '{source_root}'; the upward-include rule "
            "would scan nothing. Refusing to report a pass."
        )
    scanned_roots += 1
    scanned_files += sum(len(walk_files(d, (".h", ".cpp"))) for d in non_front_ends)

    for front_end in front_ends:
        name = re.escape(front_end.name)
        for hit in find_includes(non_front_ends, rf'#include\s+"{name}/'):
            violations.append(
                f"Front-end layer included from below; it is a top of the dependency order: {hit}"
            )

    backends = root / "backends"
    if not backends.is_dir():
        fail(
            f"Backend directory '{backends}' does not exist, so the reverse-direction rule would "
            "scan nothing. Refusing to report a pass."
        )
    scanned_roots += 1
    scanned_files += len(walk_files(backends, (".h", ".cpp")))
    for hit in find_includes([backends], r'#include\s+"(app|appcore|ui|services|platform)/'):
        violations.append(f"Backend may not include application/service/UI/platform layer: {hit}")

    if scanned_files == 0:
        fail(
            "The check read no source files at all; the roots are stale or missing. "
            "Refusing to report a pass."
        )

    if violations:
        fail_with_violations(violations)

    print(
        f"Backend boundary check passed: {scanned_roots} scan(s) over {scanned_files} scan "
        "target(s); legacy backend directories absent; 0 concrete-backend include(s) from the "
        "app layer; 0 reverse-direction include(s); 0 upward include(s) into a front end; "
        f"{len(front_ends)} front-end directory/directories covered by the include rule."
    )
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 2:
        print(f"usage: {sys.argv[0]} [SourceRoot]", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(main(sys.argv))
