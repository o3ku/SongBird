param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# Work that runs on a worker thread must not keep a raw pointer to a QObject that the
# GUI thread can destroy while the work is still in flight. The failure does not show
# up where the bug is: the continuation runs later, on the GUI thread, against a freed
# object, and the access violation surfaces in whatever code happens to allocate that
# memory next. A test that was passing yesterday starts crashing, and the stack points
# at the wrong place.
#
# This guard was written after exactly that happened here: six GUI suites went red at
# once on CI, and the cause was thirteen continuations that captured `this` with no
# owner check.
#
# The rule enforced here, stated positively:
#   * A `launchBackgroundThread(...)` lambda that captures `this` MUST also capture a
#     lifetime token and test it (`guard.expired()` / `lifetimeGuard.expired()`) before
#     touching the object. This is the dominant pattern in this codebase -- see
#     ProxySession::launchBackgroundThread call sites.
#
#   * A `QMetaObject::invokeMethod(...)` whose continuation lambda captures `this` is
#     only safe when the receiver is `this` (Qt then drops the call if the object is
#     gone) or when the continuation tests a guard/QPointer. A continuation posted to
#     some other object, capturing a bare `this`, is the dangerous shape.
#
# Text inside comments and string literals is stripped first, so a mention in a comment
# cannot satisfy the check and a `//` inside a URL cannot hide a violation.

$ErrorActionPreference = "Stop"

function Stop-CheckWithError {
    param([string]$Message)

    Write-Error $Message -ErrorAction Continue
    exit 1
}

# Canonicalize through Get-Item, not Resolve-Path: Resolve-Path keeps the caller's spelling (an
# 8.3 short name such as C:\Users\RUNNER~1\...), while Get-ChildItem below reports children by the
# long spelling (C:\Users\runneradmin\...). The substring that rebuilds the repo-relative path
# would then slice into the tail of the source root and corrupt every reported path -- exactly
# what guard-equivalence caught on the GitHub Windows runner. Get-Item's FullName uses the same
# spelling as Get-ChildItem's, so the slice lands on the separator as intended.
$sourcePath = (Get-Item -LiteralPath $SourceRoot).FullName
$files = @(Get-ChildItem -LiteralPath $sourcePath -Recurse -File -Include *.cpp -ErrorAction SilentlyContinue)
if ($files.Count -eq 0) {
    Stop-CheckWithError "No .cpp files under '$SourceRoot'; refusing to report a pass on an empty scan."
}

# Removes // and /* */ comments while preserving byte offsets and line count, so the
# reported line numbers match the real file. Strings are left in place: a capture list
# lives outside them, and blanking them would break the paren matching below.
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

# Returns the text of the call starting at `$start` (the index of the opening paren),
# including the matching close paren.
function Get-CallText {
    param([string]$Text, [int]$Start)

    $depth = 0
    for ($index = $Start; $index -lt $Text.Length; $index += 1) {
        $character = $Text[$index]
        if ($character -eq '(' -or $character -eq '[' -or $character -eq '{') {
            $depth += 1
        } elseif ($character -eq ')' -or $character -eq ']' -or $character -eq '}') {
            $depth -= 1
            if ($depth -eq 0) {
                return $Text.Substring($Start, $index - $Start + 1)
            }
        }
    }
    return $Text.Substring($Start)
}

# The capture list of the first lambda in a call, or an empty string when there is none.
function Get-FirstCaptureList {
    param([string]$Call)

    $match = [regex]::Match($Call, '\[\s*([^\[\]]*)\]')
    if (-not $match.Success) {
        return ""
    }
    return $match.Groups[1].Value
}

# The receiver of a static invokeMethod call: everything before the first top-level comma.
function Get-InvokeReceiver {
    param([string]$Call)

    $open = $Call.IndexOf('(')
    if ($open -lt 0) {
        return ""
    }
    $depth = 0
    $builder = New-Object System.Text.StringBuilder
    for ($index = $open + 1; $index -lt $Call.Length; $index += 1) {
        $character = $Call[$index]
        if ($character -eq '(' -or $character -eq '[' -or $character -eq '{') {
            $depth += 1
        } elseif ($character -eq ')' -or $character -eq ']' -or $character -eq '}') {
            if ($depth -eq 0) {
                break
            }
            $depth -= 1
        } elseif ($character -eq ',' -and $depth -eq 0) {
            break
        }
        [void]$builder.Append($character)
    }
    return $builder.ToString().Trim()
}

$violations = New-Object System.Collections.Generic.List[string]
$scannedCalls = 0

foreach ($file in $files) {
    $text = Remove-CppComments -Text (Get-Content -LiteralPath $file.FullName -Raw)
    $relative = $file.FullName.Substring($sourcePath.Length).TrimStart('\', '/')
    $relative = "src/" + ($relative -replace '\\', '/')

    $lineAt = {
        param([int]$Offset)
        return ($text.Substring(0, $Offset) -split "`n").Count
    }

    # --- launchBackgroundThread -------------------------------------------------
    foreach ($match in [regex]::Matches($text, 'launchBackgroundThread\s*\(')) {
        $scannedCalls += 1
        $call = Get-CallText -Text $text -Start ($match.Index + $match.Length - 1)
        $captures = Get-FirstCaptureList -Call $call
        if ($captures -notmatch '\bthis\b') {
            continue
        }
        # The token is only useful if the continuation actually asks it something. A captured
        # but never-tested weak_ptr is a comment, not a guard.
        if ($call -match '\.expired\s*\(\s*\)') {
            continue
        }
        $lineNumber = [int](& $lineAt $match.Index)
        $violations.Add($relative + ":" + $lineNumber +
            "  launchBackgroundThread captures this with no lifetime guard " +
            "(capture a token and test .expired() before touching the object)")
    }

    # --- QMetaObject::invokeMethod ---------------------------------------------
    foreach ($match in [regex]::Matches($text, 'QMetaObject::invokeMethod\s*\(')) {
        $scannedCalls += 1
        $call = Get-CallText -Text $text -Start ($match.Index + $match.Length - 1)
        $captures = Get-FirstCaptureList -Call $call
        if ($captures -notmatch '\bthis\b') {
            continue
        }
        if ($call -match '\.expired\s*\(\s*\)' -or $call -match 'QPointer') {
            continue
        }
        # Posting to `this` makes Qt drop the call when the receiver is gone, so a bare
        # `this` capture is safe there -- that is the shape ProxySession uses throughout.
        $receiver = Get-InvokeReceiver -Call $call
        if ($receiver -eq 'this') {
            continue
        }
        $lineNumber = [int](& $lineAt $match.Index)
        $violations.Add($relative + ":" + $lineNumber +
            "  invokeMethod posts to '" + $receiver + "' while the continuation captures this " +
            "(post to this, or hold a QPointer)")
    }
}

if ($scannedCalls -eq 0) {
    Stop-CheckWithError ("No launchBackgroundThread or QMetaObject::invokeMethod calls were found under " +
        "'$SourceRoot'. The patterns are stale, so this check would pass vacuously. Refusing to report a pass.")
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Host (("Async ownership check passed: {0} call site(s) across {1} file(s); every continuation that " +
    "captures this is guarded or posted to its own receiver.") -f $scannedCalls, $files.Count)
