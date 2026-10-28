#!/usr/bin/env python3
"""Port of scripts/check-test-suites.ps1.

A test executable that runs zero cases reports success: Qt Test counts initTestCase and
cleanupTestCase as passes, so a suite whose `private slots:` lost its body prints
"Totals: 2 passed" and exits 0. This counts the real `PASS : Class::slot()` lines instead.

Re-executes every test binary, which is why it carries its own label (`suite`) and stays out
of the default ctest run.

Usage:
    check_test_suites.py --build-directory build/msvc-debug
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail  # noqa: E402

TOTALS = re.compile(r"(?m)^Totals:\s*(\d+)\s+passed")
PASS_LINE = re.compile(r"(?m)^PASS\s+:\s+\S+::(\w+)\s*\(")
SKIP_LINE = re.compile(r"(?m)^SKIP\s+:\s+\S+::(\w+)\s*\(")
FIXTURE_SLOTS = {"initTestCase", "cleanupTestCase", "init", "cleanup"}


def ctest_metadata(build_directory: Path, default_working_directory: Path) -> dict[str, dict]:
    """Per-executable labels/environment, read from ctest's own JSON export.

    Two facts from that metadata are load-bearing and both were learned the hard way:
      LABELS      a target labelled `smoke` is deliberately allowed to run zero cases;
      ENVIRONMENT a target may need QT_QPA_PLATFORM or a PATH entry, without which the binary
                  cannot start -- which looks identical to "ran nothing".
    Reading it from ctest means this script never keeps a second copy of the per-target
    configuration in sync with tests/CMakeLists.txt.
    """
    ctest = None
    for name in ("ctest", "ctest-qt5"):
        from shutil import which

        ctest = which(name)
        if ctest:
            break
    if not ctest:
        return {}

    result = subprocess.run(
        [ctest, "--test-dir", str(build_directory), "--show-only=json-v1"],
        capture_output=True,
        check=False,
        text=True,
    )
    if result.returncode != 0 or not result.stdout.strip():
        return {}
    try:
        parsed = json.loads(result.stdout)
    except json.JSONDecodeError:
        # A ctest that cannot export JSON leaves the map empty and every target is then held to
        # the strict rule -- which is the safe direction to fail in.
        return {}

    metadata: dict[str, dict] = {}
    for test in parsed.get("tests", []):
        command = test.get("command") or []
        if not command:
            continue
        key = Path(str(command[0])).name
        if not key:
            continue
        labels: list[str] = []
        environment: dict[str, str] = {}
        working_directory = str(default_working_directory)
        for prop in test.get("properties", []):
            if prop.get("name") == "LABELS":
                labels = list(prop.get("value", []))
            elif prop.get("name") == "ENVIRONMENT":
                for pair in prop.get("value", []):
                    text = str(pair)
                    split = text.find("=")
                    if split > 0:
                        environment[text[:split]] = text[split + 1 :]
            elif prop.get("name") == "WORKING_DIRECTORY":
                working_directory = str(prop.get("value"))
        metadata[key] = {
            "labels": labels,
            "environment": environment,
            "working_directory": working_directory,
        }
    return metadata


def main(argv) -> int:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-directory", default=str(repo_root / "build/msvc-debug"))
    args = parser.parse_args(argv[1:])

    build_directory = Path(args.build_directory)
    test_directory = build_directory / "tests"
    if not test_directory.is_dir():
        fail(
            f"No test directory at '{test_directory}'. Refusing to report a pass on a scan that "
            "found nothing."
        )

    suffix = ".exe" if os.name == "nt" else ""
    executables = sorted(
        p
        for p in test_directory.iterdir()
        if p.is_file() and p.name.startswith("songbird_") and p.name.endswith(f"_tests{suffix}")
    )
    if not executables:
        fail(
            f"No songbird_*_tests{suffix} under '{test_directory}'. A tree with no test binaries "
            "must not report a pass."
        )

    metadata = ctest_metadata(build_directory, test_directory)

    failures: list[str] = []
    checked = 0
    skipped = 0

    for executable in executables:
        checked += 1
        info = metadata.get(executable.name)
        if info and "smoke" in info["labels"]:
            # Allowed to run nothing by design: it exercises a live core, which CI does not have.
            skipped += 1
            continue

        # Apply ctest's own ENVIRONMENT so the binary starts the way it does under ctest. PATH is
        # deliberately not taken: Qt reaches the tests through ENVIRONMENT_MODIFICATION
        # (`path_list_prepend:`), a ctest-side directive rather than a value, and assigning it
        # verbatim would replace PATH with that literal string.
        saved = {}
        environment = dict(os.environ)
        if info:
            for name, value in info["environment"].items():
                if name == "PATH":
                    continue
                saved[name] = os.environ.get(name)
                environment[name] = value

        with tempfile.TemporaryDirectory() as scratch_dir:
            output_file = Path(scratch_dir) / "qt-test-report.txt"
            try:
                result = subprocess.run(
                    [str(executable), "-o", f"{output_file},txt"],
                    cwd=test_directory,
                    env=environment,
                    capture_output=True,
                    check=False,
                )
            except OSError as error:
                failures.append(f"{executable.name}: could not be started ({error})")
                continue
            finally:
                for name, value in saved.items():
                    if value is None:
                        os.environ.pop(name, None)
                    else:
                        os.environ[name] = value

            text = output_file.read_text(encoding="utf-8", errors="replace") if output_file.is_file() else ""

            if result.returncode != 0:
                failures.append(f"{executable.name}: exited {result.returncode}")
                continue

            totals = TOTALS.search(text)
            if not totals:
                failures.append(
                    f"{executable.name}: no 'Totals:' line -- the binary did not run a test report "
                    "at all"
                )
                continue

            real_cases = sum(1 for m in PASS_LINE.finditer(text) if m.group(1) not in FIXTURE_SLOTS)
            skipped_cases = sum(1 for m in SKIP_LINE.finditer(text) if m.group(1) not in FIXTURE_SLOTS)

            if real_cases <= 0 and skipped_cases > 0:
                # Every case declined to run on this platform: a decision the suite made, not a
                # suite that lost its body.
                skipped += 1
                continue

            if real_cases <= 0:
                failures.append(
                    f"{executable.name}: ran 0 test cases ({totals.group(0).strip()}) -- Qt Test "
                    "counts initTestCase/cleanupTestCase as passes, so a suite with no test "
                    "functions still reports success"
                )

    if failures:
        print("\n".join(f"- {f}" for f in failures), file=sys.stderr)
        raise SystemExit(1)

    print(
        f"Test suite check passed: {checked} executable(s) checked, each ran at least one test "
        f"case; {skipped} skipped by label (smoke)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
