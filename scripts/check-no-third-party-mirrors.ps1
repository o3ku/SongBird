param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# Downloads used to be routed through third-party GitHub mirrors (ghfast.top,
# mirror.ghproxy.com, ghproxy.net) prepended ahead of github.com, so a download GitHub served
# slowly could still finish. A mirror is a man in the middle: it serves whatever bytes it
# chooses, and a checksum published beside the file on GitHub cannot vouch for bytes the
# mirror substituted. The mirrors were removed and downloads go to github.com only; this
# check makes sure one never creeps back in, in a string literal nobody reads twice.
#
# The rule: none of the three hosts may appear in a C++ source under src/. Comments are
# stripped first -- a host in a comment is history being explained, not a download being
# routed, and GitHubMirrorHelper.h itself names all three that way.

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

# The mirrors that used to be prepended to download URLs. Kept as a small list rather than a
# naming rule: a rule like "*proxy*" would let a new host opt out by being named helpfully.
$forbiddenHosts = @(
    "ghfast.top",
    "mirror.ghproxy.com",
    "ghproxy.net"
)

# Strips // and /* */ comments, replacing them with whitespace of the same length so the reported
# line numbers match the file the reader opens. String-aware: the shared Remove-CppComments in
# check-hardcoded-colors.ps1 does not know about string literals, so the `//` in
# "https://ghfast.top/..." would start a comment and blank out the host -- the thing this guard
# exists to catch. A `//` inside a string, character or raw-string literal stays code, and a real
# comment is still stripped (a host named in a comment is history being explained, not a download
# being routed, and GitHubMirrorHelper.h itself names all three that way).
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
        if ($character -ceq 'R' -and $index + 1 -lt $length -and $Text[$index + 1] -eq '"') {
            # Raw string R"delim( ... )delim": the content may hold quotes and // sequences.
            # The delimiter is at most 16 characters with no space, quote or backslash, and the
            # opening paren must come before the line ends; anything else is not a raw string,
            # and the quote is left for the string branch below.
            $delimiterEnd = $index + 2
            while ($delimiterEnd -lt $length -and $Text[$delimiterEnd] -notin @(' ', '(', '\', "`n", '"') -and ($delimiterEnd - ($index + 2)) -le 16) {
                $delimiterEnd += 1
            }
            if ($delimiterEnd -lt $length -and $Text[$delimiterEnd] -eq '(') {
                $delimiter = $Text.Substring($index + 2, $delimiterEnd - ($index + 2))
                $terminator = ")" + $delimiter + '"'
                $end = $Text.IndexOf($terminator, $delimiterEnd + 1, [System.StringComparison]::Ordinal)
                if ($end -ge 0) {
                    [void]$builder.Append($Text.Substring($index, ($end + $terminator.Length) - $index))
                    $index = $end + $terminator.Length
                } else {
                    [void]$builder.Append($Text.Substring($index))
                    $index = $length
                }
                continue
            }
        }
        if ($character -eq '"' -or $character -eq "'") {
            [void]$builder.Append($character)
            $index += 1
            while ($index -lt $length) {
                if ($Text[$index] -eq '\') {
                    [void]$builder.Append($Text.Substring($index, [Math]::Min(2, $length - $index)))
                    $index += 2
                    continue
                }
                [void]$builder.Append($Text[$index])
                if ($Text[$index] -eq $character) {
                    $index += 1
                    break
                }
                if ($Text[$index] -eq "`n" -and $character -eq "'") {
                    break
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

foreach ($file in $files) {
    $fullPath = $file.FullName
    $relative = "src/" + ($fullPath.Substring($sourcePath.Length).TrimStart('\', '/') -replace '\\', '/')

    $text = Remove-CppComments -Text (Get-Content -LiteralPath $fullPath -Raw)

    foreach ($host in $forbiddenHosts) {
        $searchFrom = 0
        while (($found = $text.IndexOf($host, $searchFrom, [System.StringComparison]::Ordinal)) -ge 0) {
            $line = ($text.Substring(0, $found) -split "`n").Count
            $violations.Add("third-party GitHub mirror host '$host' in " + $relative + ":" + $line)
            $searchFrom = $found + $host.Length
        }
    }
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host (("Mirror check passed: {0} C++ file(s) scanned, no third-party GitHub mirror host.") -f $files.Count)
