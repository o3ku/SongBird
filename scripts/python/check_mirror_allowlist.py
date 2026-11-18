#!/usr/bin/env python3
"""Port of scripts/check-mirror-allowlist.ps1 -- URL hosts in C++ sources are allowlisted.

Downloads go through three intentional GitHub mirrors (ghfast.top, mirror.ghproxy.com,
ghproxy.net) for regions where github.com is unreachable, and the checksum that vouches
for the bytes is anchored to the official GitHub channel instead (see
ReleaseChecksum.h). A new mirror host must therefore be a deliberate decision, not a
string literal nobody reads twice: every host that appears in an `http(s)://` URL in a
C++ source under src/ must be on the allowlist below. A host the list does not name
fails the build, and adding one means editing this file (and its PowerShell twin) in
the open, not slipping a prefix into GitHubMirrorHelper.h.

Hosts in comments are exempt: a comment names a host to explain history, and
GitHubMirrorHelper.h itself documents the mirrors that way.
"""

from __future__ import annotations

import re
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

# Hosts a URL in src/ may name. GitHub origins are covered by suffix below; the mirrors
# are the three intentional ones (see GitHubMirrorHelper.h); the rest are the services
# the code already talks to, grouped by purpose so a new entry has to say why it belongs.
ALLOWED_EXACT_HOSTS = frozenset(
    {
        "github.com",
        # Intentional GitHub mirrors.
        "ghfast.top",
        "mirror.ghproxy.com",
        "ghproxy.net",
        # Connectivity checks (OutboundLivenessProbeService, ProxyAvailabilityCheckService).
        "www.gstatic.com",
        "connectivitycheck.gstatic.com",
        "cp.cloudflare.com",
        "detectportal.firefox.com",
        "www.msftconnecttest.com",
        "www.google.com",
        # Exit-location lookups (OutboundLocationProbeService).
        "ifconfig.co",
        "ip-api.com",
        "ipapi.co",
        "ipinfo.io",
        "ipwho.is",
        # macOS login-item helper (MacAutoRunService).
        "www.apple.com",
        # Loopback and the DNS defaults (Config.h, ProxyGroupStatusPoller).
        "127.0.0.1",
        "localhost",
        "1.1.1.1",
        "223.5.5.5",
    }
)

# Any subdomain of a GitHub origin (api.github.com, raw.githubusercontent.com, ...).
ALLOWED_HOST_SUFFIXES = (
    ".github.com",
    ".githubusercontent.com",
)

URL_HOST_RE = re.compile(r"https?://([A-Za-z0-9._-]+)")


def is_allowed_host(host: str) -> bool:
    host = host.lower()
    if host in ALLOWED_EXACT_HOSTS:
        return True
    return host.endswith(ALLOWED_HOST_SUFFIXES)


def blank_comments_keep_strings(text: str) -> str:
    """Replace // and /* */ comments with spaces, keeping offsets and newlines.

    The shared blank_cpp_comments cannot be used here: it does not know about string
    literals, so the `//` in "https://ghfast.top/..." starts a comment and the host -- the
    thing this guard exists to catch -- is blanked out with it. This scanner tracks string,
    character and raw-string literals, so a `//` inside one stays code and a real comment is
    still stripped (a host named in a comment is history being explained, not a download
    being routed).
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
        for match in URL_HOST_RE.finditer(text):
            host = match.group(1).lower()
            if not is_allowed_host(host):
                violations.append(
                    f"URL host '{host}' is not on the mirror allowlist in "
                    f"{relative_label(path, root)}:{line_of(text, match.start(1))}"
                )

    if violations:
        fail_with_violations(violations)

    print(
        f"Mirror allowlist check passed: {len(files)} C++ file(s) scanned, "
        "every URL host is a GitHub origin, an intentional mirror, or an allowlisted service host."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
