# Verifies that the version the application reports comes from exactly one place, and that every
# consumer of it agrees.
#
# Why this exists: CMakeLists.txt declares project(SongBird VERSION x.y.z) and composes
# SONGBIRD_DISPLAY_VERSION from it plus SONGBIRD_VERSION_SUFFIX. That is already a single source, but
# nothing checked that the composition is still what the build passes to the compiler and the macOS
# bundle, and nothing checked a release tag against it -- both are silent when they drift. A tag that
# disagrees with project(VERSION) publishes a binary whose About dialog, update check and release page
# all claim different numbers.
#
# Usage:
#   check-version-consistency.ps1 [-Tag v2.6.0]
#
# Without -Tag only the in-tree checks run, which is what a developer wants before committing.
# The release workflow passes -Tag, which adds the tag-versus-declaration comparison.
[CmdletBinding()]
param(
    [string]$Tag,
    [string]$CMakeListsPath = (Join-Path $PSScriptRoot ".." "CMakeLists.txt"),
    [string]$SrcCMakeListsPath = (Join-Path $PSScriptRoot ".." "src" "CMakeLists.txt")
)

$ErrorActionPreference = 'Stop'

$violations = New-Object System.Collections.Generic.List[string]

function Stop-CheckWithError {
    param([string]$Message)
    Write-Error $Message -ErrorAction Continue
    exit 1
}

if (-not (Test-Path -LiteralPath $CMakeListsPath)) {
    Stop-CheckWithError "CMakeLists.txt not found at '$CMakeListsPath'; refusing to report a pass."
}
if (-not (Test-Path -LiteralPath $SrcCMakeListsPath)) {
    Stop-CheckWithError "src/CMakeLists.txt not found at '$SrcCMakeListsPath'; refusing to report a pass."
}

$root = Get-Content -LiteralPath $CMakeListsPath -Raw

if ($root -notmatch 'project\s*\(\s*SongBird\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    Stop-CheckWithError "Could not read project(SongBird VERSION x.y.z) from '$CMakeListsPath'."
}
$declaredVersion = $Matches[1]

if ($root -notmatch 'set\s*\(\s*SONGBIRD_VERSION_SUFFIX\s+"([^"]*)"\s*\)') {
    Stop-CheckWithError "Could not read SONGBIRD_VERSION_SUFFIX from '$CMakeListsPath'."
}
$suffix = $Matches[1]

if ($root -notmatch 'set\s*\(\s*SONGBIRD_DISPLAY_VERSION\s+"([^"]*)"\s*\)') {
    Stop-CheckWithError "Could not read SONGBIRD_DISPLAY_VERSION from '$CMakeListsPath'."
}
$displayDefinition = $Matches[1]
$expectedDisplay = "$declaredVersion$suffix"

# The composition itself must be the literal the file claims. If someone hardcodes the display
# version, project(VERSION) stops being the source and the two can drift.
if ($displayDefinition -ne '${PROJECT_VERSION}${SONGBIRD_VERSION_SUFFIX}') {
    $violations.Add(
        "SONGBIRD_DISPLAY_VERSION is '$displayDefinition' instead of " +
        "'`${PROJECT_VERSION}`${SONGBIRD_VERSION_SUFFIX}'; project(VERSION) is then no longer the " +
        "single source and the two can drift apart.")
}

# A second literal spelling of the version in build files or production sources is the drift this
# check exists to prevent, so it is reported rather than tolerated. Two exclusions, both deliberate:
#   * CHANGELOG.md -- its headings record past releases and are supposed to contain old numbers.
#   * tests/ -- a version-comparison test needs concrete numbers to compare, and those are test data
#     rather than a second declaration of what this build is.
# This script is excluded too, since it names the version it reads in its own diagnostics.
#
# The roots hang off the CMakeLists.txt being checked rather than off this script's own directory, so
# pointing -CMakeListsPath at a copy scans that copy instead of the working tree.
$buildRoot = Split-Path -Parent (Resolve-Path -LiteralPath $CMakeListsPath).Path
$scanRoots = @(
    (Resolve-Path -LiteralPath $CMakeListsPath).Path,
    (Join-Path $buildRoot "src"),
    (Join-Path $buildRoot "scripts")
)
$selfPath = (Resolve-Path -LiteralPath $PSCommandPath).Path
$versionPattern = [regex]::Escape($declaredVersion)
$literalHits = New-Object System.Collections.Generic.List[string]
foreach ($scanRoot in $scanRoots) {
    if (-not (Test-Path -LiteralPath $scanRoot)) { continue }
    $files = if (Test-Path -LiteralPath $scanRoot -PathType Leaf) {
        @(Get-Item -LiteralPath $scanRoot)
    } else {
        @(Get-ChildItem -LiteralPath $scanRoot -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in @('.txt', '.cmake', '.cpp', '.h', '.ps1', '.json') })
    }
    foreach ($file in $files) {
        if ($file.FullName -eq (Resolve-Path -LiteralPath $CMakeListsPath).Path) { continue }
        if ($file.FullName -eq $selfPath) { continue }
        $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($null -eq $content) { continue }
        if ($content -match $versionPattern) {
            $relative = Resolve-Path -Relative $file.FullName
            $literalHits.Add($relative)
        }
    }
}
foreach ($hit in $literalHits) {
    $violations.Add(
        "Hardcoded version '$declaredVersion' at $hit -- the version must come from " +
        "project(VERSION) in CMakeLists.txt, not from a second literal.")
}

# The build has to consume the composed value rather than re-spelling it.
$srcRoot = Get-Content -LiteralPath $SrcCMakeListsPath -Raw
if ($srcRoot -notmatch 'SONGBIRD_DISPLAY_VERSION') {
    $violations.Add(
        "src/CMakeLists.txt never references SONGBIRD_DISPLAY_VERSION; the executable's version " +
        "would come from somewhere other than project(VERSION).")
}

# Every #ifndef SONGBIRD_APP_VERSION fallback must be "unknown".
#
# This is the blind spot the check used to have. The literal scan above compares files against
# project(VERSION), so a fallback spelling an *older* version could never be caught by it -- and one
# did: both main.cpp files carried "2.5.0" while the project had moved two versions on. A fallback
# that names a real release is worse than no fallback, because a build that bypassed CMake then
# reports a version nobody is running and looks entirely normal doing it.
$fallbackHits = New-Object System.Collections.Generic.List[string]
# Split out so the success line at the bottom can quote it: "scanned 0 fallbacks" is what a stale
# pattern would report, and the number is the only visible evidence it did not.
$declaredFallbackCount = 0
$mainFiles = @(Get-ChildItem -LiteralPath (Join-Path $buildRoot "src") -Recurse -File -Include *.cpp -ErrorAction SilentlyContinue)
foreach ($file in $mainFiles) {
    $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($null -eq $content) { continue }
    if ($content -notmatch '(?m)^\s*#\s*ifndef\s+SONGBIRD_APP_VERSION\b') { continue }
    $declaredFallbackCount += 1
    if ($content -notmatch '#\s*define\s+SONGBIRD_APP_VERSION\s+"([^"]*)"') {
        $fallbackHits.Add("$(Resolve-Path -Relative $file.FullName): the SONGBIRD_APP_VERSION fallback defines no string")
        continue
    }
    $fallbackValue = $Matches[1]
    if ($fallbackValue -ne 'unknown') {
        $fallbackHits.Add(
            "$(Resolve-Path -Relative $file.FullName): the SONGBIRD_APP_VERSION fallback is " +
            "'$fallbackValue'; it must be 'unknown' so a build that bypassed CMake cannot report a " +
            "plausible version")
    }
}
if ($fallbackHits.Count -eq 0 -and $mainFiles.Count -gt 0) {
    # A missing fallback file is not a violation, but a run that found no #ifndef at all would
    # report success without having checked anything -- the same silent no-op this script exists to
    # avoid. Reaching here means no front end declares the macro at all, which is itself suspicious.
    $foundAnyFallback = $false
    foreach ($file in $mainFiles) {
        $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($null -ne $content -and $content -match 'SONGBIRD_APP_VERSION') { $foundAnyFallback = $true; break }
    }
    if (-not $foundAnyFallback) {
        $violations.Add(
            "No file under src/ mentions SONGBIRD_APP_VERSION; the executables cannot be reporting " +
            "the composed version at all.")
    }
}
foreach ($hit in $fallbackHits) {
    $violations.Add($hit)
}

# Optional tag comparison, used by the release workflow.
if (-not [string]::IsNullOrWhiteSpace($Tag)) {
    $tagVersion = $Tag -replace '^v', ''
    if ($tagVersion -ne $expectedDisplay) {
        $violations.Add(
            "Tag '$Tag' implies version '$tagVersion', but CMakeLists.txt declares " +
            "'$expectedDisplay'. Bump project(VERSION)/SONGBIRD_VERSION_SUFFIX in the tagged commit.")
    }
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

# Positive evidence the check actually read something, so a green run cannot be confused with a run
# that scanned nothing.
Write-Host ("Version consistency: declared '$declaredVersion', suffix '$suffix', display " +
            "'$expectedDisplay'; scanned $($scanRoots.Count) root(s), $($literalHits.Count) stray " +
            "literal(s), $declaredFallbackCount version fallback(s)" +
            $(if ($Tag) { ", tag '$Tag' verified" } else { "" }) + ".")
