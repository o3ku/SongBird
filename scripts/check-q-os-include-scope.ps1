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

    Write-Error $Message -ErrorAction Continue
    exit 1
}

function Resolve-ScannedRoot {
    param([string]$Path, [string]$Label)

    if (-not (Test-Path -LiteralPath $Path)) {
        Stop-CheckWithError "$Label root '$Path' does not exist, so the scan would read nothing. Refusing to report a pass."
    }
    return (Resolve-Path -LiteralPath $Path).Path
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

# Resolve an include to the project files it could name. Empty means a system or Qt header.
function Get-IncludeTargets {
    param([string]$Target, [string]$IncludingFile, [hashtable]$HeaderIndex)

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

    return @($resolved | Select-Object -Unique)
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

        $lines = [System.IO.File]::ReadAllLines($current)
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

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host ("Q_OS_* include-scope check passed: $($users.Count) file(s) mention Q_OS_*/Q_PROCESSOR_* " +
            "and every one of them reaches a Qt header; $($headerIndex.Count) header name(s) indexed.")
foreach ($line in $reported) {
    Write-Host "  ok  $line"
}
