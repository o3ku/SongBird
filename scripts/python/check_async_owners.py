#!/usr/bin/env python3
"""Port of scripts/check-async-owners.ps1 -- a worker must not hold a raw `this`.

A continuation that captures `this` is only safe when it is posted to `this` (Qt then drops
the call if the object is gone) or when it tests a lifetime token. The failure does not
surface where the bug is: the continuation runs later against freed memory and the crash
lands in whatever allocates that memory next.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import blank_cpp_comments, fail, fail_with_violations, line_of, read_text, walk_files  # noqa: E402

LAUNCH = re.compile(r"launchBackgroundThread\s*\(")
INVOKE = re.compile(r"QMetaObject::invokeMethod\s*\(")
FIRST_CAPTURE_LIST = re.compile(r"\[\s*([^\[\]]*)\]")
THIS_WORD = re.compile(r"\bthis\b")
EXPIRED = re.compile(r"\.expired\s*\(\s*\)")


def call_text(text: str, start: int) -> str:
    depth = 0
    for index in range(start, len(text)):
        character = text[index]
        if character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    return text[start:]


def first_capture_list(call: str) -> str:
    match = FIRST_CAPTURE_LIST.search(call)
    return match.group(1) if match else ""


def invoke_receiver(call: str) -> str:
    """Everything before the first top-level comma inside the call's parentheses."""
    open_paren = call.find("(")
    if open_paren < 0:
        return ""
    depth = 0
    out = []
    for character in call[open_paren + 1 :]:
        if character in "([{":
            depth += 1
        elif character in ")]}":
            if depth == 0:
                break
            depth -= 1
        elif character == "," and depth == 0:
            break
        out.append(character)
    return "".join(out).strip()


def main(argv) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path("src")
    if not source_root.exists():
        fail(f"Source root '{source_root}' does not exist; refusing to report a pass.")
    root = source_root.resolve()

    files = walk_files(root, (".cpp",))
    if not files:
        fail(f"No .cpp files under '{source_root}'; refusing to report a pass on an empty scan.")

    violations: list[str] = []
    scanned_calls = 0

    for path in files:
        text = blank_cpp_comments(read_text(path))
        relative = f"src/{path.relative_to(root).as_posix()}"

        for match in LAUNCH.finditer(text):
            scanned_calls += 1
            call = call_text(text, match.end() - 1)
            if not THIS_WORD.search(first_capture_list(call)):
                continue
            # The token is only useful if the continuation actually asks it something. A
            # captured but never-tested weak_ptr is a comment, not a guard.
            if EXPIRED.search(call):
                continue
            violations.append(
                f"{relative}:{line_of(text, match.start())}  launchBackgroundThread captures this "
                "with no lifetime guard (capture a token and test .expired() before touching the object)"
            )

        for match in INVOKE.finditer(text):
            scanned_calls += 1
            call = call_text(text, match.end() - 1)
            if not THIS_WORD.search(first_capture_list(call)):
                continue
            if EXPIRED.search(call) or "QPointer" in call:
                continue
            receiver = invoke_receiver(call)
            if receiver == "this":
                continue
            violations.append(
                f"{relative}:{line_of(text, match.start())}  invokeMethod posts to '{receiver}' "
                "while the continuation captures this (post to this, or hold a QPointer)"
            )

    if scanned_calls == 0:
        fail(
            "No launchBackgroundThread or QMetaObject::invokeMethod calls were found under "
            f"'{source_root}'. The patterns are stale, so this check would pass vacuously. "
            "Refusing to report a pass."
        )

    if violations:
        fail_with_violations(violations)

    print(
        f"Async ownership check passed: {scanned_calls} call site(s) across {len(files)} file(s); "
        "every continuation that captures this is guarded or posted to its own receiver."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
