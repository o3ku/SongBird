param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

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

# Returns "path:line:content" strings, mirroring ripgrep's default output shape
# closely enough for the violation messages built from them.
function Find-MatchingLines {
    param(
        [string]$Pattern,
        [string[]]$Roots,
        [string[]]$Extensions = @(".h", ".cpp")
    )

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

$consumerDirs = @(
    (Join-Path $root "app"),
    (Join-Path $root "ui"),
    (Join-Path $root "services"),
    (Join-Path $root "runtime")
)
$existingConsumerDirs = @($consumerDirs | Where-Object { Test-Path $_ })
if ($existingConsumerDirs.Count -gt 0) {
    $matches = Find-MatchingLines -Pattern '#include\s+"backends/' -Roots $existingConsumerDirs
    foreach ($match in $matches) {
        $violations.Add("Concrete backend include from common/application layer: $match")
    }
}

$backendDir = Join-Path $root "backends"
if (Test-Path $backendDir) {
    $matches = Find-MatchingLines -Pattern '#include\s+"(app|ui|services|platform)/' -Roots @($backendDir)
    foreach ($match in $matches) {
        $violations.Add("Backend may not include application/service/UI/platform layer: $match")
    }
}

if ($violations.Count -gt 0) {
    Write-Error (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
    exit 1
}

Write-Host "Backend boundary check passed."
