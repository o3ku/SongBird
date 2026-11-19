param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# Downloads go through three intentional GitHub mirrors (ghfast.top, mirror.ghproxy.com,
# ghproxy.net) for regions where github.com is unreachable, and the checksum that vouches
# for the bytes is anchored to the official GitHub channel instead (see ReleaseChecksum.h).
# A new mirror host must therefore be a deliberate decision, not a string literal nobody
# reads twice: every host that appears in an `http(s)://` URL in a C++ source under src/
# must be on the allowlist below. A host the list does not name fails the build, and
# adding one means editing this file (and its Python twin) in the open, not slipping a
# prefix into GitHubMirrorHelper.h.
#
# The rule: each URL host must be a GitHub origin, one of the three intentional mirrors,
# or an allowlisted service host (connectivity checks, exit-location lookups, the plist
# DTD, loopback and the DNS defaults). Comments are stripped first -- a host in a comment
# is history being explained, not a download being routed, and GitHubMirrorHelper.h itself
# documents the mirrors that way.

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

# Hosts a URL in src/ may name. GitHub origins are covered by suffix below; the mirrors
# are the three intentional ones (see GitHubMirrorHelper.h); the rest are the services
# the code already talks to, grouped by purpose so a new entry has to say why it belongs.
$allowedExactHosts = @(
    "github.com",
    # Intentional GitHub mirrors.
    "ghfast.top",
    "mirror.ghproxy.com",
    "ghproxy.net",
    # Connectivity checks (OutboundLivenessProbeService, ProxyAvailabilityCheckService).
    "www.gstatic.com",
    "connectivitycheck.gstatic.com",
    "cp.cloudflare.com",
    "detectportal.firefox.com",
    "www.msftconnecttest.com",
    "www.google.com",
    # Exit-location lookups (OutboundLocationProbeService).
    "ifconfig.co",
    "ip-api.com",
    "ipapi.co",
    "ipinfo.io",
    "ipwho.is",
    # macOS login-item helper (MacAutoRunService).
    "www.apple.com",
    # Loopback and the DNS defaults (Config.h, ProxyGroupStatusPoller).
    "127.0.0.1",
    "localhost",
    "1.1.1.1",
    "223.5.5.5"
)

# Any subdomain of a GitHub origin (api.github.com, raw.githubusercontent.com, ...).
$allowedHostSuffixes = @(
    ".github.com",
    ".githubusercontent.com"
)

function Test-AllowedHost {
    # Named $HostName, not $Host: $Host is a read-only automatic variable and
    # assigning to it aborts the guard ("Cannot overwrite variable Host").
    param([string]$HostName)

    $lower = $HostName.ToLowerInvariant()
    if ($allowedExactHosts -contains $lower) {
        return $true
    }
    foreach ($suffix in $allowedHostSuffixes) {
        if ($lower.EndsWith($suffix, [System.StringComparison]::Ordinal)) {
            return $true
        }
    }
    return $false
}

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

    foreach ($match in [regex]::Matches($text, 'https?://([A-Za-z0-9._-]+)')) {
        $urlHost = $match.Groups[1].Value
        if (-not (Test-AllowedHost -HostName $urlHost)) {
            $line = ($text.Substring(0, $match.Groups[1].Index) -split "`n").Count
            $violations.Add("URL host '$urlHost' is not on the mirror allowlist in " + $relative + ":" + $line)
        }
    }
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host (("Mirror allowlist check passed: {0} C++ file(s) scanned, every URL host is a GitHub origin, an intentional mirror, or an allowlisted service host.") -f $files.Count)
