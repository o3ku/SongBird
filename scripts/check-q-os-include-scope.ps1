param(
    [string]$SourceRoot = "src",
    [string]$TestRoot = "tests"
)

$ErrorActionPreference = "Stop"

# `$ErrorActionPreference = "Stop"` turns Write-Error into a terminating error, which would leave an
# `exit 1` written after it unreachable and hand the exit code to PowerShell's exception handling.
# The guards below fail through this helper instead: -ErrorAction Continue keeps the red stderr
# message, and `exit 1` then runs whatever the preference is in the caller's session.
function Stop-CheckWithError {
    param([string]$Message)

    Write-Marker "failing: $Message"
    Write-Error $Message -ErrorAction Continue
    exit 1
}

# A failure that produces no output at all is undiagnosable, and this check failed exactly
# that way on the Intel macOS runner: ctest reported the test as failed while nothing from
# the script reached the log. The markers below are flushed by hand, so a process that dies
# before PowerShell drains its own buffers still leaves a trail showing how far it got.
function Write-Marker {
    param([string]$Message)

    Write-Host "q-os-include-scope: $Message"
    [Console]::Out.Flush()
}

function Resolve-ScannedRoot {
    param([string]$Path, [string]$Label)

    if (-not (Test-Path -LiteralPath $Path)) {
        Stop-CheckWithError "$Label root '$Path' does not exist, so the scan would read nothing. Refusing to report a pass."
    }
    # Get-Item, not Resolve-Path: the returned root is later used as a length prefix for paths
    # Get-ChildItem produced (FullName), which uses the long spelling of every component. Resolve-Path
    # keeps the caller's spelling, and an 8.3 short prefix (C:\Users\RUNNER~1\...) against long
    # children (C:\Users\runneradmin\...) shifts the slice and corrupts the reported relative path.
    return (Get-Item -LiteralPath $Path).FullName
}

# Q_OS_WIN, Q_OS_MACOS, Q_PROCESSOR_ARM_64 and the rest are defined by Qt's own headers -- not by the
# compiler, and not by the platform's CMake config (Qt5::Core contributes only QT_NO_DEBUG). A
# translation unit that tests one of them without reaching a Qt header compiles cleanly, takes the
# #else path, and says nothing. That is exactly how CoreAssetPlatform.cpp came to report Os::Other on
# every platform: every backend falls through to its Windows branch for Other, so Windows kept working
# by accident while a macOS build downloaded and installed the Windows core.
$macroPattern = [regex]'\bQ_(?:OS|PROCESSOR)_[A-Z0-9_]+'
$includePattern = [regex]'^\s*#\s*include\s*(?<bracket>[<"])(?<target>[^>"]+)[>"]'

$sourceRootPath = Resolve-ScannedRoot -Path $SourceRoot -Label "Source"
$testRootPath = Resolve-ScannedRoot -Path $TestRoot -Label "Test"
$roots = @($sourceRootPath, $testRootPath)
$repoRoot = Split-Path -Parent $sourceRootPath
Write-Marker "roots: $($roots -join ', ')"

# Every project header, keyed by bare file name. An include is only treated as a project file when it
# really names one, which is what keeps "<QString>" classified as the Qt header it is.
$headerIndex = @{}
foreach ($root in $roots) {
    $headers = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -eq ".h" -or $_.Extension -eq ".hpp" }
    foreach ($header in $headers) {
        if (-not $headerIndex.ContainsKey($header.Name)) {
            $headerIndex[$header.Name] = New-Object System.Collections.Generic.List[string]
        }
        $headerIndex[$header.Name].Add($header.FullName)
    }
}

if ($headerIndex.Count -eq 0) {
    Stop-CheckWithError "No headers found under '$SourceRoot' or '$TestRoot'; the include resolution would be vacuous. Refusing to report a pass."
}
Write-Marker "indexed $($headerIndex.Count) header name(s)"

# A Qt header is recognised by its name, not by being missing from the project: "<QtCore/qstring.h>"
# and "<QString>" are the same header spelled two ways, and both must count.
function Test-LooksLikeQtHeader {
    param([string]$Target)

    foreach ($segment in $Target.Replace('\', '/').Split('/')) {
        if ($segment -match '^(?:Qt[A-Z]|Q[A-Z])') {
            return $true
        }
    }
    return $false
}

# The walk below re-reads every file once per Q_OS_* user, so the same header is read nineteen
# times over, and re-resolves every include against the filesystem on each of those passes.
# That is tens of thousands of stat calls for one check, and on the Intel macOS runner -- where
# the whole Test step ran roughly seven times slower than on arm64 -- the check died four times
# out of five with ctest reporting the test as failed and not a byte of output. Both caches
# collapse that to a single pass over each distinct file, which is not a fix for whatever killed
# it but removes the only part of this script whose cost is proportional to the runner's speed.
$fileLinesCache = @{}
$includeTargetsCache = @{}

function Get-FileLines {
    param([string]$Path)

    if (-not $fileLinesCache.ContainsKey($Path)) {
        $fileLinesCache[$Path] = [System.IO.File]::ReadAllLines($Path)
    }
    return $fileLinesCache[$Path]
}

# Resolve an include to the project files it could name. Empty means a system or Qt header.
function Get-IncludeTargets {
    param([string]$Target, [string]$IncludingFile, [hashtable]$HeaderIndex)

    $cacheKey = "$IncludingFile`0$Target"
    if ($includeTargetsCache.ContainsKey($cacheKey)) {
        return $includeTargetsCache[$cacheKey]
    }

    $resolved = New-Object System.Collections.Generic.List[string]
    $normalized = $Target.Replace('\', '/')

    # A quoted include first tries the including file's own directory, the way the compiler does.
    $candidate = Join-Path (Split-Path -Parent $IncludingFile) $normalized
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $resolved.Add((Resolve-Path -LiteralPath $candidate).Path)
    }

    # Then the include roots, which is how every project header is named from outside its own folder.
    foreach ($root in $roots) {
        $candidate = Join-Path $root $normalized
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $resolved.Add((Resolve-Path -LiteralPath $candidate).Path)
        }
    }

    # Finally a bare file name. Every candidate is followed, not just the first, so a duplicated
    # header name cannot hide the Qt include that lives behind one of the copies.
    $leaf = Split-Path -Leaf $normalized
    if ($HeaderIndex.ContainsKey($leaf)) {
        foreach ($path in $HeaderIndex[$leaf]) {
            $resolved.Add($path)
        }
    }

    $result = @($resolved | Select-Object -Unique)
    $includeTargetsCache[$cacheKey] = $result
    return $result
}

# Walk the include closure of one file and report the Qt headers reachable from it.
function Get-QtHeaderHits {
    param([string]$EntryFile, [hashtable]$HeaderIndex)

    $visited = New-Object System.Collections.Generic.HashSet[string]
    $hits = New-Object System.Collections.Generic.List[string]
    $pending = New-Object System.Collections.Generic.Stack[string]
    $pending.Push($EntryFile)

    while ($pending.Count -gt 0) {
        $current = $pending.Pop()
        if (-not $visited.Add($current)) {
            continue
        }

        $lines = Get-FileLines -Path $current
        foreach ($line in $lines) {
            $match = $includePattern.Match($line)
            if (-not $match.Success) {
                continue
            }

            $target = $match.Groups["target"].Value
            $resolvedTargets = Get-IncludeTargets -Target $target -IncludingFile $current -HeaderIndex $HeaderIndex
            if ($resolvedTargets.Count -gt 0) {
                foreach ($resolvedTarget in $resolvedTargets) {
                    $pending.Push($resolvedTarget)
                }
                continue
            }

            if (Test-LooksLikeQtHeader -Target $target) {
                $hits.Add("$current -> <$target>")
            }
        }
    }

    return @{ Hits = $hits; Closure = $visited.Count }
}

$users = New-Object System.Collections.Generic.List[string]
foreach ($root in $roots) {
    $files = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Extension -eq ".h" -or $_.Extension -eq ".hpp" -or
            $_.Extension -eq ".cpp" -or $_.Extension -eq ".cc" -or $_.Extension -eq ".cxx"
        }
    foreach ($file in $files) {
        if ($macroPattern.IsMatch([System.IO.File]::ReadAllText($file.FullName))) {
            $users.Add($file.FullName)
        }
    }
}

# A scan that finds no Q_OS_* user at all is far more likely to be looking in the wrong place than to
# be a genuine all-clear, and it would pass silently either way.
if ($users.Count -eq 0) {
    Stop-CheckWithError "No file under '$SourceRoot' or '$TestRoot' mentions Q_OS_* or Q_PROCESSOR_*; the scan is stale or the roots are wrong. Refusing to report a pass."
}
Write-Marker "walking the include closure of $($users.Count) file(s)"

$violations = New-Object System.Collections.Generic.List[string]
$reported = New-Object System.Collections.Generic.List[string]
foreach ($user in $users) {
    $relative = $user.Substring($repoRoot.Length + 1).Replace('\', '/')
    $result = Get-QtHeaderHits -EntryFile $user -HeaderIndex $headerIndex
    if ($result.Hits.Count -eq 0) {
        $violations.Add(
            "$relative tests Q_OS_*/Q_PROCESSOR_* but reaches no Qt header in its include closure " +
            "($($result.Closure) file(s) walked), so every branch it guards is compiled out. " +
            "Include <QtGlobal> (or any Qt header) from the file or from a header it includes.")
    } else {
        $reported.Add("$relative (closure $($result.Closure), first Qt header via $($result.Hits[0]))")
    }
}
Write-Marker "closures walked; $($violations.Count) violation(s)"

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host ("Q_OS_* include-scope check passed: $($users.Count) file(s) mention Q_OS_*/Q_PROCESSOR_* " +
            "and every one of them reaches a Qt header; $($headerIndex.Count) header name(s) indexed.")
foreach ($line in $reported) {
    Write-Host "  ok  $line"
}
