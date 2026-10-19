param(
    [string]$BuildDirectory = ""
)

$ErrorActionPreference = "Stop"

# Why this check exists
# --------------------
# Every Qt Test executable in this repository registers its cases inside a
# generated main(), so a target whose SOURCES list is empty -- or whose test file
# silently stopped being compiled because it was moved, renamed, or dropped from
# a CMake list -- links successfully and then runs ZERO test functions. Qt Test
# prints "Totals: 0 passed, 0 failed" and exits 0, and CTest reports Passed.
#
# That is not hypothetical: a suite went green twice while running nothing, which
# is what made this check necessary. In this repository it is reachable in one step,
# because the per-target source lists in tests/CMakeLists.txt are maintained by hand
# and the shared code lives in songbird_shared_objs -- a target can keep linking
# while its own .cpp is gone.
#
# The rule: a test executable that reports 0 passed cases is a FAILURE, not a
# pass. A binary that cannot be started at all is also a failure (a missing Qt
# platform plugin used to look like a silent pass).
#
# Usage:
#   pwsh -File scripts/check-test-suites.ps1 -BuildDirectory build/msvc-debug

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    # Resolve from the script location so a bare invocation still checks the tree it lives in.
    $BuildDirectory = Join-Path $PSScriptRoot "..\build\msvc-debug"
}

$testDirectory = Join-Path $BuildDirectory "tests"
if (-not (Test-Path -LiteralPath $testDirectory)) {
    Write-Error "No test directory at '$testDirectory'. Refusing to report a pass on a scan that found nothing." -ErrorAction Continue
    exit 1
}

$executables = @(Get-ChildItem -LiteralPath $testDirectory -Filter "songbird_*_tests.exe" -File -ErrorAction SilentlyContinue |
    Sort-Object Name)
if ($executables.Count -eq 0) {
    Write-Error "No songbird_*_tests.exe under '$testDirectory'. A tree with no test binaries must not report a pass." -ErrorAction Continue
    exit 1
}

# Qt Test writes its report to stdout. Two numbers matter, and only one of them is trustworthy
# on its own:
#
#   "Totals: N passed" counts initTestCase() and cleanupTestCase() as passes. A suite with NO
#   test functions still reports "Totals: 2 passed" -- measured, not assumed: a class with an
#   empty `private slots:` block prints exactly that and exits 0. So the totals line cannot be
#   the signal.
#
#   The per-case "PASS   : Class::slot()" lines can. Counting the ones whose slot name is not one
#   of the four fixtures gives the number of real test functions that ran.
$totalsPattern = [regex]'(?m)^Totals:\s*(\d+)\s+passed'
$passLinePattern = [regex]'(?m)^PASS\s+:\s+\S+::(\w+)\s*\('
$fixtureSlots = @('initTestCase', 'cleanupTestCase', 'init', 'cleanup')

$failures = New-Object System.Collections.Generic.List[string]
$checked = 0

foreach ($executable in $executables) {
    $checked += 1
    $outputFile = Join-Path ([System.IO.Path]::GetTempPath()) ("songbird-testsuite-" + [guid]::NewGuid().ToString("N") + ".txt")

    try {
        $process = Start-Process -FilePath $executable.FullName `
            -ArgumentList @("-o", "$outputFile,txt") `
            -WorkingDirectory $testDirectory `
            -NoNewWindow -PassThru -Wait `
            -ErrorAction Stop

        $text = ""
        if (Test-Path -LiteralPath $outputFile) {
            $text = Get-Content -LiteralPath $outputFile -Raw
        }

        if ($process.ExitCode -ne 0) {
            # A non-zero exit is already a real failure and CTest would report it;
            # the point of naming it here is that the message says *which* binary.
            $failures.Add("$($executable.Name): exited $($process.ExitCode)")
            continue
        }

        $totals = $totalsPattern.Match($text)
        if (-not $totals.Success) {
            $failures.Add("$($executable.Name): no 'Totals:' line -- the binary did not run a test report at all")
            continue
        }

        # A suite whose only passes are fixtures ran no test functions at all.
        $realCases = 0
        foreach ($passLine in $passLinePattern.Matches($text)) {
            if ($fixtureSlots -notcontains $passLine.Groups[1].Value) {
                $realCases += 1
            }
        }

        if ($realCases -le 0) {
            # The whole reason this guard exists. Reported with the totals value so the message
            # itself explains why "2 passed" was not taken as good news.
            $failures.Add(($executable.Name + ": ran 0 test cases (" + $totals.Value.Trim() +
                ") -- Qt Test counts initTestCase/cleanupTestCase as passes, so a suite with no " +
                "test functions still reports success"))
            continue
        }
    } finally {
        if (Test-Path -LiteralPath $outputFile) {
            Remove-Item -LiteralPath $outputFile -Force -ErrorAction SilentlyContinue
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Error (($failures | ForEach-Object { "- $_" }) -join [Environment]::NewLine) -ErrorAction Continue
    exit 1
}

Write-Host ("Test suite check passed: $checked executable(s) each ran at least one test case.")
