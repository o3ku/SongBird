# Verifies that every CoreFoundation symbol the macOS sources use is backed by a link.
#
# Why this exists: the macOS secret store uses CoreFoundation types and calls
# (CFDictionaryCreateMutable, CFDataCreate, CFRelease), which live in CoreFoundation, not
# Security. The macOS sources compile with only "-framework Security" linked, so the missing
# framework is invisible until the link step -- and on a Linux machine the APPLE block never
# runs at all, so nothing there could catch it. The link then fails with undefined symbols
# (_CFDataCreate, _CFDictionaryCreateMutable) on both architectures, which is exactly what
# happened after PR #6: the GCM private-SPI fix compiled, and the link broke instead.
#
# Usage:
#   check-macos-frameworks.ps1
#
# The paths default to the working tree, so a developer runs it with no arguments. The
# self-test points them at a throwaway copy instead.
[CmdletBinding()]
param(
    [string]$SrcCMakeListsPath = (Join-Path $PSScriptRoot ".." "src" "CMakeLists.txt"),
    [string]$MacOSSourceRoot = (Join-Path $PSScriptRoot ".." "src" "platform" "macos")
)

$ErrorActionPreference = 'Stop'

$violations = New-Object System.Collections.Generic.List[string]

function Stop-CheckWithError {
    param([string]$Message)
    Write-Error $Message -ErrorAction Continue
    exit 1
}

if (-not (Test-Path -LiteralPath $SrcCMakeListsPath)) {
    Stop-CheckWithError "src/CMakeLists.txt not found at '$SrcCMakeListsPath'; refusing to report a pass."
}
if (-not (Test-Path -LiteralPath $MacOSSourceRoot -PathType Container)) {
    Stop-CheckWithError "macOS platform sources not found at '$MacOSSourceRoot'; refusing to report a pass."
}

# Strips // and /* */ comments, replacing them with whitespace of the same length so the reported
# line numbers match the file the reader opens. Same contract as Remove-CppComments in
# check-hardcoded-colors.ps1.
function Remove-CppComments {
    param([string]$Text)

    $builder = New-Object System.Text.StringBuilder $Text.Length
    $index = 0
    $length = $Text.Length
    while ($index -lt $length) {
        $character = $Text[$index]
        if ($character -eq '/' -and $index + 1 -lt $length -and $Text[$index + 1] -eq '/') {
            while ($index -lt $length -and $Text[$index] -ne "`n") {
                [void]$builder.Append(' ')
                $index += 1
            }
            continue
        }
        if ($character -eq '/' -and $index + 1 -lt $length -and $Text[$index + 1] -eq '*') {
            while ($index -lt $length) {
                if ($Text[$index] -eq '*' -and $index + 1 -lt $length -and $Text[$index + 1] -eq '/') {
                    [void]$builder.Append('  ')
                    $index += 2
                    break
                }
                if ($Text[$index] -eq "`n") {
                    [void]$builder.Append("`n")
                } else {
                    [void]$builder.Append(' ')
                }
                $index += 1
            }
            continue
        }
        [void]$builder.Append($character)
        $index += 1
    }
    return $builder.ToString()
}

# The repo root is the parent of src/, so a violation path reads the same from a throwaway
# copy as from the working tree.
$repoRoot = (Get-Item -LiteralPath (Join-Path (Split-Path -Parent $SrcCMakeListsPath) "..")).FullName

$cmakeText = Get-Content -LiteralPath $SrcCMakeListsPath -Raw
# There are several if(APPLE) blocks (source lists, the bundle, the frameworks), so every
# one is scanned: the link counts wherever it sits, as long as it is Apple-only.
$appleMatches = [regex]::Matches($cmakeText, 'if\s*\(\s*APPLE\s*\)(.*?)endif\s*\(\s*\)',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if ($appleMatches.Count -eq 0) {
    Stop-CheckWithError "src/CMakeLists.txt has no if(APPLE) block; refusing to report a pass."
}
$coreFoundationLinked = $false
foreach ($appleMatch in $appleMatches) {
    if ($appleMatch.Groups[1].Value -match '-framework\s+CoreFoundation\b') {
        $coreFoundationLinked = $true
    }
}

# Any CoreFoundation identifier: CF-prefixed calls and types, and the kCF-prefixed constants
# (kCFBooleanTrue, kCFTypeDictionaryKeyCallBacks). The Security constants the same files use
# (kSecClass, kSecAttrService) do not match either arm.
$cfPattern = '\b(?:CF|kCF)[A-Z][A-Za-z0-9]*\b'
$symbols = @{}
$filesScanned = 0
$macosFiles = @(Get-ChildItem -LiteralPath $MacOSSourceRoot -File -Include *.cpp, *.h -ErrorAction SilentlyContinue |
    Sort-Object FullName)
foreach ($file in $macosFiles) {
    $filesScanned += 1
    $text = Remove-CppComments -Text (Get-Content -LiteralPath $file.FullName -Raw)
    foreach ($match in [regex]::Matches($text, $cfPattern)) {
        $symbol = $match.Value
        if ($symbols.ContainsKey($symbol)) { continue }
        $line = ($text.Substring(0, $match.Index) -split "`n").Count
        $relative = $file.FullName.Substring($repoRoot.Length).TrimStart('\', '/') -replace '\\', '/'
        $symbols[$symbol] = "${relative}:${line}"
    }
}

if ($symbols.Count -eq 0) {
    Stop-CheckWithError ("No CoreFoundation symbol found under src/platform/macos/; the scan pattern " +
        "may be stale. Either the macOS sources stopped using CoreFoundation (then remove this " +
        "guard and the link) or the pattern no longer matches what they use.")
}

if (-not $coreFoundationLinked) {
    foreach ($symbol in ($symbols.Keys | Sort-Object { $symbols[$_] })) {
        $violations.Add(
            "$($symbols[$symbol]): $symbol is used here, but src/CMakeLists.txt does not link " +
            '"-framework CoreFoundation" in any if(APPLE) block; the macOS link then fails ' +
            "with undefined symbols.")
    }
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

# Positive evidence the check actually read something, so a green run cannot be confused with a run
# that scanned nothing.
Write-Host ("macOS frameworks: $($symbols.Count) CoreFoundation symbol(s) in $filesScanned " +
    'file(s) under src/platform/macos/, "-framework CoreFoundation" linked in ' +
    "src/CMakeLists.txt.")
