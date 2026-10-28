"""Helpers every port uses: failure reporting, file walks, comment stripping."""

from __future__ import annotations

import os
import sys
from pathlib import Path


def repo_root() -> Path:
    # scripts/python/guardlib/common.py -> repository root
    return Path(__file__).resolve().parents[3]


def fail(message: str) -> "NoReturn":  # noqa: F821 - documented below
    """Report and exit 1.

    Every guard fails through here rather than raising, so the message lands on stderr
    the way the PowerShell scripts' Write-Error did and the exit code stays 1.
    """
    print(message, file=sys.stderr)
    raise SystemExit(1)


def fail_with_violations(violations, summary: str | None = None) -> None:
    body = "\n".join(f"- {v}" for v in violations)
    if summary:
        print(summary, file=sys.stderr)
    fail(body)


def walk_files(root: Path, extensions) -> list[Path]:
    """Every file under `root` with one of `extensions`, skipping hidden entries.

    Mirrors `Get-ChildItem -Recurse -File`: sorted, so two implementations report the
    same file order and a diff of their output is readable.
    """
    suffixes = tuple(extensions)
    found = []
    for directory, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        for name in sorted(filenames):
            if name.endswith(suffixes):
                found.append(Path(directory) / name)
    return found


def read_text(path: Path) -> str:
    # errors="replace" rather than a raise: the guards scan hand-written C++ and .ts, and
    # a decode error here would be a crash where the rule wants a reported violation.
    return path.read_text(encoding="utf-8-sig", errors="replace")


def relative_label(path: Path, root: Path) -> str:
    return f"{root.name}/{path.relative_to(root).as_posix()}"


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def blank_cpp_comments(text: str) -> str:
    """Replace // and /* */ comments with spaces, keeping offsets and newlines.

    Same contract as Remove-CppComments in check-hardcoded-colors.ps1: line numbers in a
    reported violation must match the file a reader opens.
    """
    out = list(text)
    index = 0
    length = len(text)
    while index < length:
        character = text[index]
        if character == "/" and index + 1 < length and text[index + 1] == "/":
            while index < length and text[index] != "\n":
                out[index] = " "
                index += 1
            continue
        if character == "/" and index + 1 < length and text[index + 1] == "*":
            while index < length:
                if text[index] == "*" and index + 1 < length and text[index + 1] == "/":
                    out[index] = " "
                    out[index + 1] = " "
                    index += 2
                    break
                out[index] = "\n" if text[index] == "\n" else " "
                index += 1
            continue
        index += 1
    return "".join(out)
