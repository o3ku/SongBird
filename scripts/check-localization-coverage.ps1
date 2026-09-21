param(
    [string]$SourceRoot = "src",
    [string]$TranslationFile = "translations/SongBird_zh_CN.ts"
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

# Why this check exists
# --------------------
# The "0 unfinished" number reported by lupdate/lrelease is NOT a measure of
# localization coverage. It only counts entries that were already extracted into the
# .ts and are still awaiting a translation. Three whole classes of gap are invisible to it:
#
#   1. User-visible strings that were never wrapped in tr()/translate() at all. They
#      never reach the .ts, so there is nothing to be "unfinished". Every non-empty
#      OperationResult message lands in the log panel via AppBootstrap::appendResult()
#      and some of them raise a modal dialog via showOperationMessage(), so a bare
#      English literal here is user-visible text.
#   2. Strings that ARE wrapped in tr()/translate() but were never added to the .ts.
#      Because the .ts is hand-maintained (there is no lupdate build target), adding a
#      translate() call without updating the .ts silently leaves it untranslated.
#   3. Strings localized through a *helper* that lupdate cannot follow. lupdate only
#      extracts literal second arguments to translate(); a helper declared as
#      `QString trayText(const char* s) { return QCoreApplication::translate("TrayController", s); }`
#      passes a parameter, so every `trayText("Switch Server")` call site is invisible to
#      it and the string has to be added to the .ts by hand. When f646481 moved a batch of
#      direct tr() calls behind such helpers, 65 user-visible strings -- the entire tray
#      menu, the main toolbar, part of the add-server dialog -- stopped being extractable
#      and were dropped from the .ts, leaving them English-only. Check C guards that.
#   4. Strings *assembled* in a `return` statement rather than handed to a sink. Nothing that
#      watches sinks can see them, and a registration list only covers the helpers somebody
#      remembered. Check D is the generic net over every `return` in the tree.
#
# This script checks all of them. Checks A, A2, C and D are self-contained; Check B needs
# lupdate, which is an optional Qt component, so it is skipped loudly rather than silently.
#
# It is a net, not a proof: only literals that read like prose are reported (see the blind
# spot note on Test-ProseLiteral), so a pass means "nothing the scan can see is untranslated",
# not "every string is translated".

# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

# Strings that legitimately carry no translatable prose. Keys are the literal contents
# exactly as they appear between the quotes in C++.
$allowlist = @{
    '%1 %2'           = 'pure join template; both arguments are localized at the call site'
    'tun-compat | %1' = 'machine-readable log tag; the argument carries the text'
    'core | %1'       = 'machine-readable log tag; the argument carries the text'
    'tun | %1'        = 'machine-readable log tag; the argument carries the text'

    # Number + unit only. These render a latency reading ("US 42 ms") in the speed-test and
    # auto-node result columns. Strip the two slots and nothing is left but the unit symbol,
    # which is written the same way in every locale this app ships -- there is no prose for a
    # translator to move, and translating it would only risk a string that no longer fits the
    # column. The numbers themselves are formatted at the call site.
    '%1 ms'           = 'pure number + unit template; no translatable prose'
    '%1 %2 ms'        = 'pure number + unit template; no translatable prose'

    # Generated-and-persisted placeholder. normalizedServerRemarks() writes this into
    # VmessItem::remarks when a custom-config server has none, and remarks is saved to the
    # config file -- so translating it would make stored data depend on the UI language at
    # import time. The user-visible alternative is the generated value itself.
    'import custom@%1' = 'generated placeholder remarks; persisted into the saved config'

    # ---------------------------------------------------------------------------
    # Check D findings -- every one of these is a `return` the scan can see, so the reason has
    # to live here rather than in a comment: the allowlist IS the record.
    # ---------------------------------------------------------------------------

    # Matched *against* core process output by isTunAdapterConflictOutput() (app/TunRuntimeState.h).
    # Translating them breaks TUN adapter-conflict detection outright -- this is the case that
    # shows why "every English literal that reaches a string" cannot be the rule.
    'configure tun interface'                            = 'matched against core process output; translating it breaks TUN conflict detection'
    'cannot create a file when that file already exists' = 'matched against core process output; translating it breaks TUN conflict detection'
    'open interface take too much time'                  = 'matched against core process output; translating it breaks TUN conflict detection'

    # fallbackUserAgent() (common/UserAgent.h) -- an HTTP header sent to subscription servers,
    # never rendered. Translating it would change the wire format.
    'nekobox/5.11.15 (Prefer ClashMeta Format)' = 'HTTP User-Agent header; never shown to the user'

    # summarizeProcessOutput() (services/SpeedTestRuntimeProcess.cpp) -- the payload slot of the
    # pipe-delimited URL Test diagnostic lines allowlisted above. Machine data, not a sentence.
    '<no output>' = 'diagnostic payload inside the pipe-delimited URL Test lines'

    # ---------------------------------------------------------------------------
    # Reviewed and deliberately left English, but *not* reachable from any scan -- so no
    # suppression is in play and these stay a comment rather than allowlist entries. Adding
    # them to the allowlist would only pre-weaken a future check.
    #
    #   "Subscription parse: ..." (13 literals)   src/subscription/SubscriptionContentParser.cpp
    #       -- the per-pass parser trace appended by appendParseNotes() to the
    #       "nothing was parsed" failure. The message the user needs is translated; the
    #       trace says which parsers ran and how many items each produced, and its payload
    #       (sip008 / sing-box / clash / base64, count=, prefix=) is not prose.
    #       KNOWN ROUGH EDGE: the "Subscription parse: %1" variant embeds a translated
    #       skippedSummary(), so that one line mixes languages. Left as-is because the trace
    #       is read by whoever is debugging a broken subscription feed, and splitting the
    #       tag off the payload would break the grep-ability that makes it useful.
    # ---------------------------------------------------------------------------

    # The speed-test runtime writes pipe-delimited key=value diagnostic lines
    # ("URL Test batch | core failed to start: %1", "... | output=%2 | code=%3") that are
    # intended for log parsing, the same contract as the tags above. Translating the
    # tag would break that; the payload is already machine data. Revisit if these lines
    # are ever presented as user-facing prose rather than diagnostics.
    'URL Test batch | port allocation failed at entry %1' = 'pipe-delimited diagnostic line'
    'URL Test batch | temp dir failed'                    = 'pipe-delimited diagnostic line'
    'URL Test batch | failed to open config for write'    = 'pipe-delimited diagnostic line'
    'URL Test batch | no servers to test'                 = 'pipe-delimited diagnostic line'
    'URL Test batch | core missing'                       = 'pipe-delimited diagnostic line'
    'URL Test batch | core failed to start: %1'           = 'pipe-delimited diagnostic line'
    'URL Test batch | core not ready before timeout | output=%1' = 'pipe-delimited diagnostic line'
    'URL Test result | %1 -> %2'                          = 'pipe-delimited diagnostic line'
    'URL Test start failed | %1 | %2'                     = 'pipe-delimited diagnostic line'
    'URL Test proxy exited before ready | %1 | code=%2 | status=%3 | output=%4' = 'pipe-delimited diagnostic line'
    'URL Test startup timeout | %1 | output=%2'           = 'pipe-delimited diagnostic line'
    'URL Test config on timeout | %1 | %2'                = 'pipe-delimited diagnostic line'
    'URL Test failure detail | %1 | %2'                   = 'pipe-delimited diagnostic line'
    'URL Test: %1'                                        = 'pipe-delimited diagnostic line'
    'URL Test direct proxy: %1'                           = 'pipe-delimited diagnostic line'
}

# Functions whose string argument becomes a user-visible message. Extend this list
# whenever a new message-building helper is introduced -- a helper that takes a bare
# string and forwards it into an OperationResult is exactly the shape this check exists
# to catch. (saveFailureResult's `context` parameter is documented in
# persistence/IConfigRepository.h as describing what the app was doing, and it becomes
# the failure message verbatim when the repository has no extra detail to append.
# SongBirdAutoCoordinator's log() emits logMessage, which the SongBirdAuto window shows
# in its Logs dialog; setStatus() writes the status bar.
# AutoNodeEvaluationService's unavailableResult(item, error) writes `error` straight into
# AutoNodeEvaluation::error, which SongBirdAutoWindow::availabilityText() and
# AutoCoordinatorLogic::evaluationStateText() then render as the node's state column --
# so its argument is user-visible prose, not a diagnostic.)
$callPattern = [regex](
    '(?:OperationResult::(?:ok|fail|cancel)|saveFailureResult' +
    '|(?<![\w.>])(?:log|setStatus|unavailableResult))\s*\('
)

# Helpers that localize their argument through QCoreApplication::translate() with a context
# lupdate cannot see (see reason 3 at the top). Each entry maps the helper to the context it
# looks the string up under, so Check C can require the pair to exist in the .ts.
#
# Add a helper here when you introduce one of these wrappers. A wrapper is only worth having
# if it is registered -- an unregistered one silently un-localizes every string it touches,
# which is exactly what happened to the tray menu.
#
# Not registered: the `QStringLiteral("%1 %2").arg(...)` join idiom used at ~15 sites
# (e.g. IConfigRepository.h:47, ServerService.cpp). The template carries no prose for a
# translator -- "%1" and "%2" are positional placeholders -- and every argument is a
# translate() call at the call site. Localizing "%1 %2" itself is a no-op, and scanning it
# would bury the signal under ~15 identical findings; it stays deliberately out of scope.
$localizingHelpers = @{
    'proxySessionText' = 'ProxySession'
    'trDialog'         = 'AddServerDialog'
    'trCoreSettings'   = 'CoreSettingsPageWidget'
    'trMainWindow'     = 'MainWindow'
    'trayText'         = 'TrayController'
}

# Message-building helpers: functions that *assemble* a user-visible string and return it,
# instead of handing it to one of the sinks above. Check A only looks at sink arguments, so a
# bare English literal inside one of these is invisible to it -- that is how
# JsonConfigRepository::describeWriteFailure leaked an entire write path, and it is the blind
# spot Check A2 exists to close. Check A2 walks each listed helper's body and requires every
# `return` in it to be localized.
#
# Keys are "file : function"; the file is relative to the repository root. Keep this list
# short and deliberate: a helper belongs here only once you have confirmed its return value
# reaches the user.
$messageHelpers = @(
    @{ File = 'src/persistence/JsonConfigRepository.cpp'; Function = 'describeWriteFailure' }

    # SubscriptionUpdateService composes its "imported N server(s)" message as
    # `appendSkippedSummary(translate(...), report)`, so everything these three return is
    # appended to an OperationResult and lands in the log panel. The two append* helpers are
    # pure join templates (registered so the whole chain is covered, and so their "%1 (%2)" /
    # "%1 [%2]" returns are visibly deliberate); skippedSummary() is the one that builds prose.
    @{ File = 'src/services/SubscriptionUpdateService.cpp'; Function = 'appendSkippedSummary' }
    @{ File = 'src/services/SubscriptionUpdateService.cpp'; Function = 'appendParseNotes' }
    @{ File = 'src/subscription/SubscriptionContentParser.cpp'; Function = 'skippedSummary' }

    # ProxyCrashRestartPolicy builds the crash-restart log lines. `decide()` returns a Decision
    # whose `message` ProxySession emits via logMessage, and the other three assemble the text
    # for it: crashSummary() the core/exit-kind sentence, the two *Message() the frame around it.
    # All four were bare QStringLiteral() until 2026-09-20, so the whole restart path was
    # English-only; registering them is what keeps it from regressing.
    @{ File = 'src/app/ProxyCrashRestartPolicy.cpp'; Function = 'crashSummary' }
    @{ File = 'src/app/ProxyCrashRestartPolicy.cpp'; Function = 'restartDisabledMessage' }
    @{ File = 'src/app/ProxyCrashRestartPolicy.cpp'; Function = 'restartingMessage' }
    @{ File = 'src/app/ProxyCrashRestartPolicy.cpp'; Function = 'decide' }

    # The speed-test / auto-node result vocabulary. These are the "result" text a node row
    # shows when a probe fails or is skipped, so they are user-visible prose rather than
    # diagnostics -- the same values unavailableResult() is registered for above.
    #   runUrlTest()            -> SpeedTestWorker collects it into the result cell
    #   formatUrlProbeResult()  -> decides what that cell says for a finished probe
    #   availabilityText()      -> the SongBirdAuto node table's state column
    #   evaluationStateText()   -> the same value in SongBirdAuto's log line
    # The one-word members of that vocabulary ("Failed", "Cancelled", "Unsupported",
    # "Timeout", "Blocked", "Pending", "OK") are invisible to Check A2 -- see the note on
    # Test-ProseLiteral below -- and were reviewed and localized by hand instead.
    @{ File = 'src/services/SpeedTestRuntimeRunner.cpp'; Function = 'runUrlTest' }
    @{ File = 'src/services/SpeedTestServiceInternal.h'; Function = 'formatUrlProbeResult' }
    @{ File = 'src/services/SpeedTestUrlProbe.cpp'; Function = 'normalizeUpstreamProxyErrorText' }
    @{ File = 'src/auto/SongBirdAutoWindow.cpp'; Function = 'availabilityText' }
    @{ File = 'src/auto/AutoCoordinatorLogic.cpp'; Function = 'evaluationStateText' }

    # The subscription parse summary and the log panel's line decoration.
    #   skippedTypeLabel()  -> "(no type)" inside skippedSummary(), which is shown to the user
    #   normalizeLine()     -> " ... [truncated]" appended to a log line in the log panel
    @{ File = 'src/subscription/ClashProxyItemParser.cpp'; Function = 'skippedTypeLabel' }
    @{ File = 'src/ui/models/LogListModel.cpp'; Function = 'normalizeLine' }

    # The fallback display name for a server that has none. It IS user-visible (it becomes the
    # row's remarks), but the generated value is also written into the saved config, so the
    # one string that carries prose is allowlisted below rather than translated.
    @{ File = 'src/services/ServerService.cpp'; Function = 'normalizedServerRemarks' }
)

# Any of the ways this codebase localizes a string: the two Qt forms, the unqualified tr()
# used inside Q_OBJECT classes, and the wrapper helpers registered above (a literal handed to
# one of those is localized too, just with a context lupdate cannot derive).
$localizingPattern = ($localizingHelpers.Keys | Sort-Object | ForEach-Object {
    $escaped = [regex]::Escape($_)
    "\b$escaped\(\s*""(?:[^""\\]|\\.)*""\s*\)"
}) -join '|'

$translatedPattern = [regex](
    'QCoreApplication::translate\(\s*"[^"]*"\s*,\s*"(?:[^"\\]|\\.)*"\s*\)' +
    '|QObject::tr\(\s*"(?:[^"\\]|\\.)*"\s*\)' +
    '|\btr\(\s*"(?:[^"\\]|\\.)*"\s*\)' +
    $(if ($localizingPattern) { "|$localizingPattern" } else { '' })
)
$literalPattern = [regex]'"(?:[^"\\]|\\.)*"'

# Returns the text between the opening parenthesis at $OpenParenIndex and its match.
# Parens inside string literals are counted too; that is acceptable for this scan and
# keeps the implementation readable.
function Get-BalancedArgument {
    param([string]$Text, [int]$OpenParenIndex)

    $depth = 0
    for ($i = $OpenParenIndex; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq '(') {
            $depth++
        } elseif ($ch -eq ')') {
            $depth--
            if ($depth -eq 0) {
                return $Text.Substring($OpenParenIndex + 1, $i - $OpenParenIndex - 1)
            }
        }
    }
    return $Text.Substring($OpenParenIndex + 1)
}

# Returns the text of the statement that starts at $StartIndex, i.e. everything up to the
# ';' that terminates it at bracket depth zero. String literals are skipped so a ';' inside
# a message cannot cut the statement short.
function Get-StatementText {
    param([string]$Text, [int]$StartIndex)

    $depth = 0
    for ($i = $StartIndex; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq '"') {
            $i++
            while ($i -lt $Text.Length -and $Text[$i] -ne '"') {
                if ($Text[$i] -eq '\') { $i++ }
                $i++
            }
            continue
        }
        if ($ch -eq '(' -or $ch -eq '[' -or $ch -eq '{') {
            $depth++
        } elseif ($ch -eq ')' -or $ch -eq ']' -or $ch -eq '}') {
            $depth--
        } elseif ($ch -eq ';' -and $depth -le 0) {
            return $Text.Substring($StartIndex, $i - $StartIndex)
        }
    }
    return $Text.Substring($StartIndex)
}

# Returns the body of the function whose definition begins at $DefinitionIndex: the text
# between its outermost braces. String literals and comments are skipped so a brace inside a
# message cannot desynchronise the match. Returns $null when no body is found.
function Get-FunctionBody {
    param([string]$Text, [int]$DefinitionIndex)

    $start = $Text.IndexOf('{', $DefinitionIndex)
    if ($start -lt 0) { return $null }

    $depth = 0
    for ($i = $start; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq '"') {
            $i++
            while ($i -lt $Text.Length -and $Text[$i] -ne '"') {
                if ($Text[$i] -eq '\') { $i++ }
                $i++
            }
            continue
        }
        if ($ch -eq '/' -and $i + 1 -lt $Text.Length -and $Text[$i + 1] -eq '/') {
            $newline = $Text.IndexOf("`n", $i)
            if ($newline -lt 0) { return $null }
            $i = $newline
            continue
        }
        if ($ch -eq '/' -and $i + 1 -lt $Text.Length -and $Text[$i + 1] -eq '*') {
            $close = $Text.IndexOf('*/', $i + 2)
            if ($close -lt 0) { return $null }
            $i = $close + 1
            continue
        }
        if ($ch -eq '{') {
            $depth++
        } elseif ($ch -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return @{ Text = $Text.Substring($start, $i - $start + 1); Offset = $start }
            }
        }
    }
    return $null
}

# Strings, then comments, in one alternation: the regex engine scans left to right, so a `"`
# opens a string match before the `//` alternative can fire, and a `//` inside a URL literal is
# therefore never mistaken for a comment. That ordering is the whole point of a single pattern.
$scanTokenPattern = [regex]'(?s)"(?:[^"\\]|\\.)*"|//[^\n]*|/\*.*?\*/'

# Two same-length views of a translation unit, for Check D:
#
#   CommentFree -- comments blanked out, string contents intact. Statements are read from this
#                  one, because that is where the literals still are.
#   Masked      -- comments blanked AND string bodies blanked. `return` keywords are located in
#                  this one, so the word inside a message ("... no return value ...") cannot be
#                  mistaken for a statement, and a `return` mentioned in a comment is invisible.
#
# Offsets are preserved in both, so line numbers computed from either are correct.
function Get-ScanViews {
    param([string]$Text)

    $commentFree = New-Object System.Text.StringBuilder
    $masked = New-Object System.Text.StringBuilder
    $cursor = 0

    foreach ($token in $scanTokenPattern.Matches($Text)) {
        if ($token.Index -gt $cursor) {
            $between = $Text.Substring($cursor, $token.Index - $cursor)
            [void]$commentFree.Append($between)
            [void]$masked.Append($between)
        }

        if ($token.Value.StartsWith('"')) {
            [void]$commentFree.Append($token.Value)
            [void]$masked.Append('""' + (' ' * ($token.Value.Length - 2)))
        } else {
            # A comment: blank everything but the newlines so line numbers survive.
            $blanked = [regex]::Replace($token.Value, '[^\n]', ' ')
            [void]$commentFree.Append($blanked)
            [void]$masked.Append($blanked)
        }
        $cursor = $token.Index + $token.Length
    }

    if ($cursor -lt $Text.Length) {
        $tail = $Text.Substring($cursor)
        [void]$commentFree.Append($tail)
        [void]$masked.Append($tail)
    }

    return @{ CommentFree = $commentFree.ToString(); Masked = $masked.ToString() }
}

function Test-ProseLiteral {
    param([string]$LiteralContent)

    # Needs both a letter and whitespace to look like a sentence. This skips "%1",
    # separators like ", " and pure format strings.
    #
    # KNOWN BLIND SPOT: the whitespace requirement also hides every one-word message, so a
    # bare QStringLiteral("Cancelled") / ("Failed") / ("Unsupported") / ("Timeout") /
    # ("Blocked") / ("Pending") / ("OK") at a registered sink passes this check unnoticed.
    # Those are real user-visible values -- the speed-test and auto-node result columns are
    # made of exactly them -- so they cannot be left to the scan. Do NOT relax the requirement
    # to `[A-Za-z]` to reach them: that also flags every enum-ish literal at a sink ("http",
    # "socks", "domain", "ip", "crash", "exit") and buries the signal. Instead, when you touch
    # a result pipeline, grep it for single-word literals by hand. The vocabulary listed above
    # was localized on 2026-09-20; re-check it with:
    #   rg 'QStringLiteral\("(Failed|Cancelled|Unsupported|Timeout|Blocked|Pending|OK)"\)' src
    return ($LiteralContent -match '[A-Za-z]') -and ($LiteralContent -match '\s')
}

# Reads a .ts into a map of context name -> set of source strings.
function Get-TranslationPairs {
    param([string]$Path)

    $pairs = @{}
    [xml]$document = Get-Content -LiteralPath $Path -Raw
    foreach ($context in $document.TS.context) {
        $name = $context.name
        foreach ($message in $context.message) {
            $source = $message.source
            if (-not $source) { continue }
            if (-not $pairs.ContainsKey($name)) {
                $pairs[$name] = New-Object System.Collections.Generic.HashSet[string]
            }
            [void]$pairs[$name].Add([string]$source)
        }
    }
    return $pairs
}

$root = Resolve-Path $SourceRoot
$violations = New-Object System.Collections.Generic.List[string]

$sourceFiles = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @('.h', '.cpp') }

# A check that scans nothing passes trivially, and a silent no-op is more dangerous than a
# red test -- it manufactures confidence. Refuse to report success in that case.
if (-not $sourceFiles) {
    Stop-CheckWithError "No C++ sources found under '$SourceRoot'; refusing to report a pass."
}

# ---------------------------------------------------------------------------
# Check A: bare prose literals passed to OperationResult::{ok,fail,cancel}
# ---------------------------------------------------------------------------

$callSiteCount = 0
foreach ($file in $sourceFiles) {
    $text = [System.IO.File]::ReadAllText($file.FullName)
    foreach ($call in $callPattern.Matches($text)) {
        $callSiteCount++
        $argument = Get-BalancedArgument -Text $text -OpenParenIndex ($call.Index + $call.Length - 1)
        $stripped = $translatedPattern.Replace($argument, '')

        foreach ($literal in $literalPattern.Matches($stripped)) {
            $content = $literal.Value.Substring(1, $literal.Value.Length - 2)
            if (-not (Test-ProseLiteral $content)) {
                continue
            }
            if ($allowlist.ContainsKey($content)) {
                continue
            }

            $line = ($text.Substring(0, $call.Index) -split "`n").Count
            $relative = Resolve-Path -Relative $file.FullName
            $violations.Add(
                "Un-localized user-visible literal at ${relative}:${line} -- wrap it in " +
                "QCoreApplication::translate(""<Context>"", ...) or add it to the allowlist " +
                "in this script with a reason: `"$content`""
            )
        }
    }
}

# Same reasoning as above: if none of the known message sinks were found, the pattern has
# drifted out of sync with the code and a clean result would be meaningless.
if ($callSiteCount -eq 0) {
    Stop-CheckWithError "Found no OperationResult/log/setStatus call sites under '$SourceRoot'; the scan pattern is stale. Refusing to report a pass."
}

# ---------------------------------------------------------------------------
# Check A2: bare prose literals returned by a registered message helper
# ---------------------------------------------------------------------------

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$messageHelperCount = 0

# Body ranges of the registered helpers, keyed by absolute path. Check D scans every `return`
# in the tree and skips these, so each literal has exactly one owner and one report -- A2's
# message names the helper, which is more useful than D's "some function returned this".
$messageHelperRanges = @{}

foreach ($entry in $messageHelpers) {
    $path = Join-Path $repositoryRoot $entry.File
    if (-not (Test-Path -LiteralPath $path)) {
        Stop-CheckWithError "Message helper '$($entry.Function)' lists file '$($entry.File)', which does not exist. Fix the registration rather than dropping it."
    }

    $text = [System.IO.File]::ReadAllText($path)
    # Anchor on a definition, not the first mention: LogListModel.cpp calls
    # normalizeLine() at line 78 and defines it at line 109, and matching the call site
    # would hand Get-FunctionBody a random later block. A definition line starts at the
    # beginning of a line and has no ';' or '=' before the name.
    $definition = [regex]::Match(
        $text, "(?m)^[^;=`n]*\b$([regex]::Escape($entry.Function))\s*\(")
    if (-not $definition.Success) {
        Stop-CheckWithError "Message helper '$($entry.Function)' was not found in '$($entry.File)'. Fix the registration rather than dropping it."
    }

    $body = Get-FunctionBody -Text $text -DefinitionIndex $definition.Index
    if (-not $body) {
        Stop-CheckWithError "Could not read the body of message helper '$($entry.Function)' in '$($entry.File)'; refusing to report a pass."
    }
    # A message helper returns a string, so a body with no `return` means we locked onto the
    # wrong block -- which would make the scan below vacuously clean. Fail loudly instead.
    if ($body.Text -notmatch '\breturn\b') {
        Stop-CheckWithError ("Read a body with no return for message helper '$($entry.Function)' in " +
                     "'$($entry.File)'; the definition lookup matched the wrong block. " +
                     "Refusing to report a pass.")
    }
    $messageHelperCount++
    if (-not $messageHelperRanges.ContainsKey($path)) {
        $messageHelperRanges[$path] = New-Object System.Collections.Generic.List[object]
    }
    $messageHelperRanges[$path].Add(@{
        Start = $body.Offset
        End   = $body.Offset + $body.Text.Length
    })

    foreach ($keyword in [regex]::Matches($body.Text, '\breturn\b')) {
        $statement = Get-StatementText -Text $body.Text -StartIndex ($keyword.Index + $keyword.Length)
        $stripped = $translatedPattern.Replace($statement, '')

        foreach ($literal in $literalPattern.Matches($stripped)) {
            $content = $literal.Value.Substring(1, $literal.Value.Length - 2)
            if (-not (Test-ProseLiteral $content)) {
                continue
            }
            if ($allowlist.ContainsKey($content)) {
                continue
            }

            $absolute = $body.Offset + $keyword.Index
            $line = ($text.Substring(0, $absolute) -split "`n").Count
            $where = "$($entry.File):$line"
            $violations.Add(
                "Un-localized user-visible literal returned by $($entry.Function)() at $where -- " +
                "its return value reaches the user, so wrap it in " +
                "QCoreApplication::translate(""<Context>"", ...) and add it to the translation " +
                "file under that context: `"$content`""
            )
        }
    }
}

if ($messageHelperCount -eq 0) {
    Stop-CheckWithError "None of the registered message helpers were inspected; refusing to report a pass."
}

# ---------------------------------------------------------------------------
# Check D: bare prose literals returned from *any* function
# ---------------------------------------------------------------------------
#
# A2 only sees helpers somebody remembered to register, which is the check's last real hole: a
# newly written helper that builds a user-visible string in its `return` is invisible until its
# author registers it -- and the two worst leaks in this project (JsonConfigRepository's write
# path, ProxyCrashRestartPolicy's restart messages) were exactly that. D is the generic net
# under it: every `return` in the tree, minus the registered helpers A2 already owns.
#
# It is affordable because it was measured first. A scan of every `return` yields a handful of
# hits, not hundreds: the prose test (a letter AND whitespace) skips format templates and
# separators, and `$translatedPattern` recognises the localizing helpers. Expect the survivors
# to be matching strings, wire-format constants and diagnostic payloads -- that is, entries that
# belong in `$allowlist` with a written reason, which is the point.
#
# KNOWN LIMIT: one-word messages ("Failed", "Cancelled", ...) are still invisible here for the
# same reason as everywhere else. See the note on Test-ProseLiteral.

$returnCount = 0
$returnLiteralCount = 0

foreach ($file in $sourceFiles) {
    $views = Get-ScanViews -Text ([System.IO.File]::ReadAllText($file.FullName))
    $ranges = $messageHelperRanges[$file.FullName]

    foreach ($keyword in [regex]::Matches($views.Masked, '\breturn\b')) {
        if ($ranges) {
            $owned = $false
            foreach ($range in $ranges) {
                if ($keyword.Index -ge $range.Start -and $keyword.Index -lt $range.End) {
                    $owned = $true
                    break
                }
            }
            if ($owned) {
                continue
            }
        }

        $returnCount++
        $statement = Get-StatementText -Text $views.CommentFree -StartIndex ($keyword.Index + $keyword.Length)
        $stripped = $translatedPattern.Replace($statement, '')

        foreach ($literal in $literalPattern.Matches($stripped)) {
            $returnLiteralCount++
            $content = $literal.Value.Substring(1, $literal.Value.Length - 2)
            if (-not (Test-ProseLiteral $content)) {
                continue
            }
            if ($allowlist.ContainsKey($content)) {
                continue
            }

            $line = ($views.CommentFree.Substring(0, $keyword.Index) -split "`n").Count
            $relative = Resolve-Path -Relative $file.FullName
            $violations.Add(
                "Un-localized user-visible literal returned at ${relative}:${line} -- if this " +
                "return value reaches the user, wrap it in QCoreApplication::translate(""<Context>"", ...) " +
                "and register the function in `$messageHelpers; if it does not, add the literal " +
                "to the allowlist in this script with a reason: `"$content`""
            )
        }
    }
}

# Same hazard as everywhere else: if the scan matched no `return` at all it proves nothing.
if ($returnCount -eq 0) {
    Stop-CheckWithError "Found no `return` statements under '$SourceRoot'; refusing to report a pass."
}

# ---------------------------------------------------------------------------
# Check C: strings passed to a runtime-translate helper must be in the .ts
# ---------------------------------------------------------------------------

if (-not (Test-Path -LiteralPath $TranslationFile)) {
    $violations.Add("Translation file not found: $TranslationFile")
} else {
    $existing = Get-TranslationPairs -Path $TranslationFile
    $helperCallSiteCount = 0

    foreach ($helper in ($localizingHelpers.Keys | Sort-Object)) {
        $context = $localizingHelpers[$helper]
        $helperPattern = [regex]("\b$([regex]::Escape($helper))\(\s*""((?:[^""\\]|\\.)*)""")
        $contextFound = $existing.ContainsKey($context)

        foreach ($file in $sourceFiles) {
            $text = [System.IO.File]::ReadAllText($file.FullName)
            foreach ($call in $helperPattern.Matches($text)) {
                $helperCallSiteCount++
                $source = $call.Groups[1].Value
                if ($contextFound -and $existing[$context].Contains($source)) {
                    continue
                }

                $line = ($text.Substring(0, $call.Index) -split "`n").Count
                $relative = Resolve-Path -Relative $file.FullName
                $where = "${relative}:${line}"
                $violations.Add(
                    "$helper() localizes through context ""$context"", but $where passes a " +
                    "string the translation file does not carry under that context, so it " +
                    "renders in English: `"$source`""
                )
            }
        }
    }

    if ($helperCallSiteCount -eq 0) {
        Stop-CheckWithError "Found no call sites for any helper registered in `$localizingHelpers; either the registrations are stale or the call sites moved. Refusing to report a pass."
    }
}

# ---------------------------------------------------------------------------
# Check B: every string lupdate can extract is present in the .ts
# ---------------------------------------------------------------------------

# Some restricted shells ship a PATHEXT that omits .EXE. PowerShell then treats a native
# tool as a "document" and refuses to run it ("Cannot run a document in the middle of a
# pipeline"), which would silently disable this check. Normalize it for this process.
if ($env:PATHEXT -notmatch '(?i)\.EXE') {
    $env:PATHEXT = ".COM;.EXE;.BAT;.CMD" + $(if ($env:PATHEXT) { ";$env:PATHEXT" } else { "" })
}

function Resolve-LupdateExecutable {
    $candidates = @()

    $command = Get-Command lupdate -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    # The CMake presets point CMAKE_PREFIX_PATH at $env:QT5_PREFIX_PATH, so honour the
    # same variable here instead of hard-coding an install path.
    foreach ($variable in @('QT5_PREFIX_PATH', 'QT6_PREFIX_PATH', 'QTDIR')) {
        $prefix = [Environment]::GetEnvironmentVariable($variable)
        if ($prefix) {
            $candidates += (Join-Path $prefix 'bin/lupdate.exe')
        }
    }

    # That variable is usually unset in a fresh shell, which would make Check B silently
    # skip on a machine where Qt is plainly installed. The CMake cache records the exact
    # Qt the project was configured against, so prefer it -- same install the build uses.
    # Resolved from the script's own location so this works whatever the working directory.
    $repositoryRoot = Split-Path -Parent $PSScriptRoot
    foreach ($tree in @('build/msvc-release', 'build/msvc-tests')) {
        $cachePath = Join-Path $repositoryRoot (Join-Path $tree 'CMakeCache.txt')
        if (-not (Test-Path -LiteralPath $cachePath)) { continue }
        $entry = Select-String -LiteralPath $cachePath -Pattern '^Qt[56]_DIR:PATH=(.+)$' -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $entry) { continue }
        $qtDirectory = ($entry.Matches[0].Groups[1].Value.Trim()) -replace '\\', '/'
        if ($qtDirectory -match '^(?<prefix>.+)/lib/cmake/Qt[56]$') {
            $candidates += (Join-Path $Matches['prefix'] 'bin/lupdate.exe')
        }
    }

    # Ask Qt where its binaries live; this is how the build finds lrelease too.
    foreach ($qmakeName in @('qmake', 'qmake6')) {
        $qmake = Get-Command $qmakeName -ErrorAction SilentlyContinue
        if (-not $qmake) { continue }
        try {
            $binDir = (& $qmake.Source -query QT_INSTALL_BINS 2>$null)
            if ($LASTEXITCODE -eq 0 -and $binDir) {
                $candidates += (Join-Path ($binDir.Trim()) 'lupdate.exe')
            }
        } catch {
            # Ignore and fall through to the other candidates.
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }
    return $null
}

$lupdate = Resolve-LupdateExecutable
if (-not $lupdate) {
    Write-Host "NOTE: lupdate was not found, so Check B (untranslated-but-wrapped strings) was SKIPPED."
    Write-Host "      Install Qt's LinguistTools or put lupdate on PATH to enable it."
} elseif (-not (Test-Path -LiteralPath $TranslationFile)) {
    # Already reported above; nothing further to add here.
} else {
    $scratch = [System.IO.Path]::Combine(
        [System.IO.Path]::GetTempPath(),
        "songbird-lupdate-$([System.Guid]::NewGuid().ToString('N')).ts"
    )
    try {
        $null = & $lupdate $SourceRoot -ts $scratch -no-obsolete 2>&1
        if ($LASTEXITCODE -ne 0) {
            $violations.Add("lupdate failed with exit code $LASTEXITCODE")
        } else {
            $extracted = Get-TranslationPairs -Path $scratch
            $knownPairs = Get-TranslationPairs -Path $TranslationFile

            # If lupdate extracted nothing the comparison below is vacuous -- the same
            # silent-pass hazard as the empty-scan guard above.
            $extractedCount = 0
            foreach ($set in $extracted.Values) { $extractedCount += $set.Count }
            if ($extractedCount -eq 0) {
                $violations.Add(
                    "lupdate extracted no strings from '$SourceRoot', so comparing against " +
                    "$TranslationFile proves nothing; refusing to report a pass."
                )
            }

            foreach ($contextName in ($extracted.Keys | Sort-Object)) {
                $known = $knownPairs[$contextName]
                foreach ($source in ($extracted[$contextName] | Sort-Object)) {
                    if ($known -and $known.Contains($source)) {
                        continue
                    }
                    $preview = if ($source.Length -gt 90) { $source.Substring(0, 87) + '...' } else { $source }
                    $violations.Add(
                        "translate()/tr() string missing from $TranslationFile -- add it, " +
                        "otherwise it renders in English: [$contextName] $preview"
                    )
                }
            }
        }
    } finally {
        if (Test-Path -LiteralPath $scratch) {
            Remove-Item -LiteralPath $scratch -Force
        }
    }
}

if ($violations.Count -gt 0) {
    Stop-CheckWithError (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
}

# Positive evidence that each scan actually ran. A green run that printed nothing is
# indistinguishable from a green run that scanned nothing, and a silent no-op is more
# dangerous than a red test because it manufactures confidence. Print the counts so a pass
# can be checked by eye -- and so "the check passed" is never the only thing you have.
function Get-ScanCount {
    param([string]$Name)
    $variable = Get-Variable -Name $Name -ErrorAction SilentlyContinue
    if ($null -eq $variable) { return 'n/a' }
    return [string]$variable.Value
}

Write-Host ("Localization coverage: A scanned $(Get-ScanCount 'callSiteCount') message-sink " +
            "call site(s); A2 inspected $(Get-ScanCount 'messageHelperCount') message helper(s); " +
            "D scanned $(Get-ScanCount 'returnCount') unregistered 'return' statement(s) " +
            "($(Get-ScanCount 'returnLiteralCount') literal(s) read); " +
            "C checked $(Get-ScanCount 'helperCallSiteCount') localizing-helper call site(s); " +
            "B compared $(Get-ScanCount 'extractedCount') lupdate-extracted string(s).")
Write-Host "Localization coverage check passed."
