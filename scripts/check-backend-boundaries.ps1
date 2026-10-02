param(
    [string]$SourceRoot = "src"
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

# The include scans below were originally hard-wired to ripgrep. ripgrep is a
# scoop/CI convenience tool, not a project dependency, so when it is absent the
# check silently turned into a permanent red test that masked real regressions.
# Resolve it if we can, otherwise fall back to a pure-PowerShell scan that
# implements the same three rules.
# A candidate is only usable if it actually runs. Scoop shims in particular can
# be present on disk yet fail to resolve their target, which would otherwise
# turn the scan into a silent no-op that reports "passed".
function Test-RgUsable {
    param([string]$Executable)

    try {
        $output = & $Executable --version 2>$null
        return ($LASTEXITCODE -eq 0) -and ($output -match "ripgrep")
    } catch {
        return $false
    }
}

function Resolve-RgExecutable {
    $candidates = @()
    $command = Get-Command rg -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }
    if ($env:USERPROFILE) {
        $candidates += @(
            (Join-Path $env:USERPROFILE "scoop/apps/ripgrep/current/rg.exe"),
            (Join-Path $env:USERPROFILE "scoop/shims/rg.exe")
        )
    }

    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate)) {
            continue
        }
        if (Test-RgUsable $candidate) {
            return $candidate
        }
    }
    return $null
}

$rgExecutable = Resolve-RgExecutable

# A scan over a directory that moved or was renamed reports no violations, which is
# indistinguishable from a clean result. Count what was actually read so the pass can be
# checked, and so the guard at the bottom can refuse to report success on an empty scan.
$script:scannedFileCount = 0
$script:scannedRootCount = 0

# Returns "path:line:content" strings, mirroring ripgrep's default output shape
# closely enough for the violation messages built from them.
function Find-MatchingLines {
    param(
        [string]$Pattern,
        [string[]]$Roots,
        [string[]]$Extensions = @(".h", ".cpp")
    )

    $script:scannedRootCount++

    if ($rgExecutable) {
        $arguments = @("--line-number")
        foreach ($extension in $Extensions) {
            $arguments += @("--glob", "*$extension")
        }
        $arguments += $Pattern
        $arguments += $Roots

        $output = & $rgExecutable @arguments 2>$null
        if ($LASTEXITCODE -gt 1) {
            throw "rg failed with exit code $LASTEXITCODE"
        }
        # rg does not report per-file hits here, so count the roots it was pointed at: a non-empty
        # root list with a successful rg run means the scan really happened. `+= 1` would only count
        # one file per scan call and starve the empty-scan guard at the bottom.
        $script:scannedFileCount += $Roots.Count
        return @($output)
    }

    $results = New-Object System.Collections.Generic.List[string]
    foreach ($root in $Roots) {
        $files = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $Extensions -contains $_.Extension -and
                # Match ripgrep's default: hidden entries (e.g. .git) are skipped.
                -not ($_.FullName -split '[\\/]' | Where-Object { $_.StartsWith(".") })
            }
        $script:scannedFileCount += @($files).Count
        foreach ($file in $files) {
            $hits = Select-String -LiteralPath $file.FullName -Pattern $Pattern -ErrorAction SilentlyContinue
            foreach ($hit in $hits) {
                $results.Add("$($file.FullName):$($hit.LineNumber):$($hit.Line)")
            }
        }
    }
    return @($results)
}

$root = Resolve-Path $SourceRoot
$violations = New-Object System.Collections.Generic.List[string]

$legacyBackendDirs = @(
    (Join-Path $root "runtime/core/xray"),
    (Join-Path $root "runtime/core/singbox")
)
foreach ($dir in $legacyBackendDirs) {
    if (Test-Path $dir) {
        $violations.Add("Legacy concrete backend directory must stay out of runtime: $dir")
    }
}

# Every layer that consumes a core but must not know which one. `auto` is here for the same reason
# `app` is: it is a front end that links a core through the runtime layer. It used to be missing,
# which meant the rule below silently did not apply to it -- see the coverage guard underneath.
$consumerDirs = @(
    (Join-Path $root "app"),
    (Join-Path $root "appcore"),
    (Join-Path $root "auto"),
    (Join-Path $root "ui"),
    (Join-Path $root "services"),
    (Join-Path $root "runtime")
)
$existingConsumerDirs = @($consumerDirs | Where-Object { Test-Path $_ })
if ($existingConsumerDirs.Count -eq 0) {
    Stop-CheckWithError "None of the consumer directories ($($consumerDirs -join ', ')) exist under '$SourceRoot'; the include rule would scan nothing. Refusing to report a pass."
}

# A directory with a main.cpp is a front-end executable, and every front end has to obey the rule.
# Without this guard a new front end escapes it by simply not being listed above -- which is exactly
# how `auto` came to include a concrete backend unnoticed.
$frontEndDirs = @(
    Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "main.cpp") }
)
if ($frontEndDirs.Count -eq 0) {
    Stop-CheckWithError "No directory under '$SourceRoot' contains a main.cpp; the front-end coverage guard would pass vacuously. Refusing to report a pass."
}
foreach ($frontEndDir in $frontEndDirs) {
    if ($existingConsumerDirs -notcontains $frontEndDir.FullName) {
        $violations.Add(
            "Front-end directory '$($frontEndDir.FullName)' is not covered by the concrete-backend " +
            "include rule; add it to the consumer list in this script.")
    }
}
$matches = Find-MatchingLines -Pattern '#include\s+"backends/' -Roots $existingConsumerDirs
foreach ($match in $matches) {
    $violations.Add("Concrete backend include from common/application layer: $match")
}

# A front end is a top of the dependency order: nothing below it may include it. Without this rule
# `appcore/` reached up into `app/` for three headers, so the shared layer depended on one
# executable's code and every `appcore/` consumer inherited that dependency transitively -- the
# reason `auto/`'s "no dependency on app/" claim only held for direct includes.
$frontEndPaths = @($frontEndDirs | ForEach-Object { $_.FullName })
$nonFrontEndDirs = @(
    Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
        Where-Object { $frontEndPaths -notcontains $_.FullName }
)
if ($nonFrontEndDirs.Count -eq 0) {
    Stop-CheckWithError "No non-front-end directory exists under '$SourceRoot'; the upward-include rule would scan nothing. Refusing to report a pass."
}
foreach ($frontEndPath in $frontEndPaths) {
    $frontEndName = [regex]::Escape((Split-Path -Leaf $frontEndPath))
    $matches = Find-MatchingLines -Pattern "#include\s+`"$frontEndName/" -Roots @($nonFrontEndDirs.FullName)
    foreach ($match in $matches) {
        $violations.Add(
            "Front-end layer included from below; it is a top of the dependency order: $match")
    }
}

$backendDir = Join-Path $root "backends"
if (-not (Test-Path $backendDir)) {
    Stop-CheckWithError "Backend directory '$backendDir' does not exist, so the reverse-direction rule would scan nothing. Refusing to report a pass."
}
$matches = Find-MatchingLines -Pattern '#include\s+"(app|appcore|ui|services|platform)/' -Roots @($backendDir)
foreach ($match in $matches) {
    $violations.Add("Backend may not include application/service/UI/platform layer: $match")
}

if ($scannedFileCount -eq 0) {
    Stop-CheckWithError "The check read no source files at all; the roots are stale or missing. Refusing to report a pass."
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host ("Backend boundary check passed: $scannedRootCount scan(s) over " +
            "$scannedFileCount scan target(s); legacy backend directories absent; " +
            "0 concrete-backend include(s) from the app layer; 0 reverse-direction include(s); " +
            "0 upward include(s) into a front end; " +
            "$($frontEndDirs.Count) front-end directory/directories covered by the include rule.")
