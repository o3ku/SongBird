param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# C++ destroys non-static data members in REVERSE declaration order, so the order
# of the members in AppBootstrapObjects.h is semantic, not cosmetic. Every
# composition phase hands a reference to an already constructed member into a
# newly constructed one:
#
#     objects_->proxySession = std::make_unique<ProxySession>(*objects_->repository, ...);
#     objects_->speedTestCoordinator = std::make_unique<SpeedTestCoordinator>(objects_->mainWindow.get(), ...);
#
# Either the callee stores the reference (`Dependencies { T& }`) or it uses the
# pointer as a QObject parent. Both mean "the callee must die before the member it
# borrows". If the declaration order is wrong the borrowed member is destroyed
# first, and for the QObject-parent case that is an outright double free:
# ~QObject deletes the child, then the child's own unique_ptr deletes it again.
#
# Nothing else catches this. unique_ptr in isolation is correct, QObject parenting
# in isolation is correct, and the bug only shows up at shutdown -- so no unit test
# observes it and the compiler cannot warn. It shipped once already; see todo.md 2.0.
#
# The rule enforced here: for `objects_->TARGET = std::make_unique<...>(...)`,
# every `objects_->DEP` passed in the constructor arguments must be declared
# BEFORE TARGET (DEP's index must be strictly smaller).
#
# Only *immediate* borrows count. A dependency named inside a lambda body is a
# deferred use: the lambda runs later (on a button press, on a timer), never while
# the object graph is being torn down, so it imposes no ordering requirement.
# Passing `objects_->backgroundTasks.get()` into a Dependencies aggregate is an
# immediate borrow; `[this]() { objects_->mainWindow->appendLog(...); }` is not.
# Text inside string literals and comments is ignored for the same reason.

$script:Chars = @{
    Backslash = [char]92
    DoubleQuote = [char]34
    SingleQuote = [char]39
    Slash = [char]47
    Star = [char]42
    LeftParen = [char]40
    RightParen = [char]41
    LeftBracket = [char]91
    RightBracket = [char]93
    LeftBrace = [char]123
    RightBrace = [char]125
    LeftAngle = [char]60
    RightAngle = [char]62
    Dash = [char]45
    Greater = [char]62
    Newline = [char]10
}

function Write-Failure {
    param([string]$Message)
    # Explicit -ErrorAction Continue so the message reaches stderr (and therefore
    # ctest output) even though $ErrorActionPreference is Stop here.
    Write-Error -Message $Message -ErrorAction Continue
    exit 1
}

# Index of the delimiter closing the one at $OpenIndex, or -1. Skips string and
# character literals and comments so parentheses, brackets and braces inside
# user-visible text (for example tr("... (see logs)")) do not shift the depth.
function Find-MatchingDelimiter {
    param(
        [string]$Text,
        [int]$OpenIndex,
        [char]$Open,
        [char]$Close
    )

    $c = $script:Chars
    $depth = 0
    $inString = $false
    $inChar = $false
    $inLineComment = $false
    $inBlockComment = $false

    for ($i = $OpenIndex; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        $next = if ($i + 1 -lt $Text.Length) { $Text[$i + 1] } else { [char]0 }

        if ($inLineComment) {
            if ($ch -eq $c.Newline) { $inLineComment = $false }
            continue
        }
        if ($inBlockComment) {
            if ($ch -eq $c.Star -and $next -eq $c.Slash) { $inBlockComment = $false; $i++ }
            continue
        }
        if ($inString) {
            if ($ch -eq $c.Backslash) { $i++ }
            elseif ($ch -eq $c.DoubleQuote) { $inString = $false }
            continue
        }
        if ($inChar) {
            if ($ch -eq $c.Backslash) { $i++ }
            elseif ($ch -eq $c.SingleQuote) { $inChar = $false }
            continue
        }

        if ($ch -eq $c.Slash -and $next -eq $c.Slash) { $inLineComment = $true; $i++ }
        elseif ($ch -eq $c.Slash -and $next -eq $c.Star) { $inBlockComment = $true; $i++ }
        elseif ($ch -eq $c.DoubleQuote) { $inString = $true }
        elseif ($ch -eq $c.SingleQuote) { $inChar = $true }
        elseif ($ch -eq $Open) { $depth++ }
        elseif ($ch -eq $Close) {
            $depth--
            if ($depth -eq 0) { return $i }
        }
    }
    return -1
}

# If the '[' at $OpenBracket introduces a lambda, returns the index of the '{' that
# opens its body; otherwise -1. Accepts the shapes used in this codebase:
#   [capture] { ... }            [capture]() { ... }
#   [capture](args) { ... }      [capture]() -> Type { ... }
#   [capture]() mutable { ... }  [capture]() noexcept { ... }
function Find-LambdaBodyBrace {
    param(
        [string]$Text,
        [int]$OpenBracket
    )

    $c = $script:Chars
    $bracketClose = Find-MatchingDelimiter -Text $Text -OpenIndex $OpenBracket -Open $c.LeftBracket -Close $c.RightBracket
    if ($bracketClose -lt 0) { return -1 }

    $k = $bracketClose + 1
    while ($k -lt $Text.Length -and [char]::IsWhiteSpace($Text[$k])) { $k++ }

    if ($k -lt $Text.Length -and $Text[$k] -eq $c.LeftParen) {
        $parenClose = Find-MatchingDelimiter -Text $Text -OpenIndex $k -Open $c.LeftParen -Close $c.RightParen
        if ($parenClose -lt 0) { return -1 }
        $k = $parenClose + 1
        while ($k -lt $Text.Length -and [char]::IsWhiteSpace($Text[$k])) { $k++ }
    }

    # Trailing return type: `-> Type {`. Bounded so a stray `->` (for example a
    # `ptr->member` access) cannot make us swallow the rest of the argument list.
    if ($k + 1 -lt $Text.Length -and $Text[$k] -eq $c.Dash -and $Text[$k + 1] -eq $c.Greater) {
        $limit = [Math]::Min($Text.Length, $k + 120)
        for ($j = $k + 2; $j -lt $limit; $j++) {
            if ($Text[$j] -eq $c.LeftBrace) { return $j }
            if ($Text[$j] -eq $c.LeftParen -or $Text[$j] -eq $c.RightParen -or $Text[$j] -eq [char]59) { return -1 }
        }
        return -1
    }

    $specifiers = @("mutable", "noexcept", "constexpr", "consteval", "const")
    while ($true) {
        $consumed = $false
        foreach ($specifier in $specifiers) {
            if ($k + $specifier.Length -le $Text.Length -and $Text.Substring($k, $specifier.Length) -eq $specifier) {
                $after = $k + $specifier.Length
                $boundary = $after -ge $Text.Length -or (-not [char]::IsLetterOrDigit($Text[$after]) -and $Text[$after] -ne [char]95)
                if ($boundary) {
                    $k = $after
                    while ($k -lt $Text.Length -and [char]::IsWhiteSpace($Text[$k])) { $k++ }
                    $consumed = $true
                    break
                }
            }
        }
        if (-not $consumed) { break }
    }

    if ($k -lt $Text.Length -and $Text[$k] -eq $c.LeftBrace) { return $k }
    return -1
}

# Returns a copy of $Text in which every deferred / non-code region is replaced by
# spaces of the same length. Offsets and line numbers are therefore identical to
# the original, but regex matches can no longer land inside a region we do not
# want to treat as a construction-time borrow.
function Get-CodeOnlyText {
    param([string]$Text)

    $c = $script:Chars
    $chars = $Text.ToCharArray()
    $n = $Text.Length
    $i = 0

    while ($i -lt $n) {
        $ch = $Text[$i]
        $next = if ($i + 1 -lt $n) { $Text[$i + 1] } else { [char]0 }

        if ($ch -eq $c.Slash -and $next -eq $c.Slash) {
            while ($i -lt $n -and $Text[$i] -ne $c.Newline) { $chars[$i] = ' '; $i++ }
            continue
        }

        if ($ch -eq $c.Slash -and $next -eq $c.Star) {
            $chars[$i] = ' '; $chars[$i + 1] = ' '; $i += 2
            while ($i -lt $n) {
                if ($Text[$i] -eq $c.Star -and $i + 1 -lt $n -and $Text[$i + 1] -eq $c.Slash) {
                    $chars[$i] = ' '; $chars[$i + 1] = ' '; $i += 2; break
                }
                if ($Text[$i] -ne $c.Newline) { $chars[$i] = ' ' }
                $i++
            }
            continue
        }

        if ($ch -eq $c.DoubleQuote -or $ch -eq $c.SingleQuote) {
            $quote = $ch
            $chars[$i] = ' '; $i++
            while ($i -lt $n) {
                if ($Text[$i] -eq $c.Backslash) {
                    $chars[$i] = ' '
                    if ($i + 1 -lt $n) { $chars[$i + 1] = ' ' }
                    $i += 2
                    continue
                }
                if ($Text[$i] -eq $quote) { $chars[$i] = ' '; $i++; break }
                if ($Text[$i] -ne $c.Newline) { $chars[$i] = ' ' }
                $i++
            }
            continue
        }

        if ($ch -eq $c.LeftBracket) {
            $bodyBrace = Find-LambdaBodyBrace -Text $Text -OpenBracket $i
            if ($bodyBrace -ge 0) {
                $bodyEnd = Find-MatchingDelimiter -Text $Text -OpenIndex $bodyBrace -Open $c.LeftBrace -Close $c.RightBrace
                if ($bodyEnd -lt 0) { $bodyEnd = $n - 1 }
                for ($j = $i; $j -le $bodyEnd; $j++) {
                    if ($Text[$j] -ne $c.Newline) { $chars[$j] = ' ' }
                }
                $i = $bodyEnd + 1
                continue
            }
        }

        $i++
    }

    return -join $chars
}

$headerPath = Join-Path $SourceRoot "app/AppBootstrapObjects.h"
if (-not (Test-Path -LiteralPath $headerPath)) {
    Write-Failure "Missing composition root header: $headerPath"
}

# Member name -> @{ Index = declaration position; Line = 1-based line number }
$members = @{}
$memberIndex = 0
$headerLineNumber = 0
foreach ($line in (Get-Content -LiteralPath $headerPath)) {
    $headerLineNumber++
    if ($line -match '^\s*std::unique_ptr<.+>\s+([A-Za-z_]\w*)\s*;\s*$') {
        $members[$Matches[1]] = @{ Index = $memberIndex; Line = $headerLineNumber }
        $memberIndex++
    }
}

if ($memberIndex -eq 0) {
    Write-Failure "No std::unique_ptr members parsed from $headerPath -- the parser is out of date, refusing to report success."
}

$appRoot = Join-Path $SourceRoot "app"
$wiringFiles = @(Get-ChildItem -LiteralPath $appRoot -Filter "AppBootstrap*.cpp" -File -ErrorAction SilentlyContinue)
if ($wiringFiles.Count -eq 0) {
    Write-Failure "No AppBootstrap*.cpp files found under $appRoot -- refusing to report success."
}

$violations = New-Object System.Collections.Generic.List[string]
$unresolved = New-Object System.Collections.Generic.List[string]
$checked = 0
$deferred = 0

foreach ($file in $wiringFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    $codeOnly = Get-CodeOnlyText -Text $text
    $shortName = Split-Path -Leaf $file.FullName

    # Anchor on the assignment target so the member name comes from the field being
    # written, not from the constructed type (they differ, for example
    # `objects_->systemProxyService = std::make_unique<WindowsSystemProxyService>()`).
    $pattern = 'objects_->([A-Za-z_]\w*)\s*=\s*std::make_unique\s*<'
    foreach ($match in [regex]::Matches($codeOnly, $pattern)) {
        $target = $match.Groups[1].Value
        $lineOfMatch = ($text.Substring(0, $match.Index) -split "`n").Count

        # Locate the constructor argument list: skip the template argument list of
        # make_unique, then take the next '('.
        $angleOpen = $match.Index + $match.Length - 1
        $angleClose = Find-MatchingDelimiter -Text $text -OpenIndex $angleOpen -Open $script:Chars.LeftAngle -Close $script:Chars.RightAngle
        if ($angleClose -lt 0) {
            $violations.Add("${shortName}:${lineOfMatch}: unbalanced '<' in make_unique<$target>")
            continue
        }
        $parenOpen = $text.IndexOf($script:Chars.LeftParen, $angleClose)
        if ($parenOpen -lt 0) {
            $violations.Add("${shortName}:${lineOfMatch}: missing '(' after make_unique<$target>")
            continue
        }
        $parenClose = Find-MatchingDelimiter -Text $text -OpenIndex $parenOpen -Open $script:Chars.LeftParen -Close $script:Chars.RightParen
        if ($parenClose -lt 0) {
            $violations.Add("${shortName}:${lineOfMatch}: unbalanced '(' in make_unique<$target>")
            continue
        }

        $arguments = $codeOnly.Substring($parenOpen + 1, $parenClose - $parenOpen - 1)

        if (-not $members.ContainsKey($target)) {
            $unresolved.Add("${shortName}:${lineOfMatch}: '$target' is assigned but not declared in $headerPath")
            continue
        }

        $seen = @{}
        foreach ($dependencyMatch in [regex]::Matches($arguments, 'objects_->([A-Za-z_]\w*)')) {
            $dependency = $dependencyMatch.Groups[1].Value
            if ($dependency -eq $target -or $seen.ContainsKey($dependency)) { continue }
            $seen[$dependency] = $true

            if (-not $members.ContainsKey($dependency)) {
                $unresolved.Add("${shortName}:${lineOfMatch}: '$dependency' is used but not declared in $headerPath")
                continue
            }

            $checked++
            if ($members[$dependency].Index -ge $members[$target].Index) {
                $violations.Add(
                    "${shortName}:${lineOfMatch}: '$target' (declared at ${headerPath}:$($members[$target].Line), position $($members[$target].Index)) " +
                    "borrows '$dependency' (declared at line $($members[$dependency].Line), position $($members[$dependency].Index)) " +
                    "but is destroyed FIRST -- declare '$target' after '$dependency' in AppBootstrapObjects.h")
            }
        }
    }

    # Report how many dependency mentions were intentionally skipped as deferred, so
    # a future edit that over-masks the source cannot silently gut the check.
    $deferred += ([regex]::Matches($text, 'objects_->[A-Za-z_]\w*').Count - [regex]::Matches($codeOnly, 'objects_->[A-Za-z_]\w*').Count)
}

# A silent no-op is worse than a red test: if nothing was actually compared, the
# parse has drifted from the source and the check must fail loudly.
if ($checked -eq 0) {
    Write-Failure "Parsed $($wiringFiles.Count) wiring file(s) but found no member dependencies to compare -- the scanner is out of date, refusing to report success."
}

if ($unresolved.Count -gt 0) {
    Write-Failure (($unresolved | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

if ($violations.Count -gt 0) {
    Write-Failure (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

Write-Output "AppBootstrap member order check passed: $checked construction-time dependencies across $($wiringFiles.Count) wiring file(s), $memberIndex members, $deferred deferred (lambda) reference(s) ignored."
