#!/usr/bin/env python3
"""Port of scripts/check-localization-coverage.ps1.

lupdate's "0 unfinished" is not a coverage metric: strings never wrapped in tr()/translate()
never reach the .ts, and strings wrapped but not added to the hand-maintained .ts render in
English. This checks five things:

  A   no bare prose literal handed to a message sink
  A2  no bare prose literal returned by a registered message helper
  B   every string lupdate can extract is present in the .ts (needs lupdate)
  C   strings passed to a runtime-translate helper exist in the .ts under its context
  D   no bare prose literal returned from any other function
  E   an English surface carries no translation machinery at all

Checks A, A2, C, D and E are self-contained; B needs lupdate and says so loudly when it is
missing rather than passing quietly. The Allowlist is the record of every literal that was
reviewed and deliberately left English -- read the PowerShell original for the reasoning
behind each entry; it is not duplicated here.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text, walk_files  # noqa: E402

# Keys are the literal contents exactly as they appear between the quotes in C++.
ALLOWLIST = {
    "%1 %2": "pure join template; both arguments are localized at the call site",
    "tun-compat | %1": "machine-readable log tag; the argument carries the text",
    "core | %1": "machine-readable log tag; the argument carries the text",
    "tun | %1": "machine-readable log tag; the argument carries the text",
    "%1 ms": "pure number + unit template; no translatable prose",
    "%1 %2 ms": "pure number + unit template; no translatable prose",
    "%1 B": "pure number + unit template; no translatable prose",
    "%1 KB": "pure number + unit template; no translatable prose",
    "%1 MB": "pure number + unit template; no translatable prose",
    "%1 GB": "pure number + unit template; no translatable prose",
    "yyyy-MM-dd HH:mm": "Qt date/time format tokens; no translatable prose",
    "import custom@%1": "generated placeholder remarks; persisted into the saved config",
    # Matched *against* core process output by isTunAdapterConflictOutput(); translating them
    # breaks TUN adapter-conflict detection outright.
    "configure tun interface": "matched against core process output; translating it breaks TUN conflict detection",
    "cannot create a file when that file already exists": "matched against core process output; translating it breaks TUN conflict detection",
    "open interface take too much time": "matched against core process output; translating it breaks TUN conflict detection",
    # fallbackUserAgent() -- an HTTP header, never rendered.
    "mihomo/1.19.28": "HTTP User-Agent header; never shown to the user",
    "clash-verge": "HTTP User-Agent header; never shown to the user",
    "FLClash": "HTTP User-Agent header; never shown to the user",
    "mihomo.party/v2.0.0 (clash.meta)": "HTTP User-Agent header; never shown to the user",
    "ClashMeta": "HTTP User-Agent header; never shown to the user",
    "v2ray": "HTTP User-Agent header; never shown to the user",
    "NekoBox/Android/1.4.1 (Prefer ClashMeta Format)": "HTTP User-Agent header; never shown to the user",
    "HiddifyNext": "HTTP User-Agent header; never shown to the user",
    "<no output>": "diagnostic payload inside the pipe-delimited URL Test lines",
    "proxies: []\\n": "serialised Clash YAML key, not user-facing prose",
    # Pipe-delimited key=value diagnostics from the speed-test runtime.
    "URL Test batch | port allocation failed at entry %1": "pipe-delimited diagnostic line",
    "URL Test batch | temp dir failed": "pipe-delimited diagnostic line",
    "URL Test batch | failed to open config for write": "pipe-delimited diagnostic line",
    "URL Test batch | no servers to test": "pipe-delimited diagnostic line",
    "URL Test batch | core missing": "pipe-delimited diagnostic line",
    "URL Test batch | core failed to start: %1": "pipe-delimited diagnostic line",
    "URL Test batch | core not ready before timeout | output=%1": "pipe-delimited diagnostic line",
    "URL Test result | %1 -> %2": "pipe-delimited diagnostic line",
    "URL Test start failed | %1 | %2": "pipe-delimited diagnostic line",
    "URL Test proxy exited before ready | %1 | code=%2 | status=%3 | output=%4": "pipe-delimited diagnostic line",
    "URL Test startup timeout | %1 | output=%2": "pipe-delimited diagnostic line",
    "URL Test config on timeout | %1 | %2": "pipe-delimited diagnostic line",
    "URL Test failure detail | %1 | %2": "pipe-delimited diagnostic line",
    "URL Test: %1": "pipe-delimited diagnostic line",
    "URL Test direct proxy: %1": "pipe-delimited diagnostic line",
}

# Functions whose string argument becomes a user-visible message.
CALL_PATTERN = re.compile(
    r"(?:OperationResult::(?:ok|fail|cancel)|saveFailureResult"
    r"|(?<![\w.>])(?:log|setStatus|unavailableResult))\s*\("
)

# Helpers that localize their argument through QCoreApplication::translate() with a context
# lupdate cannot see. An unregistered wrapper silently un-localizes every string it touches.
LOCALIZING_HELPERS = {
    "proxySessionText": "ProxySession",
    "trDialog": "AddServerDialog",
    "trCoreSettings": "CoreSettingsPageWidget",
    "trMainWindow": "MainWindow",
    "trayText": "TrayController",
}

# "file : function" pairs whose return value reaches the user. Keep this list short and
# deliberate: a helper belongs here only once you have confirmed its return value is shown.
MESSAGE_HELPERS = [
    ("src/persistence/JsonConfigRepository.cpp", "describeWriteFailure"),
    ("src/services/SubscriptionUpdateService.cpp", "appendSkippedSummary"),
    ("src/services/SubscriptionUpdateService.cpp", "appendParseNotes"),
    ("src/subscription/SubscriptionContentParser.cpp", "skippedSummary"),
    ("src/appcore/ProxyCrashRestartPolicy.cpp", "crashSummary"),
    ("src/appcore/ProxyCrashRestartPolicy.cpp", "restartDisabledMessage"),
    ("src/appcore/ProxyCrashRestartPolicy.cpp", "restartingMessage"),
    ("src/appcore/ProxyCrashRestartPolicy.cpp", "decide"),
    ("src/services/SpeedTestRuntimeRunner.cpp", "runUrlTest"),
    ("src/services/SpeedTestServiceInternal.h", "formatUrlProbeResult"),
    ("src/services/SpeedTestUrlProbe.cpp", "normalizeUpstreamProxyErrorText"),
    ("src/ui/models/LogListModel.cpp", "normalizeLine"),
    ("src/services/ServerService.cpp", "normalizedServerRemarks"),
]

STRING_BODY = r"(?:[^\"\\]|\\.)*"
LITERAL = re.compile(r'"(' + STRING_BODY + r')"')
TRANSLATED = re.compile(
    r'QCoreApplication::translate\(\s*"[^"]*"\s*,\s*"' + STRING_BODY + r'"\s*\)'
    r"|QObject::tr\(\s*\"" + STRING_BODY + r'"\s*\)'
    r"|\btr\(\s*\"" + STRING_BODY + r'"\s*\)'
    r"|"
    + "|".join(
        r"\b" + re.escape(name) + r"\(\s*\"" + STRING_BODY + r'"\s*\)' for name in sorted(LOCALIZING_HELPERS)
    )
)
SCAN_TOKEN = re.compile(r'"(?:[^"\\]|\\.)*"|//[^\n]*|/\*.*?\*/', re.DOTALL)
RETURN_WORD = re.compile(r"\breturn\b")


def balanced_argument(text: str, open_paren: int) -> str:
    depth = 0
    for index in range(open_paren, len(text)):
        character = text[index]
        if character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
            if depth == 0:
                return text[open_paren + 1 : index]
    return text[open_paren + 1 :]


def statement_text(text: str, start: int) -> str:
    depth = 0
    index = start
    while index < len(text):
        character = text[index]
        if character == '"':
            index += 1
            while index < len(text) and text[index] != '"':
                if text[index] == "\\":
                    index += 1
                index += 1
        elif character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
        elif character == ";" and depth <= 0:
            return text[start:index]
        index += 1
    return text[start:]


def function_body(text: str, definition_index: int):
    start = text.find("{", definition_index)
    if start < 0:
        return None
    depth = 0
    index = start
    while index < len(text):
        character = text[index]
        if character == '"':
            index += 1
            while index < len(text) and text[index] != '"':
                if text[index] == "\\":
                    index += 1
                index += 1
        elif character == "/" and text.startswith("//", index):
            newline = text.find("\n", index)
            if newline < 0:
                return None
            index = newline
        elif character == "/" and text.startswith("/*", index):
            close = text.find("*/", index + 2)
            if close < 0:
                return None
            index = close + 1
        elif character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1], start
        index += 1
    return None


def scan_views(text: str) -> tuple[str, str]:
    """(comment_free, masked): offsets preserved so line numbers survive both."""
    comment_free: list[str] = []
    masked: list[str] = []
    cursor = 0
    for token in SCAN_TOKEN.finditer(text):
        if token.start() > cursor:
            between = text[cursor : token.start()]
            comment_free.append(between)
            masked.append(between)
        value = token.group(0)
        if value.startswith('"'):
            comment_free.append(value)
            masked.append('""' + " " * (len(value) - 2))
        else:
            blanked = re.sub(r"[^\n]", " ", value)
            comment_free.append(blanked)
            masked.append(blanked)
        cursor = token.end()
    if cursor < len(text):
        tail = text[cursor:]
        comment_free.append(tail)
        masked.append(tail)
    return "".join(comment_free), "".join(masked)


def is_prose(content: str) -> bool:
    # Needs both a letter and whitespace to look like a sentence. KNOWN BLIND SPOT: this
    # hides every one-word message ("Failed", "Cancelled", ...). Do not relax it to
    # [A-Za-z] -- that also flags every enum-ish literal at a sink ("http", "socks",
    # "domain") and buries the signal.
    return re.search(r"[A-Za-z]", content) is not None and re.search(r"\s", content) is not None


def translation_pairs(path: Path) -> dict[str, set[str]]:
    pairs: dict[str, set[str]] = {}
    document = ET.parse(path).getroot()
    for context in document.findall("context"):
        name = context.findtext("name") or ""
        for message in context.findall("message"):
            source = message.findtext("source")
            if source is None:
                continue
            pairs.setdefault(name, set()).add(source)
    return pairs


def resolve_lupdate(repo_root: Path) -> str | None:
    candidate = shutil.which("lupdate") or shutil.which("lupdate-qt5")
    if candidate:
        return candidate
    import os

    for variable in ("QT5_PREFIX_PATH", "QT6_PREFIX_PATH", "QTDIR"):
        prefix = os.environ.get(variable)
        if prefix:
            for name in ("lupdate", "lupdate.exe"):
                path = Path(prefix) / "bin" / name
                if path.is_file():
                    return str(path)
    return None


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def main(argv) -> int:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", default=str(repo_root / "src"))
    parser.add_argument("--translation-file", default=str(repo_root / "translations/SongBird_zh_CN.ts"))
    parser.add_argument(
        "--english-surface-root",
        action="append",
        default=None,
        help="Trees whose every consumer is English-only; strings there must be bare literals.",
    )
    args = parser.parse_args(argv[1:])

    source_root = Path(args.source_root)
    translation_file = Path(args.translation_file)
    english_roots_arg = args.english_surface_root or [str(repo_root / "src/auto")]

    if not source_root.exists():
        fail(f"Source root '{source_root}' does not exist; refusing to report a pass.")
    root = source_root.resolve()

    all_sources = walk_files(root, (".h", ".cpp"))
    if not all_sources:
        fail(f"No C++ sources found under '{source_root}'; refusing to report a pass.")

    english_roots: list[Path] = []
    for surface in english_roots_arg:
        candidate = Path(surface)
        if not candidate.is_absolute():
            candidate = repo_root / candidate
        if not candidate.exists():
            fail(
                f"English-surface root '{surface}' does not exist (looked for '{candidate}'). Fix "
                "the path rather than dropping it: a missing root turns Check E into a no-op."
            )
        english_roots.append(candidate.resolve())

    def is_english_surface(path: Path) -> bool:
        return any(path.is_relative_to(surface) for surface in english_roots)

    english_files = [p for p in all_sources if is_english_surface(p)]
    source_files = [p for p in all_sources if not is_english_surface(p)]
    if not english_files:
        fail(
            f"English-surface roots ({', '.join(map(str, english_roots))}) contain no C++ "
            "sources, so Check E would scan nothing; refusing to report a pass."
        )
    if not source_files:
        fail(
            f"No C++ sources remain under '{source_root}' once the English surface(s) are "
            "removed; refusing to report a pass."
        )

    def relative(path: Path) -> str:
        return path.relative_to(repo_root).as_posix() if path.is_relative_to(repo_root) else str(path)

    violations: list[str] = []

    # --- Check E: an English surface must carry no translation machinery ---------------
    helper_alternation = "|".join(re.escape(name) for name in sorted(LOCALIZING_HELPERS))
    english_patterns = [
        (re.compile(r"QCoreApplication::translate\s*\("), "QCoreApplication::translate()"),
        (re.compile(r"(?<![\w])tr\s*\("), "tr()"),
        (re.compile(r"\bQT_TR[A-Z0-9_]*\s*\("), "a QT_TR_* macro"),
        (re.compile(rf"(?<![\w.])(?:{helper_alternation})\s*\("), "a runtime-translate helper"),
    ]
    english_call_count = 0
    for path in english_files:
        _, masked = scan_views(read_text(path))
        for pattern, what in english_patterns:
            for match in pattern.finditer(masked):
                english_call_count += 1
                violations.append(
                    f"{relative(path)}:{line_of(masked, match.start())} calls {what}, but this "
                    f"file is on an English surface ({', '.join(map(str, english_roots))}): every "
                    "executable that links it is English-only, so the wrapper asks for a "
                    "translation nothing can load. Use QStringLiteral(...) instead -- or, if this "
                    "tree is no longer English-only, remove it from the English-surface list in "
                    "the ctest registration and re-wrap its strings."
                )

    # --- Check A: bare prose literals passed to a message sink ------------------------
    call_site_count = 0
    for path in source_files:
        text = read_text(path)
        for call in CALL_PATTERN.finditer(text):
            call_site_count += 1
            argument = balanced_argument(text, call.end() - 1)
            stripped = TRANSLATED.sub("", argument)
            for literal in LITERAL.finditer(stripped):
                content = literal.group(1)
                if not is_prose(content) or content in ALLOWLIST:
                    continue
                violations.append(
                    f"Un-localized user-visible literal at {relative(path)}:"
                    f"{line_of(text, call.start())} -- wrap it in "
                    f'QCoreApplication::translate("<Context>", ...) or add it to the allowlist in '
                    f'this script with a reason: "{content}"'
                )
    if call_site_count == 0:
        fail(
            f"Found no OperationResult/log/setStatus call sites under '{source_root}'; the scan "
            "pattern is stale. Refusing to report a pass."
        )

    # --- Check A2: bare prose literals returned by a registered message helper --------
    helper_ranges: dict[Path, list[tuple[int, int]]] = {}
    message_helper_count = 0
    for helper_file, function in MESSAGE_HELPERS:
        path = repo_root / helper_file
        if not path.is_file():
            fail(
                f"Message helper '{function}' lists file '{helper_file}', which does not exist. "
                "Fix the registration rather than dropping it."
            )
        text = read_text(path)
        # Anchor on a definition, not the first mention: LogListModel.cpp calls
        # normalizeLine() before it defines it, and matching the call site would hand the
        # body reader a random later block.
        definition = re.search(
            r"(?m)^[^;=\n]*\b" + re.escape(function) + r"\s*\(", text
        )
        if not definition:
            fail(
                f"Message helper '{function}' was not found in '{helper_file}'. Fix the "
                "registration rather than dropping it."
            )
        body = function_body(text, definition.start())
        if body is None:
            fail(
                f"Could not read the body of message helper '{function}' in '{helper_file}'; "
                "refusing to report a pass."
            )
        body_text, body_offset = body
        if "return" not in body_text:
            fail(
                f"Read a body with no return for message helper '{function}' in '{helper_file}'; "
                "the definition lookup matched the wrong block. Refusing to report a pass."
            )
        message_helper_count += 1
        helper_ranges.setdefault(path.resolve(), []).append(
            (body_offset, body_offset + len(body_text))
        )

        for keyword in RETURN_WORD.finditer(body_text):
            statement = statement_text(body_text, keyword.end())
            stripped = TRANSLATED.sub("", statement)
            for literal in LITERAL.finditer(stripped):
                content = literal.group(1)
                if not is_prose(content) or content in ALLOWLIST:
                    continue
                absolute = body_offset + keyword.start()
                violations.append(
                    f"Un-localized user-visible literal returned by {function}() at "
                    f"{helper_file}:{line_of(text, absolute)} -- its return value reaches the "
                    "user, so wrap it in QCoreApplication::translate(\"<Context>\", ...) and add "
                    f'it to the translation file under that context: "{content}"'
                )
    if message_helper_count == 0:
        fail("None of the registered message helpers were inspected; refusing to report a pass.")

    # --- Check D: bare prose literals returned from any other function ----------------
    return_count = 0
    return_literal_count = 0
    for path in source_files:
        comment_free, masked = scan_views(read_text(path))
        ranges = helper_ranges.get(path.resolve(), [])
        for keyword in RETURN_WORD.finditer(masked):
            if any(start <= keyword.start() < end for start, end in ranges):
                continue
            return_count += 1
            statement = statement_text(comment_free, keyword.end())
            stripped = TRANSLATED.sub("", statement)
            for literal in LITERAL.finditer(stripped):
                return_literal_count += 1
                content = literal.group(1)
                if not is_prose(content) or content in ALLOWLIST:
                    continue
                violations.append(
                    f"Un-localized user-visible literal returned at "
                    f"{relative(path)}:{line_of(comment_free, keyword.start())} -- if this "
                    'return value reaches the user, wrap it in QCoreApplication::translate("<Context>", '
                    "...) and register the function in message_helpers; if it does not, add the "
                    f'literal to the allowlist in this script with a reason: "{content}"'
                )
    if return_count == 0:
        fail(f"Found no `return` statements under '{source_root}'; refusing to report a pass.")

    # --- Check C: a runtime-translate helper's string must be in the .ts --------------
    helper_call_site_count = 0
    if not translation_file.is_file():
        violations.append(f"Translation file not found: {translation_file}")
    else:
        existing = translation_pairs(translation_file)
        for helper in sorted(LOCALIZING_HELPERS):
            context = LOCALIZING_HELPERS[helper]
            helper_pattern = re.compile(
                r"\b" + re.escape(helper) + r'\(\s*"(' + STRING_BODY + r')"'
            )
            for path in source_files:
                text = read_text(path)
                for call in helper_pattern.finditer(text):
                    helper_call_site_count += 1
                    source = call.group(1)
                    if source in existing.get(context, set()):
                        continue
                    violations.append(
                        f'{helper}() localizes through context "{context}", but '
                        f"{relative(path)}:{line_of(text, call.start())} passes a string the "
                        f'translation file does not carry under that context, so it renders in '
                        f'English: "{source}"'
                    )
        if helper_call_site_count == 0:
            fail(
                "Found no call sites for any helper registered in LOCALIZING_HELPERS; either the "
                "registrations are stale or the call sites moved. Refusing to report a pass."
            )

    # --- Check B: every string lupdate can extract is present in the .ts --------------
    extracted_count = "n/a"
    lupdate = resolve_lupdate(repo_root)
    if not lupdate:
        print("NOTE: lupdate was not found, so Check B (untranslated-but-wrapped strings) was SKIPPED.")
        print("      Install Qt's LinguistTools or put lupdate on PATH to enable it.")
    elif translation_file.is_file():
        with tempfile.TemporaryDirectory() as scratch_dir:
            scratch = Path(scratch_dir) / "lupdate.ts"
            surface_scratch = Path(scratch_dir) / "lupdate-surface.ts"
            result = subprocess.run(
                [lupdate, str(source_root), "-ts", str(scratch), "-no-obsolete"],
                capture_output=True,
                check=False,
                text=True,
            )
            if result.returncode != 0:
                violations.append(f"lupdate failed with exit code {result.returncode}")
            else:
                extracted = translation_pairs(scratch)
                known = translation_pairs(translation_file)

                # Subtract what the English surfaces contribute on their own, so a wrapper
                # re-added there is reported by Check E ("remove the wrapper") rather than here
                # ("add it to the .ts"), which would be the opposite advice.
                surface_result = subprocess.run(
                    [lupdate, *[str(p) for p in english_roots], "-ts", str(surface_scratch), "-no-obsolete"],
                    capture_output=True,
                    check=False,
                    text=True,
                )
                if surface_result.returncode != 0:
                    violations.append(
                        f"lupdate failed with exit code {surface_result.returncode} while reading "
                        f"the English surface(s) {', '.join(map(str, english_roots))}"
                    )
                else:
                    for context in translation_pairs(surface_scratch).items():
                        name, surface_strings = context
                        if name in extracted:
                            extracted[name] -= surface_strings
                            if not extracted[name]:
                                del extracted[name]

                extracted_count = sum(len(v) for v in extracted.values())
                if extracted_count == 0:
                    violations.append(
                        f"lupdate extracted no strings from '{source_root}', so comparing against "
                        f"{translation_file} proves nothing; refusing to report a pass."
                    )
                for context in sorted(extracted):
                    for source in sorted(extracted[context]):
                        if source in known.get(context, set()):
                            continue
                        preview = source if len(source) <= 90 else source[:87] + "..."
                        violations.append(
                            f"translate()/tr() string missing from {translation_file} -- add it, "
                            f"otherwise it renders in English: [{context}] {preview}"
                        )

    if violations:
        fail_with_violations(violations)

    print(
        f"Localization coverage: A scanned {call_site_count} message-sink call site(s); "
        f"A2 inspected {message_helper_count} message helper(s); D scanned {return_count} "
        f"unregistered 'return' statement(s) ({return_literal_count} literal(s) read); C checked "
        f"{helper_call_site_count} localizing-helper call site(s); E checked "
        f"{english_call_count} translation call(s) across {len(english_files)} English-surface "
        f"file(s); B compared {extracted_count} lupdate-extracted string(s)."
    )
    print("Localization coverage check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
