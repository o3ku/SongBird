param(
    [string]$SourceRoot = "src",
    [string]$TranslationFile = "translations/SongBird_zh_CN.ts"
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# The "0 unfinished" number reported by lupdate/lrelease is NOT a measure of
# localization coverage. It only counts entries that were already extracted into the
# .ts and are still awaiting a translation. Two whole classes of gap are invisible to it:
#
#   1. User-visible strings that were never wrapped in tr()/translate() at all. They
#      never reach the .ts, so there is nothing to be "unfinished". Every non-empty
#      OperationResult message lands in the log panel via AppBootstrap::appendResult()
#      and some of them raise a modal dialog via showOperationMessage(), so a bare
#      English literal here is user-visible text.
#   2. Strings that ARE wrapped in tr()/translate() but were never added to the .ts.
#      Because the .ts is hand-maintained (there is no lupdate build target), adding a
#      translate() call without updating the .ts silently leaves it untranslated.
#
# This script checks both. Check A is self-contained; Check B needs lupdate, which is an
# optional Qt component, so it is skipped loudly rather than silently.

# ---------------------------------------------------------------------------
# Check A: bare prose literals passed to OperationResult::{ok,fail,cancel}
# ---------------------------------------------------------------------------

# Strings that legitimately carry no translatable prose. Keys are the literal contents
# exactly as they appear between the quotes in C++.
$allowlist = @{
    '%1 %2'           = 'pure join template; both arguments are localized at the call site'
    'tun-compat | %1' = 'machine-readable log tag; the argument carries the text'
    'core | %1'       = 'machine-readable log tag; the argument carries the text'
    'tun | %1'        = 'machine-readable log tag; the argument carries the text'

    # The speed-test runtime writes pipe-delimited key=value diagnostic lines
    # ("URL Test batch | core failed to start: %1", "... | output=%2 | code=%3") that are
    # intended for log parsing, the same contract as the tags above. Translating the
    # tag would break that; the payload is already machine data. Revisit if these lines
    # are ever presented as user-facing prose rather than diagnostics.
    'URL Test batch | port allocation failed at entry %1' = 'pipe-delimited diagnostic line'
    'URL Test batch | temp dir failed'                    = 'pipe-delimited diagnostic line'
    'URL Test batch | failed to open config for write'    = 'pipe-delimited diagnostic line'
    'URL Test batch | no servers to test'                 = 'pipe-delimited diagnostic line'
    'URL Test batch | core missing'                       = 'pipe-delimited diagnostic line'
    'URL Test batch | core failed to start: %1'           = 'pipe-delimited diagnostic line'
    'URL Test batch | core not ready before timeout | output=%1' = 'pipe-delimited diagnostic line'
    'URL Test result | %1 -> %2'                          = 'pipe-delimited diagnostic line'
    'URL Test start failed | %1 | %2'                     = 'pipe-delimited diagnostic line'
    'URL Test proxy exited before ready | %1 | code=%2 | status=%3 | output=%4' = 'pipe-delimited diagnostic line'
    'URL Test startup timeout | %1 | output=%2'           = 'pipe-delimited diagnostic line'
    'URL Test config on timeout | %1 | %2'                = 'pipe-delimited diagnostic line'
    'URL Test failure detail | %1 | %2'                   = 'pipe-delimited diagnostic line'
    'URL Test: %1'                                        = 'pipe-delimited diagnostic line'
    'URL Test direct proxy: %1'                           = 'pipe-delimited diagnostic line'
}

# Functions whose string argument becomes a user-visible message. Extend this list
# whenever a new message-building helper is introduced -- a helper that takes a bare
# string and forwards it into an OperationResult is exactly the shape this check exists
# to catch. (saveFailureResult's `context` parameter is documented in
# persistence/IConfigRepository.h as describing what the app was doing, and it becomes
# the failure message verbatim when the repository has no extra detail to append.
# SongBirdAutoCoordinator's log() emits logMessage, which the SongBirdAuto window shows
# in its Logs dialog; setStatus() writes the status bar.)
$callPattern = [regex](
    '(?:OperationResult::(?:ok|fail|cancel)|saveFailureResult|(?<![\w.>])(?:log|setStatus))\s*\('
)
# Any of the three ways this codebase localizes a string.
$translatedPattern = [regex](
    'QCoreApplication::translate\(\s*"[^"]*"\s*,\s*"(?:[^"\\]|\\.)*"\s*\)' +
    '|QObject::tr\(\s*"(?:[^"\\]|\\.)*"\s*\)' +
    '|\btr\(\s*"(?:[^"\\]|\\.)*"\s*\)'
)
$literalPattern = [regex]'"(?:[^"\\]|\\.)*"'

# Returns the text between the opening parenthesis at $OpenParenIndex and its match.
# Parens inside string literals are counted too; that is acceptable for this scan and
# keeps the implementation readable.
function Get-BalancedArgument {
    param([string]$Text, [int]$OpenParenIndex)

    $depth = 0
    for ($i = $OpenParenIndex; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq '(') {
            $depth++
        } elseif ($ch -eq ')') {
            $depth--
            if ($depth -eq 0) {
                return $Text.Substring($OpenParenIndex + 1, $i - $OpenParenIndex - 1)
            }
        }
    }
    return $Text.Substring($OpenParenIndex + 1)
}

function Test-ProseLiteral {
    param([string]$LiteralContent)

    # Needs both a letter and whitespace to look like a sentence. This skips "%1",
    # separators like ", " and pure format strings.
    return ($LiteralContent -match '[A-Za-z]') -and ($LiteralContent -match '\s')
}

$root = Resolve-Path $SourceRoot
$violations = New-Object System.Collections.Generic.List[string]

$sourceFiles = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @('.h', '.cpp') }

# A check that scans nothing passes trivially, and a silent no-op is more dangerous than a
# red test -- it manufactures confidence. Refuse to report success in that case.
if (-not $sourceFiles) {
    Write-Error "No C++ sources found under '$SourceRoot'; refusing to report a pass."
    exit 1
}

$callSiteCount = 0
foreach ($file in $sourceFiles) {
    $text = [System.IO.File]::ReadAllText($file.FullName)
    foreach ($call in $callPattern.Matches($text)) {
        $callSiteCount++
        $argument = Get-BalancedArgument -Text $text -OpenParenIndex ($call.Index + $call.Length - 1)
        $stripped = $translatedPattern.Replace($argument, '')

        foreach ($literal in $literalPattern.Matches($stripped)) {
            $content = $literal.Value.Substring(1, $literal.Value.Length - 2)
            if (-not (Test-ProseLiteral $content)) {
                continue
            }
            if ($allowlist.ContainsKey($content)) {
                continue
            }

            $line = ($text.Substring(0, $call.Index) -split "`n").Count
            $relative = Resolve-Path -Relative $file.FullName
            $violations.Add(
                "Un-localized user-visible literal at ${relative}:${line} -- wrap it in " +
                "QCoreApplication::translate(""<Context>"", ...) or add it to the allowlist " +
                "in this script with a reason: `"$content`""
            )
        }
    }
}

# Same reasoning as above: if none of the known message sinks were found, the pattern has
# drifted out of sync with the code and a clean result would be meaningless.
if ($callSiteCount -eq 0) {
    Write-Error "Found no OperationResult/log/setStatus call sites under '$SourceRoot'; the scan pattern is stale. Refusing to report a pass."
    exit 1
}

# ---------------------------------------------------------------------------
# Check B: every string lupdate can extract is present in the .ts
# ---------------------------------------------------------------------------

# Some restricted shells ship a PATHEXT that omits .EXE. PowerShell then treats a native
# tool as a "document" and refuses to run it ("Cannot run a document in the middle of a
# pipeline"), which would silently disable this check. Normalize it for this process.
if ($env:PATHEXT -notmatch '(?i)\.EXE') {
    $env:PATHEXT = ".COM;.EXE;.BAT;.CMD" + $(if ($env:PATHEXT) { ";$env:PATHEXT" } else { "" })
}

function Resolve-LupdateExecutable {
    $candidates = @()

    $command = Get-Command lupdate -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    # The CMake presets point CMAKE_PREFIX_PATH at $env:QT5_PREFIX_PATH, so honour the
    # same variable here instead of hard-coding an install path.
    foreach ($variable in @('QT5_PREFIX_PATH', 'QT6_PREFIX_PATH', 'QTDIR')) {
        $prefix = [Environment]::GetEnvironmentVariable($variable)
        if ($prefix) {
            $candidates += (Join-Path $prefix 'bin/lupdate.exe')
        }
    }

    # That variable is usually unset in a fresh shell, which would make Check B silently
    # skip on a machine where Qt is plainly installed. The CMake cache records the exact
    # Qt the project was configured against, so prefer it -- same install the build uses.
    # Resolved from the script's own location so this works whatever the working directory.
    $repositoryRoot = Split-Path -Parent $PSScriptRoot
    foreach ($tree in @('build/msvc-release', 'build/msvc-tests')) {
        $cachePath = Join-Path $repositoryRoot (Join-Path $tree 'CMakeCache.txt')
        if (-not (Test-Path -LiteralPath $cachePath)) { continue }
        $entry = Select-String -LiteralPath $cachePath -Pattern '^Qt[56]_DIR:PATH=(.+)$' -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $entry) { continue }
        $qtDirectory = ($entry.Matches[0].Groups[1].Value.Trim()) -replace '\\', '/'
        if ($qtDirectory -match '^(?<prefix>.+)/lib/cmake/Qt[56]$') {
            $candidates += (Join-Path $Matches['prefix'] 'bin/lupdate.exe')
        }
    }

    # Ask Qt where its binaries live; this is how the build finds lrelease too.
    foreach ($qmakeName in @('qmake', 'qmake6')) {
        $qmake = Get-Command $qmakeName -ErrorAction SilentlyContinue
        if (-not $qmake) { continue }
        try {
            $binDir = (& $qmake.Source -query QT_INSTALL_BINS 2>$null)
            if ($LASTEXITCODE -eq 0 -and $binDir) {
                $candidates += (Join-Path ($binDir.Trim()) 'lupdate.exe')
            }
        } catch {
            # Ignore and fall through to the other candidates.
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }
    return $null
}

function Get-TranslationPairs {
    param([string]$Path)

    $pairs = @{}
    [xml]$document = Get-Content -LiteralPath $Path -Raw
    foreach ($context in $document.TS.context) {
        $name = $context.name
        foreach ($message in $context.message) {
            $source = $message.source
            if (-not $source) { continue }
            if (-not $pairs.ContainsKey($name)) {
                $pairs[$name] = New-Object System.Collections.Generic.HashSet[string]
            }
            [void]$pairs[$name].Add([string]$source)
        }
    }
    return $pairs
}

$lupdate = Resolve-LupdateExecutable
if (-not $lupdate) {
    Write-Host "NOTE: lupdate was not found, so Check B (untranslated-but-wrapped strings) was SKIPPED."
    Write-Host "      Install Qt's LinguistTools or put lupdate on PATH to enable it."
} elseif (-not (Test-Path -LiteralPath $TranslationFile)) {
    $violations.Add("Translation file not found: $TranslationFile")
} else {
    $scratch = [System.IO.Path]::Combine(
        [System.IO.Path]::GetTempPath(),
        "songbird-lupdate-$([System.Guid]::NewGuid().ToString('N')).ts"
    )
    try {
        $null = & $lupdate $SourceRoot -ts $scratch -no-obsolete 2>&1
        if ($LASTEXITCODE -ne 0) {
            $violations.Add("lupdate failed with exit code $LASTEXITCODE")
        } else {
            $extracted = Get-TranslationPairs -Path $scratch
            $existing = Get-TranslationPairs -Path $TranslationFile

            # If lupdate extracted nothing the comparison below is vacuous -- the same
            # silent-pass hazard as the empty-scan guard above.
            $extractedCount = 0
            foreach ($set in $extracted.Values) { $extractedCount += $set.Count }
            if ($extractedCount -eq 0) {
                $violations.Add(
                    "lupdate extracted no strings from '$SourceRoot', so comparing against " +
                    "$TranslationFile proves nothing; refusing to report a pass."
                )
            }

            foreach ($contextName in ($extracted.Keys | Sort-Object)) {
                $known = $existing[$contextName]
                foreach ($source in ($extracted[$contextName] | Sort-Object)) {
                    if ($known -and $known.Contains($source)) {
                        continue
                    }
                    $preview = if ($source.Length -gt 90) { $source.Substring(0, 87) + '...' } else { $source }
                    $violations.Add(
                        "translate()/tr() string missing from $TranslationFile -- add it, " +
                        "otherwise it renders in English: [$contextName] $preview"
                    )
                }
            }
        }
    } finally {
        if (Test-Path -LiteralPath $scratch) {
            Remove-Item -LiteralPath $scratch -Force
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Error (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
    exit 1
}

Write-Host "Localization coverage check passed."
