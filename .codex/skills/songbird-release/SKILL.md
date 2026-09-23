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
   - Update only the root `CMakeLists.txt` `project(SongBird VERSION ...)` value unless the codebase has another authoritative version source.
   - Preserve existing formatting.

4. Verify UI language and Chinese translation coverage.
   - Scan UI-facing strings in `src/ui` and app-level UI coordinators under `src/app`.
   - Visible default UI text should be English.
   - Proper nouns, product names, acronyms, protocol names, config enum values, sample URLs/IPs, JSON examples, object names, file names, and internal state keys do not need Chinese translation.
   - Treat Routing Settings Custom Rules action labels `BLOCK`, `DIRECT`, and `PROXY` as uppercase domain terms. Keep them uppercase English and do not wrap them for translation.
   - Wrap untranslated visible text in `tr(...)` or `QCoreApplication::translate(...)` using the nearest stable context.
   - Run:
     ```powershell
     & 'D:\vcpkg\installed\x64-windows-static-md\tools\qt5\bin\lupdate.exe' src -no-obsolete -ts translations\SongBird_zh_CN.ts
     Select-String -Path translations\SongBird_zh_CN.ts -Pattern 'type="unfinished"','type="obsolete"'
     & 'D:\vcpkg\installed\x64-windows-static-md\tools\qt5\bin\lrelease.exe' translations\SongBird_zh_CN.ts -qm build\msvc-release\SongBird_zh_CN.check.qm
     Remove-Item build\msvc-release\SongBird_zh_CN.check.qm
     ```
   - Fill all unfinished Chinese translations before continuing.

5. Run the test suite locally before tagging.
   - CI runs `ctest -LE smoke` and refuses to publish when it fails, but a local run
     catches the failure before a tag exists and has to be re-pushed.
   - Run the repository test suite. The normal test flow uses the debug preset with tests enabled:
     ```powershell
     cmake --preset msvc-debug -DBUILD_TEST=ON
     cmake --build --preset msvc-debug --parallel
     ctest --test-dir build/msvc-debug --output-on-failure
     ```
   - The `smoke`-labelled tests download cores and subscriptions and start real processes,
     so they are excluded here and in CI.
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
   - Any push that adds or edits a file under `.github/workflows/` needs the `workflow`
     token scope. If the push is rejected for that reason, run
     `gh auth refresh -h github.com -s workflow` and retry.
   - If the remote rejects SSH, inspect `git remote -v` and `git branch --show-current`
     first. This repository stays reachable over HTTPS on networks that block port 22.

7. Tag and let CI publish.
   - Create an annotated tag `v<version>` and push it:
     ```powershell
     git tag -a v<version> -m "Release <version>"
     git push origin v<version>
     ```
   - Pushing the tag runs `.github/workflows/release.yml`, which restores the vcpkg cache,
     builds the static Qt5 release, runs `ctest -LE smoke`, stages `build/src/SongBird.exe`
     as `songbird.exe`, and publishes the release.
   - Do not call `gh release create` on this path. CI owns the asset, and a hand-made
     release bypasses the test gate.

8. Verify the published release.
   - Confirm the release exists with `songbird.exe` attached:
     ```powershell
     gh release view v<version>
     ```
   - Confirm the publisher on the release page is `github-actions`. Any other publisher
     means the release did not go through CI.
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
