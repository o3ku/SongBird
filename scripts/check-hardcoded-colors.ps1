param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# This repository already keeps its colours in one place by convention: ui/theme/AppThemePalette.cpp
# holds every value the main window uses, and nothing else under src/ui writes a raw hex. A
# convention, though, has no way to notice when it stops being true, and the failure is invisible:
# nobody files a bug for "one scrollbar handle is #c5ced8 in the light theme only".
#
# A runtime probe cannot replace this. It can only inspect widgets that were actually constructed
# in that run, so a colour inside a stylesheet branch nobody took, or a widget behind a feature
# flag, is exactly the case it cannot see -- and exactly the case that drifts.
#
# The rule: a hex colour literal may appear only in a palette source file. Everything else
# interpolates a palette field.
#
# Comments are stripped first. A hex in a comment is explanation, not code, and counting it would
# train people to stop explaining colours. String literals are what get scanned, because that is
# where a stylesheet colour lives.

function Stop-CheckWithError {
    param([string]$Message)

    Write-Error $Message -ErrorAction Continue
    exit 1
}

# Canonicalize through Get-Item, not Resolve-Path: Resolve-Path keeps the caller's spelling (an
# 8.3 short name such as C:\Users\RUNNER~1\...), while Get-ChildItem below reports children by the
# long spelling (C:\Users\runneradmin\...). The substring that rebuilds the repo-relative path
# would then slice into the tail of `...\clean\src` and double the `src/` prefix -- exactly what
# guard-equivalence caught on the GitHub Windows runner. Get-Item's FullName uses the same
# spelling as Get-ChildItem's, so the slice lands on the separator as intended.
$sourcePath = (Get-Item -LiteralPath $SourceRoot).FullName

# The only files allowed to name a colour. Kept as a small list rather than a naming rule: a rule
# like "*Palette*" would let a new file opt out by being named helpfully.
$allowedFiles = @(
    "src/ui/theme/AppThemePalette.cpp",
    "src/auto/AutoWindowPalette.cpp"
)

# Strips // and /* */ comments, replacing them with whitespace of the same length so the reported
# line numbers match the file the reader opens.
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

$files = @(Get-ChildItem -LiteralPath $sourcePath -Recurse -File -Include *.cpp, *.h -ErrorAction SilentlyContinue)
if ($files.Count -eq 0) {
    Stop-CheckWithError "No C++ sources under '$SourceRoot'; refusing to report a pass on an empty scan."
}

$violations = New-Object System.Collections.Generic.List[string]
$scannedLiterals = 0

foreach ($file in $files) {
    $fullPath = $file.FullName
    $relative = "src/" + ($fullPath.Substring($sourcePath.Length).TrimStart('\', '/') -replace '\\', '/')
    if ($allowedFiles -contains $relative) {
        continue
    }

    $text = Remove-CppComments -Text (Get-Content -LiteralPath $fullPath -Raw)

    # Only hex inside a string literal counts. Matching the whole file would flag a variable named
    # `hexColor` and would also miss the real question, which is "what does this string end up
    # being drawn with".
    foreach ($literal in [regex]::Matches($text, '"((?:[^"\\\n]|\\.)*)"')) {
        $body = $literal.Groups[1].Value
        foreach ($hex in [regex]::Matches($body, '#[0-9a-fA-F]{6}\b')) {
            $scannedLiterals += 1
            $line = ($text.Substring(0, $literal.Index) -split "`n").Count
            $violations.Add($relative + ":" + $line + "  " + $hex.Value)
        }
    }
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- hardcoded colour: $_" }) -join [Environment]::NewLine)
}

Write-Host (("Colour check passed: {0} C++ file(s) scanned, no hex colour literal outside " +
    "{1} palette file(s).") -f $files.Count, $allowedFiles.Count)
