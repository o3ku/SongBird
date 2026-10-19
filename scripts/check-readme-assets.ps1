# Verifies that every image the READMEs reference actually exists in the repository.
#
# Why this exists: a screenshot in a README is the first thing a visitor looks at, and it is also the
# thing that rots silently. Moving or renaming docs/screenshots/ leaves a broken image icon on the
# project's front page, and nothing in the build notices because the README is not compiled.
#
# Every README*.md is scanned, not just README.md: the translated pages share the same screenshots and
# a translation added later would otherwise have no protection at all.
#
# What this does NOT check: whether a screenshot still shows the current UI, or whether a translation
# is still in sync with the English original. Neither can be automated -- both need a human -- so the
# rule is recorded in AGENTS.md instead of being pretended to be automated here.
#
# Usage:
#   pwsh -File scripts/check-readme-assets.ps1 [-Root <repository root>]
[CmdletBinding()]
param(
    [string]$Root = (Join-Path $PSScriptRoot "..")
)

$ErrorActionPreference = 'Stop'

function Stop-CheckWithError {
    param([string]$Message)
    Write-Error $Message -ErrorAction Continue
    exit 1
}

if (-not (Test-Path -LiteralPath $Root)) {
    Stop-CheckWithError "Repository root not found at '$Root'; refusing to report a pass on a tree it did not read."
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$readmes = @(Get-ChildItem -LiteralPath $rootPath -Filter "README*.md" -File -ErrorAction SilentlyContinue |
    Sort-Object Name)
if ($readmes.Count -eq 0) {
    Stop-CheckWithError "No README*.md found under '$rootPath'. A tree with no README must not report a pass."
}

# Markdown image syntax: ![alt](path). The alt text is allowed to be empty.
$imagePattern = [regex]'!\[[^\]]*\]\(([^)\s]+)'

$violations = New-Object System.Collections.Generic.List[string]
$checked = 0

foreach ($readme in $readmes) {
    $content = Get-Content -LiteralPath $readme.FullName -Raw

    foreach ($match in $imagePattern.Matches($content)) {
        $target = $match.Groups[1].Value

        # Skip anything that is not a repository-relative path: an absolute URL is someone else's
        # asset and cannot be checked from here.
        if ($target -match '^[a-zA-Z][a-zA-Z0-9+.-]*://') {
            continue
        }

        $checked += 1
        # Images resolve relative to the file that references them, which for a top-level README is
        # the repository root.
        $resolved = Join-Path $readme.DirectoryName $target
        if (-not (Test-Path -LiteralPath $resolved)) {
            $violations.Add(
                "$($readme.Name) references '$target', which does not exist. A missing image renders " +
                "as a broken icon on the project's front page.")
            continue
        }

        $item = Get-Item -LiteralPath $resolved
        if ($item.Length -le 0) {
            $violations.Add("$($readme.Name) references '$target', which exists but is empty.")
        }
    }
}

if ($checked -eq 0) {
    # A README with no local images is legitimate, but a regex that stopped matching is not. Report
    # which case this is rather than passing silently.
    Write-Host ("README asset check: no local image references found across $($readmes.Count) " +
        "README file(s); nothing to verify.")
    exit 0
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host ("README asset check passed: $checked local image reference(s) across " +
    "$($readmes.Count) README file(s) all resolve to a non-empty file.")
