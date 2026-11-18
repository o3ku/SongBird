#!/usr/bin/env python3
"""Port of scripts/check-no-third-party-mirrors.ps1 -- downloads go to github.com only."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import (  # noqa: E402
    fail,
    fail_with_violations,
    line_of,
    read_text,
    relative_label,
    walk_files,
)

# The mirrors that used to be prepended to download URLs (GitHubMirrorHelper.h). A mirror is
# a man in the middle: it serves whatever bytes it chooses, and a checksum published beside
# the file on GitHub cannot vouch for bytes the mirror substituted. Downloads go to github.com
# only, so none of these hosts may appear in a C++ source.
FORBIDDEN_HOSTS = (
    "ghfast.top",
    "mirror.ghproxy.com",
    "ghproxy.net",
)


def blank_comments_keep_strings(text: str) -> str:
    """Replace // and /* */ comments with spaces, keeping offsets and newlines.

    The shared blank_cpp_comments cannot be used here: it does not know about string
    literals, so the `//` in "https://ghfast.top/..." starts a comment and the host -- the
    thing this guard exists to catch -- is blanked out with it. This scanner tracks string,
    character and raw-string literals, so a `//` inside one stays code and a real comment is
    still stripped (a host named in a comment is history being explained, not a download
    being routed, and GitHubMirrorHelper.h itself names all three that way).
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
        if character == "R" and index + 1 < length and text[index + 1] == '"':
            # Raw string R"delim( ... )delim": the content may hold quotes and // sequences.
            # The delimiter is at most 16 characters with no space, quote or backslash, and
            # the opening paren must come before the line ends; anything else is not a raw
            # string, and the quote is left for the string branch below.
            delimiter_end = index + 2
            while (
                delimiter_end < length
                and text[delimiter_end] not in " ()\\\n\""
                and delimiter_end - (index + 2) <= 16
            ):
                delimiter_end += 1
            if delimiter_end < length and text[delimiter_end] == "(":
                delimiter = text[index + 2 : delimiter_end]
                end = text.find(")" + delimiter + '"', delimiter_end + 1)
                index = length if end < 0 else end + len(delimiter) + 2
                continue
        if character == '"' or character == "'":
            index += 1
            while index < length:
                if text[index] == "\\":
                    index += 2
                    continue
                if text[index] == character:
                    index += 1
                    break
                if text[index] == "\n" and character == "'":
                    break
                index += 1
            continue
        index += 1
    return "".join(out)


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
        text = blank_comments_keep_strings(read_text(path))
        for host in FORBIDDEN_HOSTS:
            index = text.find(host)
            while index >= 0:
                violations.append(
                    f"third-party GitHub mirror host '{host}' in "
                    f"{relative_label(path, root)}:{line_of(text, index)}"
                )
                index = text.find(host, index + len(host))

    if violations:
        fail_with_violations(violations)

    print(
        f"Mirror check passed: {len(files)} C++ file(s) scanned, "
        "no third-party GitHub mirror host."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
