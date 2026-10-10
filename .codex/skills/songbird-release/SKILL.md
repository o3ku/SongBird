---
name: songbird-release
description: SongBird repository release workflow for D:\sss\v2rayq. Use when the user asks to publish a version, publish a new version, release a new version, 发布版本, 发布新版本, or release new version for SongBird/v2rayq.
---

# SongBird Release

Use this skill only for the SongBird repository at `D:\sss\v2rayq`. Follow the workflow in order. Do not skip the explicit version confirmation step.

## Release Path

GitHub Actions is the only official release path. Pushing a `v*` tag makes
`.github/workflows/release.yml` build the static `x64-windows-static-md` binary, run
`ctest -LE smoke`, and publish `songbird.exe` with `gh release create`. Do not publish
from a local build unless the "Fallback: publish without CI" section applies.

What CI guarantees that a local build does not:

- The published asset is the static Qt build with the compiled `.qm` translations embedded
  into the executable, so every release ships the same kind of artifact. A local
  `msvc-release` binary depends on the developer machine's Qt and vcpkg setup instead.
- The test gate is structural. The publish step only runs after `ctest -LE smoke` passes.
- The tagged commit is always what gets built, so a stale local `build\msvc-release` tree
  cannot leak into a release.

## Release Workflow

1. Inspect the current version and release scope.
   - Read the root `CMakeLists.txt` line `project(SongBird VERSION x.y.z LANGUAGES CXX)`.
   - Inspect recent edits with `git status --short`, `git diff --stat`, and, when needed, `git diff` or recent commits.
   - Propose the next version:
     - Large, broad refactor or compatibility-breaking architectural change: increment the first/major number.
     - User-facing feature or substantial capability addition: increment the second/minor number.
     - Bug fix, translation polish, UI adjustment, documentation, packaging, or small maintenance change: increment the third/patch number.
   - Reset lower-order numbers when incrementing a higher-order number.

2. Confirm the version with the user.
   - State the current version, proposed new version, and one short reason for the bump level.
   - Ask the user to confirm before editing files.
   - Do not write the version, commit, tag, push, or publish until confirmed.

3. Write the confirmed version.
   - Update the root `CMakeLists.txt` `project(SongBird VERSION ...)` value, and the
     `SONGBIRD_VERSION_SUFFIX` line directly under it.
   - `project(VERSION)` accepts digits only. A prerelease label there is a configure error,
     not a warning, so the suffix is carried separately and appended into
     `SONGBIRD_DISPLAY_VERSION`. That display value is what `SONGBIRD_APP_VERSION` and the
     macOS bundle's two human-readable fields use. Bumping a beta is the one suffix line; a
     final release is that line emptied. `CFBundleVersion` deliberately keeps the numeric
     `PROJECT_VERSION`, because macOS compares it as a sequence of integers.
   - `src/app/main.cpp` and `src/auto/main.cpp` each carry an `#ifndef SONGBIRD_APP_VERSION`
     fallback that is bumped by hand, and it must match the display version exactly. Check
     the result rather than trusting it: `build/msvc-release/src/SongBird.exe --version`
     prints the string the application will report.
   - Preserve existing formatting.

4. Verify UI language and Chinese translation coverage.
   - Scan UI-facing strings in `src/ui` and app-level UI coordinators under `src/app`.
   - Visible default UI text should be English.
   - Proper nouns, product names, acronyms, protocol names, config enum values, sample URLs/IPs, JSON examples, object names, file names, and internal state keys do not need Chinese translation.
   - Treat Routing Settings Custom Rules action labels `BLOCK`, `DIRECT`, and `PROXY` as uppercase domain terms. Keep them uppercase English and do not wrap them for translation.
   - Wrap untranslated visible text in `tr(...)` or `QCoreApplication::translate(...)` using the nearest stable context.
   - Run the repository's own gates, which are the ones CI runs:
     ```powershell
     & .workbuddy-ai/tools/run-check-in-runspace.ps1 -Script scripts/check-localization-coverage.ps1 -TranslationFile translations/SongBird_zh_CN.ts
     & .workbuddy-ai/tools/run-check-in-runspace.ps1 -Script scripts/check-translations-fresh.ps1
     ```
     The helper contains each script's `exit 1` in a child runspace; read its `=== STATE ===` line
     (`hadErrors=False`) for the verdict, and its scan counts to confirm the check actually scanned
     something.
   - **Do not hardcode a Qt tool path here.** An earlier revision of this step called
     `D:\vcpkg\installed\x64-windows-static-md\tools\qt5\bin\lupdate.exe`, which does not exist on
     this machine — the static vcpkg Qt that CI builds against ships neither lupdate nor lrelease.
     Both scripts resolve the tool from PATH, the Qt prefix variables, or the `Qt5_DIR` recorded in
     `build/*/CMakeCache.txt`, which is how this machine finds `D:/local/Qt5/5.15.2/msvc2019_64`.
   - There is no separate lupdate step: `check-localization-coverage.ps1` runs lupdate itself and
     compares every extracted string, so an incomplete `.ts` fails check B.
   - The `.qm` is **committed** (`translations/compiled/SongBird_zh_CN.qm`, with the `.ts` hash it was
     built from in `SongBird_zh_CN.qm.ts.sha256`). `check-translations-fresh.ps1` proves the pair is
     in step by recompiling and comparing byte for byte. After editing the `.ts`, run that script
     with `-Update` and commit the `.qm` and the hash together — filling in translations without
     regenerating the `.qm` silently ships the previous revision of every changed string.

5. Run the test suite locally before tagging.
   - CI runs `ctest -LE smoke` and refuses to publish when it fails, but a local run
     catches the failure before a tag exists and has to be re-pushed.
   - Run the repository test suite. Do **not** use the `msvc-debug` preset: it is dead on this
     machine, because its cached compiler is `scoop/shims/clang++.exe` and the `llvm` app behind
     that shim has been removed, so it fails with
     `Could not create process ... scoop\apps\llvm\current\bin\clang++.exe`. Configure a test tree
     with the MSVC toolchain instead, and put Qt's `bin` on `PATH` before ctest or every QtTest
     executable exits 127 immediately:
     ```powershell
     $env:PATH = 'D:/local/Qt5/5.15.2/msvc2019_64/bin;' + $env:PATH
     cmake -S . -B build/msvc-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TEST=ON
     cmake --build build/msvc-tests --parallel
     ctest --test-dir build/msvc-tests --output-on-failure
     ```
   - A full build of a fresh MSVC tree used to stop at `SongBird.exe` / `SongBirdAuto.exe`
     with `mt.exe : command line error c10100a7`. That is fixed: the two Windows executables
     are linked with `LINKER:/INCREMENTAL:NO`, because `/MANIFESTINPUT:` together with
     `/MANIFEST:EMBED` is incompatible with the `/INCREMENTAL` MSVC passes by default for
     Debug and RelWithDebInfo. A recurrence means that flag was dropped, not that the tree is
     fine.
   - The `smoke`-labelled tests download cores and subscriptions, start real processes, and
     write real system-proxy / registry entries, so CI excludes them with `-LE smoke`. The
     command above does **not** pass `-LE smoke`, so it runs them too — that is fine and adds
     coverage, but it means the two counts differ by design: a full local run reports **59**
     tests while CI reports **58**. Before reporting a lost test, reconcile with
     `ctest --test-dir build/msvc-tests -N -L smoke` (exactly 1: `end-to-end-smoke`).
   - Four of those tests (`vcpkg-guard`, `vcpkg-skip`, `vcpkg-image-namespace`, `vcpkg-evict`)
     are not source checks: they extract the vcpkg cache steps' real `run:` blocks out of
     `.github/workflows/release.yml` and execute them under pwsh 7 against fixtures. They spawn
     child processes and take ~50 s together, which is why a full run is now ~2 min. A failure
     names the individual case (e.g. `C2 full drift, 35 -> 35 new names -> save`).
   - Everything in the `if(SONGBIRD_POWERSHELL_EXECUTABLE)` block disappears from the count if
     `find_program` cannot locate `pwsh` or `powershell`; that silently drops five source checks
     and these four harnesses.
   - Stop and report failures instead of tagging.

6. Commit and push `main` before tagging.
   - Review `git status --short` and avoid staging unrelated user-local data.
   - Use a concise imperative commit title such as `Release 2.4.3`.
   - Commit all intended release changes, then push:
     ```powershell
     git push origin main
     ```
   - This step is not optional. A `main` push builds without publishing and writes the
     vcpkg cache under the default-branch scope, which is the only scope a tag build can
     read from. Skipping it makes the next tag build compile Qt5 from source (~1.5h).
   - The `workflow` token scope is only required when pushing over HTTPS, because the
     restriction is imposed by the OAuth App mechanism. SSH keys carry no scopes, so an
     SSH push that adds or edits `.github/workflows/` is not affected. If an HTTPS push is
     rejected for that reason, either push over SSH (this repository's origin) or run
     `gh auth refresh -h github.com -s workflow` followed by `gh auth setup-git`, then retry.
   - If the remote rejects SSH, inspect `git remote -v` and `git branch --show-current`
     first. GitHub's `ssh.github.com:443` endpoint works on networks that block port 22.

7. Tag and let CI publish.
   - Create an annotated tag `v<version>` and push it:
     ```powershell
     git tag -a v<version> -m "Release <version>"
     git push origin v<version>
     ```
   - Pushing the tag runs `.github/workflows/release.yml`, which restores the vcpkg cache,
     builds the static Qt5 release, runs `ctest -LE smoke`, stages `build/src/SongBird.exe`
     as `songbird.exe`, and publishes the release.
   - A tag whose version carries a suffix (`v2.5.0-beta1`) makes CI add `--prerelease`, so a
     beta does not become the repository's "Latest release" while it is still a beta. The
     check is on the version, not on the tag shape, and plain `vX.Y.Z` tags publish exactly
     as they always did.
   - Do not call `gh release create` on this path. CI owns the asset, and a hand-made
     release bypasses the test gate.

8. Verify the published release.
   - Confirm the release exists with `songbird.exe` attached:
     ```powershell
     gh release view v<version>
     ```
   - Confirm the publisher on the release page is `github-actions`. Any other publisher
     means the release did not go through CI.
   - For a beta, confirm the release is marked prerelease and that
     `gh release view --json isPrerelease` agrees. A beta showing up as "Latest release"
     means the `--prerelease` branch in the workflow did not fire.
   - Report the version, tag, commit hash, release asset, and the tests that passed.

## Fallback: publish without CI

Use this only when CI cannot run at all: an Actions outage or quota, an urgent hotfix that
cannot wait for a build, or a fork with Actions disabled. The asset produced this way is
built from the local Qt and vcpkg setup rather than the static CI build, so say so
explicitly when reporting the release.

1. Run steps 1 through 5 unchanged.
2. Build the local release binary:
   ```powershell
   cmake --preset msvc-release
   cmake --build --preset msvc-release --parallel
   ctest --test-dir build/msvc-release --output-on-failure
   ```
3. Commit, push, tag, and publish with GitHub CLI. CI is unavailable on this path, so the
   tag push does not produce a release on its own:
   ```powershell
   Copy-Item build\msvc-release\src\SongBird.exe build\msvc-release\songbird.exe -Force
   gh release create v<version> build\msvc-release\songbird.exe --title "SongBird <version>" --generate-notes
   ```
4. If the release already exists, upload or replace the asset explicitly:
   ```powershell
   gh release upload v<version> build\msvc-release\songbird.exe --clobber
   ```
5. If `gh` is unavailable or not authenticated, stop and tell the user exactly what is missing.

## Guardrails

- Keep the user informed before destructive or externally visible actions.
- Do not create a tag, push, or publish a GitHub release if build or tests fail.
- Do not run `gh release create` or `gh release upload` on the normal path. CI creates the release, and a hand-made release skips the test gate.
- Do not include local config files, generated runtime configs, secrets, subscriptions, or user state files in the commit.
- Use non-interactive git commands where possible.
- Do not claim `SongBirdAuto.exe` was published. The release asset is `SongBird.exe` only.
- After publishing, report the version, tag, commit hash, release asset, and verification commands that passed.
