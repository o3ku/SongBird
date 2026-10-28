#!/usr/bin/env python3
"""Port of scripts/check-appbootstrap-member-order.ps1.

Members of AppBootstrapObjects.h are destroyed in reverse declaration order, so a borrower
declared above the member it borrows is a double free at shutdown -- silently, only at
exit. See the PowerShell original for the full reasoning; the port keeps its parser shape
(delimiter matching that skips strings and comments, lambda bodies masked as deferred use)
because a second, cleverer parser would be a second thing to get wrong.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text  # noqa: E402

OPENERS = {"(": ")", "[": "]", "{": "}"}
CLOSERS = {")", "]", "}"}


def find_matching_delimiter(text: str, open_index: int, open_char: str, close_char: str) -> int:
    depth = 0
    in_string = in_char = in_line_comment = in_block_comment = False
    index = open_index
    length = len(text)
    while index < length:
        character = text[index]
        following = text[index + 1] if index + 1 < length else "\0"

        if in_line_comment:
            if character == "\n":
                in_line_comment = False
            index += 1
            continue
        if in_block_comment:
            if character == "*" and following == "/":
                in_block_comment = False
                index += 1
            index += 1
            continue
        if in_string:
            if character == "\\":
                index += 1
            elif character == '"':
                in_string = False
            index += 1
            continue
        if in_char:
            if character == "\\":
                index += 1
            elif character == "'":
                in_char = False
            index += 1
            continue

        if character == "/" and following == "/":
            in_line_comment = True
            index += 1
        elif character == "/" and following == "*":
            in_block_comment = True
            index += 1
        elif character == '"':
            in_string = True
        elif character == "'":
            in_char = True
        elif character == open_char:
            depth += 1
        elif character == close_char:
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return -1


def find_lambda_body_brace(text: str, open_bracket: int) -> int:
    """Index of the '{' opening a lambda body, or -1 when the bracket is not a lambda."""
    bracket_close = find_matching_delimiter(text, open_bracket, "[", "]")
    if bracket_close < 0:
        return -1

    index = bracket_close + 1
    while index < len(text) and text[index].isspace():
        index += 1

    if index < len(text) and text[index] == "(":
        paren_close = find_matching_delimiter(text, index, "(", ")")
        if paren_close < 0:
            return -1
        index = paren_close + 1
        while index < len(text) and text[index].isspace():
            index += 1

    # Trailing return type: `-> Type {`. Bounded so a stray `ptr->member` cannot swallow the
    # rest of the argument list.
    if index + 1 < len(text) and text[index] == "-" and text[index + 1] == ">":
        limit = min(len(text), index + 120)
        for cursor in range(index + 2, limit):
            if text[cursor] == "{":
                return cursor
            if text[cursor] in "();":
                return -1
        return -1

    specifiers = ("mutable", "noexcept", "constexpr", "consteval", "const")
    while True:
        consumed = False
        for specifier in specifiers:
            if text.startswith(specifier, index):
                after = index + len(specifier)
                boundary = after >= len(text) or not (text[after].isalnum() or text[after] == "_")
                if boundary:
                    index = after
                    while index < len(text) and text[index].isspace():
                        index += 1
                    consumed = True
                    break
        if not consumed:
            break

    if index < len(text) and text[index] == "{":
        return index
    return -1


def code_only(text: str) -> str:
    """`text` with comments, literals and lambda bodies blanked, offsets preserved.

    A dependency named inside a lambda body is a deferred use: the lambda runs on a button
    press or a timer, never while the object graph is torn down, so it imposes no ordering
    requirement. Blanking the body is what keeps this check from demanding an order that
    costs nothing.
    """
    chars = list(text)
    length = len(text)
    index = 0
    while index < length:
        character = text[index]
        following = text[index + 1] if index + 1 < length else "\0"

        if character == "/" and following == "/":
            while index < length and text[index] != "\n":
                chars[index] = " "
                index += 1
            continue

        if character == "/" and following == "*":
            chars[index] = " "
            chars[index + 1] = " "
            index += 2
            while index < length:
                if text[index] == "*" and index + 1 < length and text[index + 1] == "/":
                    chars[index] = " "
                    chars[index + 1] = " "
                    index += 2
                    break
                if text[index] != "\n":
                    chars[index] = " "
                index += 1
            continue

        if character in ('"', "'"):
            quote = character
            chars[index] = " "
            index += 1
            while index < length:
                if text[index] == "\\":
                    chars[index] = " "
                    if index + 1 < length:
                        chars[index + 1] = " "
                    index += 2
                    continue
                if text[index] == quote:
                    chars[index] = " "
                    index += 1
                    break
                if text[index] != "\n":
                    chars[index] = " "
                index += 1
            continue

        if character == "[":
            body_brace = find_lambda_body_brace(text, index)
            if body_brace >= 0:
                body_end = find_matching_delimiter(text, body_brace, "{", "}")
                if body_end < 0:
                    body_end = length - 1
                for cursor in range(index, body_end + 1):
                    if text[cursor] != "\n":
                        chars[cursor] = " "
                index = body_end + 1
                continue

        index += 1
    return "".join(chars)


def main(argv) -> int:
    source_root = Path(argv[1]) if len(argv) > 1 else Path("src")
    header_path = source_root / "app/AppBootstrapObjects.h"
    if not header_path.is_file():
        fail(f"Missing composition root header: {header_path}")

    members: dict[str, dict[str, int]] = {}
    declared = 0
    for number, line in enumerate(read_text(header_path).splitlines(), start=1):
        match = re.fullmatch(r"\s*std::unique_ptr<.+>\s+([A-Za-z_]\w*)\s*;\s*", line)
        if match:
            members[match.group(1)] = {"index": declared, "line": number}
            declared += 1

    if declared == 0:
        fail(
            f"No std::unique_ptr members parsed from {header_path} -- the parser is out of "
            "date, refusing to report success."
        )

    app_root = source_root / "app"
    wiring_files = sorted(app_root.glob("AppBootstrap*.cpp"))
    if not wiring_files:
        fail(f"No AppBootstrap*.cpp files found under {app_root} -- refusing to report success.")

    violations: list[str] = []
    unresolved: list[str] = []
    checked = 0
    deferred = 0

    for path in wiring_files:
        text = read_text(path)
        masked = code_only(text)
        short_name = path.name

        for match in re.finditer(r"objects_->([A-Za-z_]\w*)\s*=\s*std::make_unique\s*<", masked):
            target = match.group(1)
            line = text.count("\n", 0, match.start()) + 1

            angle_open = match.end() - 1
            angle_close = find_matching_delimiter(text, angle_open, "<", ">")
            if angle_close < 0:
                violations.append(f"{short_name}:{line}: unbalanced '<' in make_unique<{target}>")
                continue
            paren_open = text.find("(", angle_close)
            if paren_open < 0:
                violations.append(f"{short_name}:{line}: missing '(' after make_unique<{target}>")
                continue
            paren_close = find_matching_delimiter(text, paren_open, "(", ")")
            if paren_close < 0:
                violations.append(f"{short_name}:{line}: unbalanced '(' in make_unique<{target}>")
                continue

            arguments = masked[paren_open + 1 : paren_close]

            if target not in members:
                unresolved.append(
                    f"{short_name}:{line}: '{target}' is assigned but not declared in {header_path}"
                )
                continue

            seen = set()
            for dependency_match in re.finditer(r"objects_->([A-Za-z_]\w*)", arguments):
                dependency = dependency_match.group(1)
                if dependency == target or dependency in seen:
                    continue
                seen.add(dependency)

                if dependency not in members:
                    unresolved.append(
                        f"{short_name}:{line}: '{dependency}' is used but not declared in {header_path}"
                    )
                    continue

                checked += 1
                if members[dependency]["index"] >= members[target]["index"]:
                    violations.append(
                        f"{short_name}:{line}: '{target}' (declared at {header_path}:"
                        f"{members[target]['line']}, position {members[target]['index']}) borrows "
                        f"'{dependency}' (declared at line {members[dependency]['line']}, position "
                        f"{members[dependency]['index']}) but is destroyed FIRST -- declare "
                        f"'{target}' after '{dependency}' in AppBootstrapObjects.h"
                    )

        deferred += len(re.findall(r"objects_->[A-Za-z_]\w*", text)) - len(
            re.findall(r"objects_->[A-Za-z_]\w*", masked)
        )

    if checked == 0:
        fail(
            f"Parsed {len(wiring_files)} wiring file(s) but found no member dependencies to "
            "compare -- the scanner is out of date, refusing to report success."
        )

    if unresolved:
        fail_with_violations(unresolved)
    if violations:
        fail_with_violations(violations)

    print(
        f"AppBootstrap member order check passed: {checked} construction-time dependencies "
        f"across {len(wiring_files)} wiring file(s), {declared} members, {deferred} deferred "
        "(lambda) reference(s) ignored."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
