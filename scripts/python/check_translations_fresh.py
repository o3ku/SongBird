#!/usr/bin/env python3
"""Port of scripts/check-translations-fresh.ps1 (the check half; --update is PowerShell only).

The .qm embedded into SongBird.exe comes from translations/compiled/, which is committed,
because the Qt CI builds against has no lrelease. A committed binary is only as trustworthy
as the guarantee that it still matches the .ts it came from, and nothing in the build
enforces that -- so a forgotten lrelease ships the previous revision of every changed string
with a perfectly green build.

Two guarantees, available in different environments:
  1. A recorded hash of the .ts (LF-normalised), compared against the current file. Needs no
     Qt tooling, so it is what CI can run.
  2. A byte-exact recompile with lrelease, compared against the committed .qm. Skipped loudly
     when lrelease is absent, never silently.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from guardlib.common import fail, fail_with_violations, read_text  # noqa: E402


def content_sha256(path: Path) -> str:
    # Line endings normalised to LF: the same committed content checks out as LF or CRLF
    # depending on core.autocrlf, and the hash is meant to be a property of the content.
    text = read_text(path).replace("\r\n", "\n")
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def find_lrelease(repo_root: Path) -> str | None:
    candidate = shutil.which("lrelease") or shutil.which("lrelease-qt5")
    if candidate:
        return candidate
    for variable in ("QT5_PREFIX_PATH", "QT6_PREFIX_PATH", "QTDIR"):
        prefix = os.environ.get(variable)
        if prefix:
            for name in ("lrelease", "lrelease.exe"):
                candidate = Path(prefix) / "bin" / name
                if candidate.is_file():
                    return str(candidate)
    return None


def main(argv) -> int:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--translation-file", default=str(repo_root / "translations/SongBird_zh_CN.ts"))
    parser.add_argument("--compiled-file", default=str(repo_root / "translations/compiled/SongBird_zh_CN.qm"))
    args = parser.parse_args(argv[1:])

    translation_path = Path(args.translation_file)
    compiled_path = Path(args.compiled_file)
    manifest_path = Path(str(compiled_path) + ".ts.sha256")

    if not translation_path.is_file():
        fail(f"Translation source not found: {translation_path}")
    if not compiled_path.is_file():
        fail(
            f"The compiled translation is missing: {compiled_path}. Re-run "
            "check-translations-fresh.ps1 with -Update (needs lrelease) to produce it."
        )

    violations: list[str] = []

    current_hash = content_sha256(translation_path)
    recorded_hash = ""
    if manifest_path.is_file():
        recorded_hash = read_text(manifest_path).strip().lower()

    if not recorded_hash:
        violations.append(
            f"No recorded source hash at {manifest_path}, so nothing proves that "
            f"{compiled_path.name} was built from the current {translation_path.name}. Run "
            "check-translations-fresh.ps1 with -Update."
        )
    elif recorded_hash != current_hash:
        violations.append(
            f"{translation_path.name} has changed since {compiled_path.name} was generated "
            f"(recorded {recorded_hash}, current {current_hash}). Re-run "
            "check-translations-fresh.ps1 with -Update, then commit both the .qm and its "
            ".ts.sha256."
        )

    lrelease = find_lrelease(repo_root)
    recompiled = False
    if lrelease:
        with tempfile.NamedTemporaryFile(suffix=".qm", delete=False) as handle:
            scratch = Path(handle.name)
        try:
            result = subprocess.run(
                [lrelease, str(translation_path), "-qm", str(scratch)],
                capture_output=True,
                check=False,
            )
            if result.returncode != 0:
                violations.append(f"lrelease failed with exit code {result.returncode}")
            else:
                recompiled = True
                if hashlib.sha256(scratch.read_bytes()).hexdigest() != hashlib.sha256(
                    compiled_path.read_bytes()
                ).hexdigest():
                    violations.append(
                        f"Recompiling {translation_path.name} does not reproduce "
                        f"{compiled_path.name} byte for byte, so the committed .qm did not come "
                        "from this .ts (hand edit, stale copy, or a .qm built by a different Qt)."
                    )
        finally:
            scratch.unlink(missing_ok=True)
    else:
        print(
            "NOTE: lrelease was not found, so the byte-exact recompile was SKIPPED. The recorded"
        )
        print(
            "      .ts hash was still checked, which is what catches an edited-but-not-regenerated"
        )
        print("      .ts. Install Qt's LinguistTools or put lrelease on PATH to enable the rest.")

    if violations:
        for violation in violations:
            print(f"  - {violation}")
        fail_with_violations([], f"Compiled translation is out of date ({len(violations)} problem(s)).")

    hash_state = "matched" if recorded_hash else "absent"
    print(
        f"Checked compiled translation freshness: compared the SHA256 of "
        f"{translation_path.name} (recorded hash {hash_state}); recompiled with lrelease: "
        f"{'yes' if recompiled else 'no'}."
    )
    print("Compiled translation freshness check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
