---
name: songbird-release
description: SongBird repository release workflow for D:\sss\v2rayq. Use when the user asks to publish a version, publish a new version, release a new version, 发布版本, 发布新版本, or release new version for SongBird/v2rayq.
---

# SongBird Release

Use this skill only for the SongBird repository at `D:\sss\v2rayq`. Follow the workflow in order. Do not skip the explicit version confirmation step.

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

5. Build release and run tests.
   - Configure and build release:
     ```powershell
     cmake --preset msvc-release
     cmake --build --preset msvc-release --parallel
     ```
   - Run the repository test suite. The normal test flow uses the debug preset with tests enabled:
     ```powershell
     cmake --preset msvc-debug -DBUILD_TEST=ON
     cmake --build --preset msvc-debug --parallel
     ctest --test-dir build/msvc-debug --output-on-failure
     ```
   - If a release test preset exists or the user specifically requires release tests, also run the release test target/CTest directory.
   - Stop and report failures instead of publishing.

6. Commit, tag, and push.
   - Review `git status --short` and avoid staging unrelated user-local data.
   - Use a concise imperative commit title such as `Release 2.2.2`.
   - Commit all intended release changes.
   - Create an annotated tag `v<version>`:
     ```powershell
     git tag -a v<version> -m "Release <version>"
     ```
   - Push code and tag:
     ```powershell
     git push
     git push origin v<version>
     ```
   - If the remote or branch requires a different push command, inspect `git remote -v` and `git branch --show-current` first.

7. Publish GitHub release with `songbird.exe`.
   - Verify the release binary exists at `build\msvc-release\src\SongBird.exe`.
   - Copy it to a lowercase release asset name:
     ```powershell
     Copy-Item build\msvc-release\src\SongBird.exe build\msvc-release\songbird.exe -Force
     ```
   - Publish with GitHub CLI:
     ```powershell
     gh release create v<version> build\msvc-release\songbird.exe --title "SongBird <version>" --generate-notes
     ```
   - If the release already exists, upload or replace the asset explicitly:
     ```powershell
     gh release upload v<version> build\msvc-release\songbird.exe --clobber
     ```
   - If `gh` is unavailable or not authenticated, stop and tell the user exactly what is missing.

## Guardrails

- Keep the user informed before destructive or externally visible actions.
- Do not create a tag, push, or publish a GitHub release if build or tests fail.
- Do not include local config files, generated runtime configs, secrets, subscriptions, or user state files in the commit.
- Use non-interactive git commands where possible.
- After publishing, report the version, tag, commit hash, release asset, and verification commands that passed.
