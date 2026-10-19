# Verifies that every image the README references actually exists in the repository.
#
# Why this exists: a screenshot in a README is the first thing a visitor looks at, and it is also the
# thing that rots silently. Moving or renaming docs/screenshots/ leaves a broken image icon on the
# project's front page, and nothing in the build notices because the README is not compiled.
#
# What this does NOT check: whether a screenshot still shows the current UI. That cannot be
# automated -- it needs a human to compare the image against the running application -- so the rule
# is recorded here instead: when a change alters the main window's layout or the settings dialog's
# tabs, the screenshots in docs/screenshots/ are part of that change.
#
# Usage:
#   pwsh -File scripts/check-readme-assets.ps1 [-Readme README.md]
[CmdletBinding()]
param(
    [string]$Readme = (Join-Path $PSScriptRoot ".." "README.md")
)

$ErrorActionPreference = 'Stop'

function Stop-CheckWithError {
    param([string]$Message)
    Write-Error $Message -ErrorAction Continue
    exit 1
}

if (-not (Test-Path -LiteralPath $Readme)) {
    Stop-CheckWithError "README not found at '$Readme'; refusing to report a pass on a file it did not read."
}

$readmePath = (Resolve-Path -LiteralPath $Readme).Path
$readmeRoot = Split-Path -Parent $readmePath
$content = Get-Content -LiteralPath $readmePath -Raw

# Markdown image syntax: ![alt](path). The alt text is allowed to be empty.
$imagePattern = [regex]'!\[[^\]]*\]\(([^)\s]+)'

$violations = New-Object System.Collections.Generic.List[string]
$checked = 0

foreach ($match in $imagePattern.Matches($content)) {
    $target = $match.Groups[1].Value

    # Skip anything that is not a repository-relative path: an absolute URL is someone else's asset
    # and cannot be checked from here.
    if ($target -match '^[a-zA-Z][a-zA-Z0-9+.-]*://') {
        continue
    }

    $checked += 1
    $resolved = Join-Path $readmeRoot $target
    if (-not (Test-Path -LiteralPath $resolved)) {
        $violations.Add(
            "README references '$target', which does not exist. A missing image renders as a broken " +
            "icon on the project's front page.")
        continue
    }

    $item = Get-Item -LiteralPath $resolved
    if ($item.Length -le 0) {
        $violations.Add("README references '$target', which exists but is empty.")
    }
}

if ($checked -eq 0) {
    # A README with no local images is legitimate, but a regex that stopped matching is not. Report
    # which case this is rather than passing silently.
    Write-Host "README asset check: no local image references found in '$Readme'; nothing to verify."
    exit 0
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host "README asset check passed: $checked local image reference(s) all resolve to a non-empty file."
