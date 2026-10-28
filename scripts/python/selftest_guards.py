#!/usr/bin/env python3
"""Port of scripts/selftest-guards.ps1 -- the half that proves the guards can fail.

A guard that can never fail is worse than no guard: it produces the appearance of coverage.
The only way to know a check still does its job is to break what it is supposed to catch and
watch it go red. Every case below mutates a THROWAWAY COPY of the tree, runs the real guard
against it, and asserts a non-zero exit; the final pass asserts every guard is green on an
untouched copy, so a guard that rejects everything unconditionally fails too.

Only the guards with a Python port are covered here. The layout scope is: what the PR gate
runs is what the self-test exercises. check-test-suites is excluded because it needs a build
tree full of test binaries, and its own failure mode (a binary that runs zero cases) is
already a case a Qt Test suite reports directly.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]


def run_guard(script: str, arguments: list[str]) -> int:
    result = subprocess.run(
        [sys.executable, str(SCRIPT_DIR / script), *arguments],
        capture_output=True,
        check=False,
    )
    return result.returncode


def copy_tree(source: Path, destination: Path) -> Path:
    copied = destination / source.name
    shutil.copytree(source, copied)
    return copied


# Each case: name, guard, the arguments builder, and the mutation.
def _backend_leak(tree: Path) -> None:
    (tree / "services/SelftestBackendLeak.cpp").write_text(
        '#include "backends/xray/XrayCoreDescriptor.h"\n', encoding="utf-8"
    )


def _backend_upward_leak(tree: Path) -> None:
    (tree / "backends/xray/SelftestUpwardLeak.cpp").write_text(
        '#include "services/RoutingService.h"\n', encoding="utf-8"
    )


def _unlisted_front_end(tree: Path) -> None:
    # A new front end that is not in the guard's consumer list. Without the coverage rule this
    # directory would simply be skipped, which is exactly how `auto/` escaped the rule once.
    front_end = tree / "selftestfrontend"
    front_end.mkdir()
    (front_end / "main.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")


def _front_end_include_from_below(tree: Path) -> None:
    (tree / "appcore/SelftestFrontEndLeak.cpp").write_text(
        '#include "app/AppBootstrap.h"\n', encoding="utf-8"
    )


def _borrower_before_dependency(tree: Path) -> None:
    # SpeedTestCoordinator borrows mainWindow as its QObject parent, so declaring it above
    # mainWindow is a double free at shutdown.
    header = tree / "app/AppBootstrapObjects.h"
    lines = header.read_text(encoding="utf-8").splitlines(keepends=True)
    borrower_index = next(
        i for i, line in enumerate(lines) if "std::unique_ptr<SpeedTestCoordinator> speedTestCoordinator;" in line
    )
    anchor_index = next(i for i, line in enumerate(lines) if "std::unique_ptr<MainWindow> mainWindow;" in line)
    line = lines.pop(borrower_index)
    anchor_index = next(i for i, l in enumerate(lines) if "std::unique_ptr<MainWindow> mainWindow;" in l)
    lines.insert(anchor_index, line)
    header.write_text("".join(lines), encoding="utf-8")


def _async_leak(tree: Path) -> None:
    (tree / "appcore/SelftestAsyncLeak.cpp").write_text(
        "void f(const std::function<void(QThread*)>& track) {\n"
        "    launchBackgroundThread([this]() { doWork(); }, track);\n"
        "}\n",
        encoding="utf-8",
    )


def _invoke_leak(tree: Path) -> None:
    (tree / "appcore/SelftestInvokeLeak.cpp").write_text(
        "void g(QObject* other) {\n"
        "    QMetaObject::invokeMethod(other, [this]() { touch(); }, Qt::QueuedConnection);\n"
        "}\n",
        encoding="utf-8",
    )


def _color_leak(tree: Path) -> None:
    (tree / "ui/SelftestColorLeak.cpp").write_text(
        'void h() {\n    setStyleSheet(QStringLiteral("QWidget { background: #ff0000; }"));\n}\n',
        encoding="utf-8",
    )


def _palette_is_exempt(tree: Path) -> None:
    # This case asserts the OPPOSITE direction: the palette files are exempt, so a tree where
    # only they contain hex must still pass. A guard that rejects every hex anywhere cannot
    # satisfy it.
    target = tree / "ui/theme/AppThemePalette.cpp"
    target.write_text(
        target.read_text(encoding="utf-8") + '\nconst char* kSelftestPaletteColor = "#123456";\n',
        encoding="utf-8",
    )


def _bare_sink_literal(tree: Path) -> None:
    (tree / "services/SelftestLocalizationLeak.cpp").write_text(
        "OperationResult f() {\n    return OperationResult::fail(QStringLiteral(\"the update failed \"\n"
        "        \"for an unknown reason\"));\n}\n",
        encoding="utf-8",
    )


def _wrapper_on_english_surface(tree: Path) -> None:
    (tree / "auto/SelftestLocalizedSurface.cpp").write_text(
        "QString f() {\n    return QCoreApplication::translate(\"main\", \"Switch Server\");\n}\n",
        encoding="utf-8",
    )


def _q_os_without_qt(tree: Path) -> None:
    # The shape that made CoreAssetPlatform.cpp report Os::Other everywhere: a Q_OS_* test in a
    # file that reaches no Qt header at all, so the branch compiles out in silence.
    (tree / "common/SelftestPlatformLeak.cpp").write_text(
        "#ifdef Q_OS_WIN\nstatic const char* name = \"windows\";\n#else\nstatic const char* name = \"other\";\n#endif\n",
        encoding="utf-8",
    )


def _hardcoded_version_literal(tree: Path) -> None:
    # Read the declared version out of the tree under test rather than spelling it here: a
    # fixture with its own copy of the number would need editing on every bump.
    text = (tree / "CMakeLists.txt").read_text(encoding="utf-8")
    import re

    match = re.search(r"project\s*\(\s*SongBird\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    declared = match.group(1) if match else "1.0.0"
    (tree / "src/VersionLiteral.h").write_text(
        f'inline constexpr const char* kVersion = "{declared}";\n', encoding="utf-8"
    )


def _display_version_not_composed(tree: Path) -> None:
    path = tree / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")
    import re

    updated = re.sub(
        r'set\s*\(\s*SONGBIRD_DISPLAY_VERSION\s+"[^"]*"\s*\)',
        'set(SONGBIRD_DISPLAY_VERSION "9.9.9")',
        text,
    )
    path.write_text(updated, encoding="utf-8")


def _plausible_version_fallback(tree: Path) -> None:
    # The literal scan compares against project(VERSION), so a fallback spelling an *older*
    # release was invisible to it -- and both main.cpp files carried one for two versions.
    import re

    target = tree / "src/app/main.cpp"
    text = target.read_text(encoding="utf-8")
    updated = re.sub(
        r'#define\s+SONGBIRD_APP_VERSION\s+"[^"]*"', '#define SONGBIRD_APP_VERSION "9.9.9"', text
    )
    if updated == text:
        raise AssertionError("selftest fixture is stale: no SONGBIRD_APP_VERSION define matched")
    target.write_text(updated, encoding="utf-8")


def _missing_readme_image(tree: Path) -> None:
    (tree / "README.md").write_text("# Title\n\n![shot](docs/screenshots/missing.png)\n", encoding="utf-8")


def _empty_readme_image(tree: Path) -> None:
    (tree / "docs/screenshots/empty.png").write_text("", encoding="utf-8")
    (tree / "README.md").write_text("# Title\n\n![shot](docs/screenshots/empty.png)\n", encoding="utf-8")


def _broken_image_on_translated_page(tree: Path) -> None:
    (tree / "README_zh_CN.md").write_text(
        "# Title\n\n![shot](docs/screenshots/missing.png)\n", encoding="utf-8"
    )


def _stale_translation_hash(tree: Path) -> None:
    manifest = tree / "translations/compiled/SongBird_zh_CN.qm.ts.sha256"
    manifest.write_text("0" * 64 + "\n", encoding="utf-8")


SOURCE_TREE_CASES = [
    ("backend-boundaries: a service may not include a concrete backend", "check_backend_boundaries.py", _backend_leak),
    ("backend-boundaries: a backend may not include the service layer", "check_backend_boundaries.py", _backend_upward_leak),
    ("backend-boundaries: an unlisted front end must be reported", "check_backend_boundaries.py", _unlisted_front_end),
    ("backend-boundaries: nothing below a front end may include it", "check_backend_boundaries.py", _front_end_include_from_below),
    ("appbootstrap-member-order: a borrower declared before the member it borrows", "check_appbootstrap_member_order.py", _borrower_before_dependency),
    ("async-owners: a worker thread lambda capturing this with no guard", "check_async_owners.py", _async_leak),
    ("async-owners: an invokeMethod continuation posted elsewhere while capturing this", "check_async_owners.py", _invoke_leak),
    ("hardcoded-colors: a hex colour in a stylesheet literal outside the palette", "check_hardcoded_colors.py", _color_leak),
]

# Cases whose expected result is GREEN: they pin an exemption, so a guard that rejects
# everything cannot pass the suite.
GREEN_SOURCE_TREE_CASES = [
    ("hardcoded-colors: a palette file is allowed to name colours", "check_hardcoded_colors.py", _palette_is_exempt),
]

LOCALIZATION_CASES = [
    ("localization-coverage: a bare prose literal at a message sink", _bare_sink_literal),
    ("localization-coverage: a translate() wrapper on an English surface", _wrapper_on_english_surface),
]

Q_OS_CASES = [
    ("q-os-include-scope: a Q_OS_* test that reaches no Qt header", _q_os_without_qt),
]

VERSION_CASES = [
    ("version-consistency: a hardcoded version literal in src is rejected", _hardcoded_version_literal, []),
    ("version-consistency: a display version that does not compose from project(VERSION) is rejected", _display_version_not_composed, []),
    ("version-consistency: a release tag that disagrees with project(VERSION) is rejected", lambda tree: None, ["--tag", "v999.0.0"]),
    ("version-consistency: a plausible version fallback in main.cpp is rejected", _plausible_version_fallback, []),
]

README_CASES = [
    ("readme-assets: a reference to a missing image is rejected", _missing_readme_image, "README.md"),
    ("readme-assets: a reference to an empty file is rejected", _empty_readme_image, "README.md"),
    ("readme-assets: a broken image on a translated page is rejected", _broken_image_on_translated_page, "README_zh_CN.md"),
]


def main(argv) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", default=str(REPO_ROOT / "src"))
    args = parser.parse_args(argv[1:])

    source_path = Path(args.source_root).resolve()
    failures: list[str] = []

    with tempfile.TemporaryDirectory(prefix="songbird-guard-selftest-") as temp_root:
        temp_root_path = Path(temp_root)

        # --- guards over a copy of src/ ---------------------------------------------
        for name, guard, mutation in SOURCE_TREE_CASES + GREEN_SOURCE_TREE_CASES:
            tree = copy_tree(source_path, temp_root_path / f"t{len(failures)}-{abs(hash(name)) % 10**8}")
            mutation(tree)
            # These four take the source root positionally, exactly like the .ps1 originals'
            # -SourceRoot parameter; there is no shared flag spelling to standardise on.
            exit_code = run_guard(guard, [str(tree)])
            expect_green = name in {case[0] for case in GREEN_SOURCE_TREE_CASES}
            if expect_green:
                if exit_code != 0:
                    failures.append(f"{name}: the guard rejected a tree it is supposed to allow")
                else:
                    print(f"  green as expected: {name}")
            elif exit_code == 0:
                failures.append(f"{name}: the guard still reported success on a tree that violates it")
            else:
                print(f"  red as expected: {name}")

        # Untouched copies must pass, or a guard that fails unconditionally satisfies every case.
        clean_tree = copy_tree(source_path, temp_root_path / "clean")
        for guard in [
            "check_backend_boundaries.py",
            "check_appbootstrap_member_order.py",
            "check_async_owners.py",
            "check_hardcoded_colors.py",
        ]:
            if run_guard(guard, [str(clean_tree)]) != 0:
                failures.append(f"{guard}: reported a violation in an unmodified copy of the tree")
            else:
                print(f"  green on the clean tree: {guard}")

        # --- localization: needs src/ plus the .ts, because Check C reads it ----------
        for index, (name, mutation) in enumerate(LOCALIZATION_CASES):
            tree = copy_tree(source_path, temp_root_path / f"loc{index}")
            mutation(tree)
            exit_code = run_guard(
                "check_localization_coverage.py",
                [
                    "--source-root",
                    str(tree),
                    "--translation-file",
                    str(REPO_ROOT / "translations/SongBird_zh_CN.ts"),
                    "--english-surface-root",
                    str(tree / "auto"),
                ],
            )
            if exit_code == 0:
                failures.append(f"{name}: the guard still reported success on a tree that violates it")
            else:
                print(f"  red as expected: {name}")

        clean_localization_tree = copy_tree(source_path, temp_root_path / "loc-clean")
        if (
            run_guard(
                "check_localization_coverage.py",
                [
                    "--source-root",
                    str(clean_localization_tree),
                    "--translation-file",
                    str(REPO_ROOT / "translations/SongBird_zh_CN.ts"),
                    "--english-surface-root",
                    str(clean_localization_tree / "auto"),
                ],
            )
            != 0
        ):
            failures.append(
                "check_localization_coverage.py: reported a violation in an unmodified copy of the tree"
            )
        else:
            print("  green on the clean tree: check_localization_coverage.py")

        # --- q-os-include-scope: needs src/ AND tests/, both copied ------------------
        for index, (name, mutation) in enumerate(Q_OS_CASES):
            tree = copy_tree(source_path, temp_root_path / f"qos{index}")
            mutation(tree)
            exit_code = run_guard(
                "check_q_os_include_scope.py",
                ["--source-root", str(tree), "--test-root", str(REPO_ROOT / "tests")],
            )
            if exit_code == 0:
                failures.append(f"{name}: the guard still reported success on a tree that violates it")
            else:
                print(f"  red as expected: {name}")

        # --- version-consistency: operates on the build files, not on src/ -----------
        for index, (name, mutation, extra) in enumerate(VERSION_CASES):
            tree = temp_root_path / f"ver{index}"
            tree.mkdir()
            shutil.copy(REPO_ROOT / "CMakeLists.txt", tree / "CMakeLists.txt")
            shutil.copytree(REPO_ROOT / "src", tree / "src")
            (tree / "scripts").mkdir()
            for front_end in ("app", "auto"):
                (tree / "src" / front_end).mkdir(exist_ok=True)
            mutation(tree)
            exit_code = run_guard("check_version_consistency.py", ["--repo-root", str(tree), *extra])
            if exit_code == 0:
                failures.append(f"{name}: the guard still reported success on a tree that violates it")
            else:
                print(f"  red as expected: {name}")

        clean_version_tree = temp_root_path / "ver-clean"
        clean_version_tree.mkdir()
        shutil.copy(REPO_ROOT / "CMakeLists.txt", clean_version_tree / "CMakeLists.txt")
        shutil.copytree(REPO_ROOT / "src", clean_version_tree / "src")
        (clean_version_tree / "scripts").mkdir()
        if run_guard("check_version_consistency.py", ["--repo-root", str(clean_version_tree)]) != 0:
            failures.append(
                "check_version_consistency.py: reported a violation in an unmodified copy of the build files"
            )
        else:
            print("  green on the clean tree: check_version_consistency.py")

        # --- readme-assets: its own scratch tree ------------------------------------
        for index, (name, mutation, readme) in enumerate(README_CASES):
            tree = temp_root_path / f"readme{index}"
            (tree / "docs/screenshots").mkdir(parents=True)
            (tree / "README.md").write_text("# Title\n", encoding="utf-8")
            (tree / "README_zh_CN.md").write_text("# Title\n", encoding="utf-8")
            (tree / "docs/screenshots/shot.png").write_text("not a real png, but non-empty", encoding="utf-8")
            mutation(tree)
            if run_guard("check_readme_assets.py", [str(tree)]) == 0:
                failures.append(f"{name}: the guard still reported success on a README that violates it")
            else:
                print(f"  red as expected: {name}")

        clean_readme_tree = temp_root_path / "readme-clean"
        (clean_readme_tree / "docs/screenshots").mkdir(parents=True)
        (clean_readme_tree / "docs/screenshots/shot.png").write_text("png", encoding="utf-8")
        for name in ("README.md", "README_zh_CN.md"):
            (clean_readme_tree / name).write_text(
                "# Title\n\n![shot](docs/screenshots/shot.png)\n", encoding="utf-8"
            )
        if run_guard("check_readme_assets.py", [str(clean_readme_tree)]) != 0:
            failures.append(
                "check_readme_assets.py: reported a violation in a README whose images all resolve"
            )
        else:
            print("  green on the clean tree: check_readme_assets.py")

        # --- translations-fresh: the hash half needs no Qt tooling -------------------
        stale_tree = temp_root_path / "stale-translations"
        shutil.copytree(REPO_ROOT / "translations", stale_tree / "translations")
        _stale_translation_hash(stale_tree)
        if (
            run_guard(
                "check_translations_fresh.py",
                [
                    "--translation-file",
                    str(stale_tree / "translations/SongBird_zh_CN.ts"),
                    "--compiled-file",
                    str(stale_tree / "translations/compiled/SongBird_zh_CN.qm"),
                ],
            )
            == 0
        ):
            failures.append(
                "translations-fresh: an edited .ts with a stale recorded hash was accepted"
            )
        else:
            print("  red as expected: translations-fresh: a stale recorded .ts hash is rejected")

        if (
            run_guard(
                "check_translations_fresh.py",
                [
                    "--translation-file",
                    str(REPO_ROOT / "translations/SongBird_zh_CN.ts"),
                    "--compiled-file",
                    str(REPO_ROOT / "translations/compiled/SongBird_zh_CN.qm"),
                ],
            )
            != 0
        ):
            failures.append("check_translations_fresh.py: reported a violation on the committed pair")
        else:
            print("  green on the clean tree: check_translations_fresh.py")

    if failures:
        print("\n".join(f"- {f}" for f in failures), file=sys.stderr)
        raise SystemExit(1)

    print("Guard self-test passed: every rule went red when broken, and green when intact.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
