param(
    [string]$TranslationFile = "translations/SongBird_zh_CN.ts",
    [string]$CompiledFile = "translations/compiled/SongBird_zh_CN.qm",
    [switch]$Update
)

$ErrorActionPreference = "Stop"

# `$ErrorActionPreference = "Stop"` turns Write-Error into a terminating error, which would leave
# an `exit 1` written after it unreachable. Fail through this helper instead, matching the other
# scripts in this directory.
function Stop-CheckWithError {
    param([string]$Message)

    Write-Error $Message -ErrorAction Continue
    exit 1
}

# Why this check exists
# ---------------------
# The release pipeline builds against vcpkg's static Qt5, which ships neither lrelease nor Qt's own
# .qm files. The .qm files embedded into SongBird.exe therefore come from translations/compiled/,
# which is committed to the repository. A committed binary is only as trustworthy as the guarantee
# that it still matches the .ts it was produced from -- and nothing in the build enforces that. The
# failure mode is silent in the worst way: edit the .ts, forget lrelease, and the shipped UI keeps
# showing the previous revision of every changed string with a perfectly green build.
#
# There are two guarantees here because they are available in different environments:
#
#   1. A recorded hash of the .ts, compared against the current file. Needs no Qt tooling, so it
#      runs in CI. It catches "the .ts changed but the .qm was not regenerated".
#   2. A byte-exact recompile with lrelease and comparison against the committed .qm. lrelease
#      output is deterministic (verified), so any difference means the .qm was not produced from
#      this .ts -- a hand edit, a stale copy, or a .qm built by a different Qt. This needs
#      lrelease, so it is skipped loudly when the tool is absent rather than silently.
#
# Re-run with -Update after editing the .ts to regenerate the .qm and refresh the hash. That is
# the only supported way to update the pair; it keeps the two in step by construction.

$repositoryRoot = Split-Path -Parent $PSScriptRoot

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $repositoryRoot $Path)
}

function Get-Sha256 {
    param([string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

# Mirrors Resolve-LupdateExecutable in check-localization-coverage.ps1: PATH first, then the Qt
# prefix environment variables, then the exact Qt recorded in a CMake cache. The CMake cache is the
# most reliable source because it is the Qt the build was actually configured against, and it is
# resolved from the script's own location so this works from any working directory (ctest runs the
# tests with build/<tree>/tests as the working directory).
function Resolve-LreleaseExecutable {
    $candidates = @()

    $command = Get-Command lrelease -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    foreach ($variable in @('QT5_PREFIX_PATH', 'QT6_PREFIX_PATH', 'QTDIR')) {
        $prefix = [Environment]::GetEnvironmentVariable($variable)
        if ($prefix) {
            $candidates += (Join-Path $prefix 'bin/lrelease.exe')
        }
    }

    foreach ($tree in @('build/msvc-release', 'build/msvc-tests')) {
        $cachePath = Join-Path $repositoryRoot (Join-Path $tree 'CMakeCache.txt')
        if (-not (Test-Path -LiteralPath $cachePath)) { continue }

        $entry = Select-String -LiteralPath $cachePath -Pattern '^Qt[56]_DIR:PATH=(.+)$' -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $entry) { continue }

        $qtDirectory = ($entry.Matches[0].Groups[1].Value.Trim()) -replace '\\', '/'
        if ($qtDirectory -match '^(?<prefix>.+)/lib/cmake/Qt[56]$') {
            $candidates += (Join-Path $Matches['prefix'] 'bin/lrelease.exe')
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

$translationPath = Resolve-RepoPath $TranslationFile
$compiledPath = Resolve-RepoPath $CompiledFile
$manifestPath = "$compiledPath.ts.sha256"

if (-not (Test-Path -LiteralPath $translationPath -PathType Leaf)) {
    Stop-CheckWithError "Translation source not found: $translationPath"
}

$lrelease = Resolve-LreleaseExecutable

if ($Update) {
    if (-not $lrelease) {
        Stop-CheckWithError ("lrelease was not found, so $CompiledFile cannot be regenerated. " +
            "Install Qt's LinguistTools or put lrelease on PATH.")
    }

    $compiledDirectory = Split-Path -Parent $compiledPath
    if (-not (Test-Path -LiteralPath $compiledDirectory)) {
        $null = New-Item -ItemType Directory -Force -Path $compiledDirectory
    }

    $null = & $lrelease $translationPath -qm $compiledPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        Stop-CheckWithError "lrelease failed with exit code $LASTEXITCODE while writing $CompiledFile"
    }

    Set-Content -LiteralPath $manifestPath -Value (Get-Sha256 $translationPath) -Encoding ascii
    Write-Host "Updated $CompiledFile from $TranslationFile and refreshed $manifestPath."
    exit 0
}

$violations = New-Object System.Collections.Generic.List[string]

if (-not (Test-Path -LiteralPath $compiledPath -PathType Leaf)) {
    Stop-CheckWithError ("The compiled translation is missing: $compiledPath. " +
        "Run this script with -Update (needs lrelease) to produce it.")
}

$currentHash = Get-Sha256 $translationPath
$recordedHash = $null
if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
    $recordedHash = (Get-Content -LiteralPath $manifestPath -Raw).Trim().ToLowerInvariant()
}

if (-not $recordedHash) {
    $violations.Add("No recorded source hash at $manifestPath, so nothing proves that " +
        "$CompiledFile was built from the current $TranslationFile. Run this script with -Update.")
} elseif ($recordedHash -ne $currentHash) {
    $violations.Add("$TranslationFile has changed since $CompiledFile was generated " +
        "(recorded $recordedHash, current $currentHash). Run this script with -Update, then commit " +
        "both the .qm and its .ts.sha256.")
}

$recompiled = $false
if ($lrelease) {
    $scratch = [System.IO.Path]::Combine(
        [System.IO.Path]::GetTempPath(),
        "songbird-lrelease-$([System.Guid]::NewGuid().ToString('N')).qm"
    )
    try {
        $null = & $lrelease $translationPath -qm $scratch 2>&1
        if ($LASTEXITCODE -ne 0) {
            $violations.Add("lrelease failed with exit code $LASTEXITCODE")
        } else {
            $recompiled = $true
            if ((Get-Sha256 $scratch) -ne (Get-Sha256 $compiledPath)) {
                $violations.Add("Recompiling $TranslationFile does not reproduce $CompiledFile " +
                    "byte for byte, so the committed .qm did not come from this .ts (hand edit, " +
                    "stale copy, or a .qm built by a different Qt). Run this script with -Update.")
            }
        }
    } finally {
        if (Test-Path -LiteralPath $scratch) { Remove-Item -LiteralPath $scratch -Force }
    }
} else {
    Write-Host "NOTE: lrelease was not found, so the byte-exact recompile was SKIPPED. The recorded"
    Write-Host "      .ts hash was still checked, which is what catches an edited-but-not-regenerated"
    Write-Host "      .ts. Install Qt's LinguistTools or put lrelease on PATH to enable the rest."
}

if ($violations.Count -gt 0) {
    foreach ($violation in $violations) {
        Write-Host "  - $violation"
    }
    Stop-CheckWithError "Compiled translation is out of date ($($violations.Count) problem(s))."
}

# Print what was actually checked so a pass can be verified by eye: a check that reports nothing is
# indistinguishable from a check that did nothing.
$hashState = if ($recordedHash) { 'matched' } else { 'absent' }
Write-Host ("Checked compiled translation freshness: compared the SHA256 of $TranslationFile " +
    "(recorded hash $hashState); recompiled with lrelease: $(if ($recompiled) { 'yes' } else { 'no' }).")
Write-Host "Compiled translation freshness check passed."
