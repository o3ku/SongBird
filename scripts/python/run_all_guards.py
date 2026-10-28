#!/usr/bin/env python3
"""Run every Python guard against this checkout and report one line per rule.

Not a gate: `ctest -L py` (or plain ctest) already runs each one as its own test, so a
failure is attributed to a rule rather than to "the guard step". This exists for the case
that motivated the port in the first place -- someone has a shell and no cmake/Qt and wants
to run the static half before pushing.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]

# (label, script, args). Keep the order stable so two runs are diffable.
COMMANDS = [
    ("backend-boundaries", "check_backend_boundaries.py", [str(REPO_ROOT / "src")]),
    ("appbootstrap-member-order", "check_appbootstrap_member_order.py", [str(REPO_ROOT / "src")]),
    ("async-owners", "check_async_owners.py", [str(REPO_ROOT / "src")]),
    ("hardcoded-colors", "check_hardcoded_colors.py", [str(REPO_ROOT / "src")]),
    (
        "q-os-include-scope",
        "check_q_os_include_scope.py",
        ["--source-root", str(REPO_ROOT / "src"), "--test-root", str(REPO_ROOT / "tests")],
    ),
    (
        "localization-coverage",
        "check_localization_coverage.py",
        [
            "--source-root",
            str(REPO_ROOT / "src"),
            "--translation-file",
            str(REPO_ROOT / "translations/SongBird_zh_CN.ts"),
            "--english-surface-root",
            str(REPO_ROOT / "src/auto"),
        ],
    ),
    ("versions-consistent", "check_version_consistency.py", ["--repo-root", str(REPO_ROOT)]),
    ("readme-assets", "check_readme_assets.py", [str(REPO_ROOT)]),
    (
        "translations-fresh",
        "check_translations_fresh.py",
        [
            "--translation-file",
            str(REPO_ROOT / "translations/SongBird_zh_CN.ts"),
            "--compiled-file",
            str(REPO_ROOT / "translations/compiled/SongBird_zh_CN.qm"),
        ],
    ),
]


def main() -> int:
    failed = []
    for label, script, arguments in COMMANDS:
        result = subprocess.run(
            [sys.executable, str(HERE / script), *arguments],
            capture_output=True,
            text=True,
        )
        if result.returncode == 0:
            # A guard whose first line is a NOTE has skipped part of itself; say so instead of
            # quoting the note, which would read like the check did less than it did.
            lines = [line for line in result.stdout.strip().splitlines() if line.strip()]
            first = lines[0] if lines else ""
            detail = "passed" if first.startswith("NOTE") else first
            print(f"PASS  {label}  {detail}")
        else:
            failed.append(label)
            print(f"FAIL  {label}")
            for line in (result.stderr or result.stdout).strip().splitlines():
                print(f"        {line}")

    if failed:
        print(f"\n{len(failed)} guard(s) failed: {', '.join(failed)}", file=sys.stderr)
        return 1
    print(f"\nAll {len(COMMANDS)} Python guards passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
