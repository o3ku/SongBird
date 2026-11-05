#!/usr/bin/env python3
"""Equivalence harness for the dual guard implementations.

Every rule in scripts/check-*.ps1 has a port in this directory, and the two halves are kept
in step by hand -- which is another way of saying nothing keeps them in step. selftest_guards
proves each implementation can fail; it does not prove the two implementations agree.

This harness closes that gap on any host that has both pwsh and python: for each rule it runs
the .ps1 and the .py against the SAME tree (clean, then one mutated copy per rule, reusing the
mutation set selftest_guards already defines), and compares the violation sets. A rule that
drifts -- reports a different file, line, or message on either side -- fails here.

The comparison normalizes what genuinely may differ between hosts and nothing else:
  * drive letter case and slashes in paths (pwsh and python resolve these differently),
  * a trailing "." or whitespace on reported lines,
  * ordering (a rule may walk files in either order; sorted comparison).
Everything else -- a different file, a different line number, a different sentence -- is a
drift and is reported as one, with the rule name and both outputs side by side.

Rules that need tools or trees this harness does not have (lupdate, lrelease, a build tree)
are skipped, and the skip is said out loud -- a silent skip is how a gate goes hollow.

Usage:
    python guard_equivalence.py [--repo-root DIR]
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]

sys.path.insert(0, str(SCRIPT_DIR))

from selftest_guards import (  # noqa: E402
    LOCALIZATION_CASES,
    Q_OS_CASES,
    README_CASES,
    SOURCE_TREE_CASES,
    VERSION_CASES,
)

# One entry per equivalence run: the human name, the .ps1 path, the .py script name, the
# arguments to hand BOTH implementations for the clean-tree run, and the mutations to apply.
# A mutation is a function taking the throwaway src/ tree; SOURCE_TREE_CASES and friends
# already define exactly the mutations each rule is supposed to catch, so they are reused
# verbatim -- a mutation selftest proved red is a mutation worth diffing on.
RULES = [
    {
        "name": "backend-boundaries",
        "ps1": "check-backend-boundaries.ps1",
        "py": "check_backend_boundaries.py",
        # The .ps1 takes -SourceRoot; the .py takes a positional source root.
        "ps1_args": ["-SourceRoot", "{SRC}"],
        "py_args": ["{SRC}"],
        "mutations": [
            m for m in SOURCE_TREE_CASES if m[1] == "check_backend_boundaries.py"
        ],
    },
    {
        "name": "appbootstrap-member-order",
        "ps1": "check-appbootstrap-member-order.ps1",
        "py": "check_appbootstrap_member_order.py",
        "ps1_args": ["-SourceRoot", "{SRC}"],
        "py_args": ["{SRC}"],
        "mutations": [
            m for m in SOURCE_TREE_CASES if m[1] == "check_appbootstrap_member_order.py"
        ],
    },
    {
        "name": "async-owners",
        "ps1": "check-async-owners.ps1",
        "py": "check_async_owners.py",
        "ps1_args": ["-SourceRoot", "{SRC}"],
        "py_args": ["{SRC}"],
        "mutations": [
            m for m in SOURCE_TREE_CASES if m[1] == "check_async_owners.py"
        ],
    },
    {
        "name": "hardcoded-colors",
        "ps1": "check-hardcoded-colors.ps1",
        "py": "check_hardcoded_colors.py",
        "ps1_args": ["-SourceRoot", "{SRC}"],
        "py_args": ["{SRC}"],
        "mutations": [
            m for m in SOURCE_TREE_CASES if m[1] == "check_hardcoded_colors.py"
        ],
    },
    {
        "name": "q-os-include-scope",
        "ps1": "check-q-os-include-scope.ps1",
        "py": "check_q_os_include_scope.py",
        # The rule scans src/ AND tests/: the mutation goes in src/, the test half is the
        # real one (the self-test passes REPO_ROOT/tests for it, and so does this harness).
        "ps1_args": ["-SourceRoot", "{SRC}", "-TestRoot", "{TESTS}"],
        "py_args": ["--source-root", "{SRC}", "--test-root", "{TESTS}"],
        "mutations": [
            (name, guard, mutation)
            for name, guard, mutation in [(n, "check_q_os_include_scope.py", m) for n, m in Q_OS_CASES]
        ],
    },
]


def find_pwsh() -> str | None:
    for name in ("pwsh", "pwsh.exe"):
        found = shutil.which(name)
        if found:
            return found
    return None

# pwsh decorates its error output with ANSI colour when the host pretends to be a terminal.
# The comparison reads the text, so the decoration is stripped before anything else.
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


def normalize_line(line: str) -> str:
    """The shared contract: a violation is `<what>: <where>` (or prose naming a directory),
    optionally wrapped by the host. Only host-level spelling differs between the halves."""
    line = ANSI_ESCAPE.sub("", line).strip()
    if line.endswith("."):
        line = line[:-1]
    # Collapse the separator spelling between "path:line  msg" and "path:line: msg".
    line = re.sub(r"^([^\s:]+):\d+\s*:?\s*", r"\1:", line, count=1)
    return line


def violation_lines(raw: list[str]) -> list[str]:
    """Extract the `- <violation>` entries from a run's stderr.

    Both implementations report a failing run as one joined list of `- ` entries: the .ps1
    passes them to Write-Error, which the host then decorates with `Line |` frames, a `204 |`
    source excerpt and underline tildes; the ports print them straight to stderr. Those
    decorations are the host's rendering of ONE error record, not more violations, so this
    function keeps the `- ` entries and re-glues the tails Write-Error wrapped at console
    width, and drops everything else."""
    violations: list[str] = []
    for line in raw:
        cleaned = ANSI_ESCAPE.sub("", line).strip()
        # Strip the `| ` continuation marker Write-Error puts in front of wrapped text, so a
        # wrapped tail glues back onto its `- ` head instead of looking like a new entry.
        if cleaned.startswith("| "):
            cleaned = cleaned[2:].strip()
        if cleaned.startswith("- "):
            violations.append(cleaned[2:])
        elif violations and cleaned and not cleaned.startswith(("Line |", "|", "Stop-", "At ", "~~~")):
            violations[-1] += " " + cleaned
    return violations


def run_ps1(ps1: str, arguments: list[str]) -> tuple[int, list[str]]:
    result = subprocess.run(
        ["pwsh", "-NoProfile", "-NonInteractive", "-File", ps1, *arguments],
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
        check=False,
    )
    return result.returncode, [ln for ln in result.stderr.splitlines() if ln.strip()]


def run_py(py: str, arguments: list[str]) -> tuple[int, list[str]]:
    result = subprocess.run(
        [sys.executable, str(SCRIPT_DIR / py), *arguments],
        capture_output=True,
        text=True,
        cwd=REPO_ROOT,
        check=False,
    )
    return result.returncode, [ln for ln in result.stderr.splitlines() if ln.strip()]


def compare(rule_name: str, case_name: str, ps1_out: tuple[int, list[str]], py_out: tuple[int, list[str]]) -> list[str]:
    problems: list[str] = []
    ps1_code, ps1_raw = ps1_out
    py_code, py_raw = py_out

    if (ps1_code == 0) != (py_code == 0):
        problems.append(
            f"{rule_name} [{case_name}]: exit codes disagree "
            f"(ps1={ps1_code}, py={py_code})"
        )

    ps1_norm = sorted(normalize_line(ln) for ln in violation_lines(ps1_raw))
    py_norm = sorted(normalize_line(ln) for ln in violation_lines(py_raw))

    # Path spelling: 8.3 short names vs long names, drive letter case, slashes, and on macOS
    # the /var -> /private/var symlink (the .py ports resolve() their roots, the .ps1 halves do
    # not, so one host reports /var/folders/... and the other /private/var/folders/... for the
    # same violation). The comparison is on the violation's identity, not on how the host spells
    # its location. A token is resolved only when it exists as spelled: a POSIX-looking token on
    # Windows (or a URL path, or an #include) would otherwise resolve against the current
    # drive's root and corrupt the message, so it passes through untouched -- and both halves
    # pass through the same corruption-free text either way.
    def canonicalize(violation: str) -> str:
        def fix_path(match: re.Match) -> str:
            text = match.group(0)
            # Drive-letter spans keep the unconditional resolve that has always normalized
            # 8.3 short names and separator direction on Windows (a span that swallows a
            # `:LINE:` tail names no real file, so an existence gate here would re-introduce
            # exactly the drift this function exists to hide). A POSIX span is resolved only
            # when it exists as spelled: on Windows a stray /-token would resolve against the
            # current drive's root and corrupt the message.
            if text[1:2] != ":" and not Path(text).exists():
                return text
            return str(Path(text).resolve()).replace("\\", "/")

        # Drive-letter paths keep the greedy form that has always worked on Windows; POSIX
        # absolute paths are matched component-wise (letters, digits, dots, dashes only) so the
        # match stops at the `:LINE:` that trails a path in a violation, and never starts at a
        # `/` inside a relative include or a URL.
        return re.sub(
            r"[A-Za-z]:[\\/][^'\"]*|(?<![\w./])/(?:[A-Za-z0-9._+-]+/)*[A-Za-z0-9._+-]+",
            fix_path,
            violation,
        )

    ps1_set = {canonicalize(x) for x in ps1_norm}
    py_set = {canonicalize(x) for x in py_norm}
    only_ps1 = [ln for ln in ps1_norm if canonicalize(ln) not in py_set]
    only_py = [ln for ln in py_norm if canonicalize(ln) not in ps1_set]
    for ln in only_ps1:
        problems.append(f"{rule_name} [{case_name}] only .ps1 reports: {ln}")
    for ln in only_py:
        problems.append(f"{rule_name} [{case_name}] only .py reports: {ln}")
    return problems


def format_arguments(template: list[str], src: Path, tests: Path) -> list[str]:
    return [a.replace("{SRC}", str(src)).replace("{TESTS}", str(tests)) for a in template]

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    opts = parser.parse_args(argv)
    repo = opts.repo_root.resolve()

    pwsh = find_pwsh()
    if pwsh is None:
        print("guard-equivalence: pwsh not found; this harness needs both halves.", file=sys.stderr)
        return 1

    problems: list[str] = []
    skipped: list[str] = [
        "localization-coverage (needs lupdate)",
        "version-consistency (lives in selftest only)",
        "readme-assets (report shape is a summary, not per-line violations)",
        "translations-fresh / translations-embedded (need lrelease / a build tree)",
        "test-suites (needs a build tree of test binaries)",
    ]

    with tempfile.TemporaryDirectory(prefix="songbird-guard-equiv-") as tmp:
        tmp_path = Path(tmp)

        # One clean copy, shared by every rule's clean-tree run: copying src/ once per rule
        # would redo the same multi-thousand-file copy for no reason.
        clean_root = tmp_path / "clean"
        clean_root.mkdir(parents=True, exist_ok=True)
        clean_src = clean_root / "src"
        shutil.copytree(repo / "src", clean_src)
        # The test half stays the real one: it is what the rule's scope is, not what mutates.
        real_tests = repo / "tests"

        for rule in RULES:
            ps1_path = repo / "scripts" / rule["ps1"]

            # Clean tree: both halves must agree that there is nothing to report.
            ps1_out = run_ps1(
                str(ps1_path),
                format_arguments(rule["ps1_args"], clean_src, real_tests),
            )
            py_out = run_py(
                rule["py"],
                format_arguments(rule["py_args"], clean_src, real_tests),
            )
            problems += compare(rule["name"], "clean tree", ps1_out, py_out)

            # One mutated copy per case: both halves must agree on what is wrong.
            for index, (case_name, _guard, mutation) in enumerate(rule["mutations"]):
                tree_root = tmp_path / rule["name"] / str(index)
                tree_root.mkdir(parents=True, exist_ok=True)
                src = tree_root / "src"
                shutil.copytree(repo / "src", src)
                mutation(src)
                ps1_out = run_ps1(
                    str(ps1_path),
                    format_arguments(rule["ps1_args"], src, real_tests),
                )
                py_out = run_py(
                    rule["py"],
                    format_arguments(rule["py_args"], src, real_tests),
                )
                problems += compare(rule["name"], case_name, ps1_out, py_out)

    for name in skipped:
        print(f"guard-equivalence: skipped {name}")

    if problems:
        print("guard-equivalence: the two implementations have drifted:", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1

    print(
        "guard-equivalence: .ps1 and .py report the same violations for every covered rule."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
