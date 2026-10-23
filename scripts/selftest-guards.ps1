param(
    [string]$SourceRoot = ""
)

$ErrorActionPreference = "Stop"

# Why this file exists
# --------------------
# A guard that can never fail is worse than no guard: it produces the *appearance*
# of coverage and the confidence that goes with it. The only way to know a check
# is still doing its job is to break what it is supposed to catch and confirm it
# goes red.
#
# This is a self-test for the two static guards in this directory that enforce
# architecture rather than text formatting:
#
#   check-backend-boundaries.ps1     the layering rules
#   check-appbootstrap-member-order.ps1  the destruction-order rule
#
# Every case below mutates a TEMPORARY COPY of the source tree, runs the real
# guard against it, and asserts a non-zero exit. The working tree is never
# touched, because a self-test that can corrupt the tree it is testing will
# eventually do so.
#
# The final case asserts the guards PASS on the unmodified copy, so a guard that
# fails everything unconditionally cannot masquerade as a working check.

function Stop-SelfTestWithError {
    param([string]$Message)

    Write-Error $Message -ErrorAction Continue
    exit 1
}

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $PSScriptRoot "..\src"
}

$sourcePath = (Resolve-Path -LiteralPath $SourceRoot).Path
$scriptRoot = $PSScriptRoot
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("songbird-guard-selftest-" + [guid]::NewGuid().ToString("N"))

# Runs a guard script against a tree and returns its exit code. `2>$null` keeps the
# guard's own Write-Error chatter out of the test output; the exit code is the
# assertion.
function Invoke-Guard {
    param(
        [string]$Guard,
        [string[]]$Arguments
    )

    $allArguments = @("-NoProfile", "-File", (Join-Path $scriptRoot $Guard)) + $Arguments
    $process = Start-Process -FilePath "pwsh" -ArgumentList $allArguments `
        -NoNewWindow -PassThru -Wait -ErrorAction SilentlyContinue `
        -RedirectStandardOutput ([System.IO.Path]::GetTempFileName()) `
        -RedirectStandardError ([System.IO.Path]::GetTempFileName())
    if ($null -eq $process) {
        # pwsh is not on PATH; fall back to the host's own PowerShell.
        $process = Start-Process -FilePath "powershell" -ArgumentList $allArguments `
            -NoNewWindow -PassThru -Wait -ErrorAction SilentlyContinue `
            -RedirectStandardOutput ([System.IO.Path]::GetTempFileName()) `
            -RedirectStandardError ([System.IO.Path]::GetTempFileName())
    }
    if ($null -eq $process) {
        Stop-SelfTestWithError "Could not launch a PowerShell host to run $Guard."
    }
    return $process.ExitCode
}

$cases = @(
    @{
        Name = "backend-boundaries: a service may not include a concrete backend"
        Guard = "check-backend-boundaries.ps1"
        Break = {
            param($tree)
            $target = Join-Path $tree "services\SelftestBackendLeak.cpp"
            Set-Content -LiteralPath $target -Value '#include "backends/xray/XrayCoreDescriptor.h"' -Encoding UTF8
        }
    },
    @{
        Name = "backend-boundaries: a backend may not include the service layer"
        Guard = "check-backend-boundaries.ps1"
        Break = {
            param($tree)
            $target = Join-Path $tree "backends\xray\SelftestUpwardLeak.cpp"
            Set-Content -LiteralPath $target -Value '#include "services/RoutingService.h"' -Encoding UTF8
        }
    },
    @{
        Name = "backend-boundaries: nothing below a front end may include it"
        Guard = "check-backend-boundaries.ps1"
        Break = {
            param($tree)
            $target = Join-Path $tree "appcore\SelftestFrontEndLeak.cpp"
            Set-Content -LiteralPath $target -Value '#include "app/AppBootstrap.h"' -Encoding UTF8
        }
    },
    @{
        Name = "backend-boundaries: an unlisted front end must be reported"
        Guard = "check-backend-boundaries.ps1"
        Break = {
            param($tree)
            # A new front end that is not in the guard's consumer list. Without the
            # coverage rule this directory would simply be skipped, which is exactly
            # how `auto/` escaped the concrete-backend rule once.
            $frontEnd = Join-Path $tree "selftestfrontend"
            New-Item -ItemType Directory -Path $frontEnd -Force | Out-Null
            Set-Content -LiteralPath (Join-Path $frontEnd "main.cpp") -Value 'int main() { return 0; }' -Encoding UTF8
        }
    },
    @{
        Name = "appbootstrap-member-order: a borrower declared before the member it borrows"
        Guard = "check-appbootstrap-member-order.ps1"
        Break = {
            param($tree)
            # SpeedTestCoordinator borrows mainWindow as its QObject parent, so
            # declaring it *above* mainWindow is a double free at shutdown.
            $header = Join-Path $tree "app\AppBootstrapObjects.h"
            $text = Get-Content -LiteralPath $header -Raw
            $borrower = [regex]::Match($text, '(?m)^\s*std::unique_ptr<SpeedTestCoordinator>\s+speedTestCoordinator;\s*$')
            if (-not $borrower.Success) {
                throw "selftest could not locate the speedTestCoordinator declaration; the fixture is stale"
            }
            $text = $text.Remove($borrower.Index, $borrower.Length)
            $anchor = [regex]::Match($text, '(?m)^\s*std::unique_ptr<MainWindow>\s+mainWindow;\s*$')
            if (-not $anchor.Success) {
                throw "selftest could not locate the mainWindow declaration; the fixture is stale"
            }
            # Insert the borrower immediately above its dependency.
            $text = $text.Insert($anchor.Index, $borrower.Value.Trim() + [Environment]::NewLine + "    ")
            Set-Content -LiteralPath $header -Value $text -Encoding UTF8
        }
    },
    @{
        Name = "async-owners: a worker thread lambda capturing this with no guard"
        Guard = "check-async-owners.ps1"
        Break = {
            param($tree)
            $target = Join-Path $tree "appcore\SelftestAsyncLeak.cpp"
            Set-Content -LiteralPath $target -Encoding UTF8 -Value @'
void f(const std::function<void(QThread*)>& track) {
    launchBackgroundThread([this]() { doWork(); }, track);
}
'@
        }
    },
    @{
        Name = "async-owners: an invokeMethod continuation posted elsewhere while capturing this"
        Guard = "check-async-owners.ps1"
        Break = {
            param($tree)
            $target = Join-Path $tree "appcore\SelftestInvokeLeak.cpp"
            Set-Content -LiteralPath $target -Encoding UTF8 -Value @'
void g(QObject* other) {
    QMetaObject::invokeMethod(other, [this]() { touch(); }, Qt::QueuedConnection);
}
'@
        }
    },
    @{
        Name = "hardcoded-colors: a hex colour in a stylesheet literal outside the palette"
        Guard = "check-hardcoded-colors.ps1"
        Break = {
            param($tree)
            $target = Join-Path $tree "ui\SelftestColorLeak.cpp"
            Set-Content -LiteralPath $target -Encoding UTF8 -Value @'
void h() {
    setStyleSheet(QStringLiteral("QWidget { background: #ff0000; }"));
}
'@
        }
    },
    @{
        Name = "hardcoded-colors: a palette file is allowed to name colours"
        Guard = "check-hardcoded-colors.ps1"
        # This case asserts the OPPOSITE direction from every other one: the palette files are
        # exempt, so a tree where only they contain hex must still pass. Running it here means a
        # guard that simply rejects every hex anywhere cannot pass the suite.
        ExpectGreen = $true
        Break = {
            param($tree)
            $target = Join-Path $tree "ui\theme\AppThemePalette.cpp"
            Add-Content -LiteralPath $target -Encoding UTF8 -Value 'const char* kSelftestPaletteColor = "#123456";'
        }
    }
)

$failures = New-Object System.Collections.Generic.List[string]

try {
    foreach ($case in $cases) {
        $tree = Join-Path $tempRoot ([guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path $tree -Force | Out-Null
        Copy-Item -LiteralPath $sourcePath -Destination $tree -Recurse -Force
        $copiedSource = Join-Path $tree (Split-Path -Leaf $sourcePath)

        & $case.Break $copiedSource

        $exitCode = Invoke-Guard -Guard $case.Guard -Arguments @("-SourceRoot", $copiedSource)
        $expectGreen = $case.ContainsKey("ExpectGreen") -and $case.ExpectGreen
        if ($expectGreen) {
            if ($exitCode -ne 0) {
                $failures.Add("$($case.Name): the guard rejected a tree it is supposed to allow")
            } else {
                Write-Host "  green as expected: $($case.Name)"
            }
        } elseif ($exitCode -eq 0) {
            $failures.Add("$($case.Name): the guard still reported success on a tree that violates it")
        } else {
            Write-Host "  red as expected: $($case.Name)"
        }

        Remove-Item -LiteralPath $tree -Recurse -Force -ErrorAction SilentlyContinue
    }

    # The other half: an untouched copy must PASS. Without this, a guard that fails
    # unconditionally would satisfy every case above.
    $cleanTree = Join-Path $tempRoot "clean"
    New-Item -ItemType Directory -Path $cleanTree -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $cleanTree -Recurse -Force
    $cleanSource = Join-Path $cleanTree (Split-Path -Leaf $sourcePath)

    foreach ($guard in @("check-backend-boundaries.ps1", "check-appbootstrap-member-order.ps1", "check-async-owners.ps1", "check-hardcoded-colors.ps1")) {
        $exitCode = Invoke-Guard -Guard $guard -Arguments @("-SourceRoot", $cleanSource)
        if ($exitCode -ne 0) {
            $failures.Add("${guard}: reported a violation in an unmodified copy of the tree")
        } else {
            Write-Host "  green on the clean tree: $guard"
        }
    }
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

# The version check works on CMakeLists.txt rather than on a source tree, so it needs its own fixture
# shape: copy the two build files into a scratch directory and break them there. Without this case
# the check could stop matching (a reworded project() line, a renamed suffix variable) and go on
# reporting a pass against a file it no longer reads.
$versionTempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "songbird-version-selftest-" + [guid]::NewGuid().ToString("N"))
try {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
    New-Item -ItemType Directory -Path $versionTempRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Destination $versionTempRoot
    New-Item -ItemType Directory -Path (Join-Path $versionTempRoot "src") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "src\CMakeLists.txt") `
        -Destination (Join-Path $versionTempRoot "src")
    # The front-end sources too, because one case below mutates main.cpp. They were missing when that
    # case was written, so Get-Content threw "Cannot find path" and the whole self-test died before it
    # reached the assertion -- a case that could not pass, guarding a rule nobody could verify.
    foreach ($frontEnd in @("app", "auto")) {
        New-Item -ItemType Directory -Path (Join-Path $versionTempRoot "src\$frontEnd") -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $repoRoot "src\$frontEnd\main.cpp") `
            -Destination (Join-Path $versionTempRoot "src\$frontEnd")
    }
    New-Item -ItemType Directory -Path (Join-Path $versionTempRoot "scripts") -Force | Out-Null

    $versionCases = @(
        @{
            Name = "version-consistency: a hardcoded version literal in src is rejected"
            Break = {
                param($tree)
                # Read the declared version out of the tree under test rather than spelling it here:
                # a fixture with its own copy of the number would need editing on every bump and
                # would silently stop matching the literal the guard looks for.
                $declared = '1.0.0'
                $rootContent = Get-Content -LiteralPath (Join-Path $tree "CMakeLists.txt") -Raw
                if ($rootContent -match 'project\s*\(\s*SongBird\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
                    $declared = $Matches[1]
                }
                Set-Content -LiteralPath (Join-Path $tree "src\VersionLiteral.h") `
                    -Value "inline constexpr const char* kVersion = `"$declared`";" -Encoding UTF8
            }
        },
        @{
            Name = "version-consistency: a display version that does not compose from project(VERSION) is rejected"
            Break = {
                param($tree)
                $path = Join-Path $tree "CMakeLists.txt"
                $content = Get-Content -LiteralPath $path -Raw
                $content = $content -replace 'set\s*\(\s*SONGBIRD_DISPLAY_VERSION\s+"[^"]*"\s*\)', 'set(SONGBIRD_DISPLAY_VERSION "9.9.9")'
                Set-Content -LiteralPath $path -Value $content -Encoding UTF8 -NoNewline
            }
        },
        @{
            Name = "version-consistency: a release tag that disagrees with project(VERSION) is rejected"
            ExtraArguments = @("-Tag", "v999.0.0")
            Break = { param($tree) }
        },
        @{
            Name = "version-consistency: a plausible version fallback in main.cpp is rejected"
            # The reason this case exists: the literal scan compares against project(VERSION), so a
            # fallback spelling an *older* release was invisible to it -- and both main.cpp files
            # carried one for two versions. A number is the shape of the bug, so the fixture is a
            # number that is not the declared version.
            Break = {
                param($tree)
                $target = Join-Path $tree "src\app\main.cpp"
                if (-not (Test-Path -LiteralPath $target)) {
                    throw "selftest fixture is stale: '$target' is not in the sandbox"
                }
                $content = Get-Content -LiteralPath $target -Raw
                $updated = $content -replace '#define\s+SONGBIRD_APP_VERSION\s+"[^"]*"', '#define SONGBIRD_APP_VERSION "9.9.9"'
                # A -replace that matches nothing returns the input unchanged, and the case would then
                # pass on the strength of the *other* literal-scan half of the guard. Assert the
                # mutation landed, so a renamed macro reports "fixture stale" instead of going green.
                if ($updated -eq $content) {
                    throw "selftest fixture is stale: no SONGBIRD_APP_VERSION define matched in '$target'"
                }
                Set-Content -LiteralPath $target -Value $updated -Encoding UTF8 -NoNewline
            }
        }
    )

    foreach ($case in $versionCases) {
        $tree = Join-Path $versionTempRoot ([guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path $tree -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $versionTempRoot "CMakeLists.txt") -Destination $tree
        Copy-Item -LiteralPath (Join-Path $versionTempRoot "src") -Destination $tree -Recurse
        Copy-Item -LiteralPath (Join-Path $versionTempRoot "scripts") -Destination $tree -Recurse

        & $case.Break $tree

        $arguments = @(
            "-CMakeListsPath", (Join-Path $tree "CMakeLists.txt"),
            "-SrcCMakeListsPath", (Join-Path $tree "src\CMakeLists.txt"))
        if ($case.ContainsKey("ExtraArguments")) {
            $arguments += $case.ExtraArguments
        }

        $exitCode = Invoke-Guard -Guard "check-version-consistency.ps1" -Arguments $arguments
        if ($exitCode -eq 0) {
            $failures.Add("$($case.Name): the guard still reported success on a tree that violates it")
        } else {
            Write-Host "  red as expected: $($case.Name)"
        }

        Remove-Item -LiteralPath $tree -Recurse -Force -ErrorAction SilentlyContinue
    }

    # And the untouched copy must pass, so a guard that always fails cannot satisfy the cases above.
    $cleanVersionTree = Join-Path $versionTempRoot "clean"
    New-Item -ItemType Directory -Path $cleanVersionTree -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $versionTempRoot "CMakeLists.txt") -Destination $cleanVersionTree
    Copy-Item -LiteralPath (Join-Path $versionTempRoot "src") -Destination $cleanVersionTree -Recurse
    Copy-Item -LiteralPath (Join-Path $versionTempRoot "scripts") -Destination $cleanVersionTree -Recurse
    $cleanExit = Invoke-Guard -Guard "check-version-consistency.ps1" -Arguments @(
        "-CMakeListsPath", (Join-Path $cleanVersionTree "CMakeLists.txt"),
        "-SrcCMakeListsPath", (Join-Path $cleanVersionTree "src\CMakeLists.txt"))
    if ($cleanExit -ne 0) {
        $failures.Add("check-version-consistency.ps1: reported a violation in an unmodified copy of the build files")
    } else {
        Write-Host "  green on the clean tree: check-version-consistency.ps1"
    }
} finally {
    Remove-Item -LiteralPath $versionTempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

# The README asset check needs a README and a docs directory, so it gets its own scratch tree for the
# same reason the version check does: the working tree must never be mutated by a self-test.
$readmeTempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "songbird-readme-selftest-" + [guid]::NewGuid().ToString("N"))
try {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
    New-Item -ItemType Directory -Path (Join-Path $readmeTempRoot "docs\screenshots") -Force | Out-Null
    # A real, non-empty file so the passing case exercises the same path a genuine screenshot would.
    Set-Content -LiteralPath (Join-Path $readmeTempRoot "docs\screenshots\shot.png") `
        -Value "not a real png, but non-empty" -Encoding UTF8

    # Each case names the README file it writes, so a case can target the translated page as well as
    # the English one -- the translated pages share the screenshots and would otherwise have no
    # protection of their own.
    $readmeCases = @(
        @{
            Name = "readme-assets: a reference to a missing image is rejected"
            File = "README.md"
            Content = "# Title`n`n![shot](docs/screenshots/missing.png)`n"
        },
        @{
            Name = "readme-assets: a reference to an empty file is rejected"
            File = "README.md"
            Break = {
                param($tree)
                Set-Content -LiteralPath (Join-Path $tree "docs\screenshots\empty.png") -Value "" -NoNewline
            }
            Content = "# Title`n`n![shot](docs/screenshots/empty.png)`n"
        },
        @{
            Name = "readme-assets: a broken image on a translated page is rejected"
            File = "README_zh_CN.md"
            Content = "# Title`n`n![shot](docs/screenshots/missing.png)`n"
        }
    )

    foreach ($case in $readmeCases) {
        $tree = Join-Path $readmeTempRoot ([guid]::NewGuid().ToString("N"))
        New-Item -ItemType Directory -Path (Join-Path $tree "docs\screenshots") -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $readmeTempRoot "docs\screenshots\shot.png") `
            -Destination (Join-Path $tree "docs\screenshots")
        if ($case.ContainsKey("Break")) {
            & $case.Break $tree
        }
        Set-Content -LiteralPath (Join-Path $tree $case.File) -Value $case.Content -Encoding UTF8

        $exitCode = Invoke-Guard -Guard "check-readme-assets.ps1" -Arguments @("-Root", $tree)
        if ($exitCode -eq 0) {
            $failures.Add("$($case.Name): the guard still reported success on a README that violates it")
        } else {
            Write-Host "  red as expected: $($case.Name)"
        }

        Remove-Item -LiteralPath $tree -Recurse -Force -ErrorAction SilentlyContinue
    }

    # And a tree whose images all resolve must pass, so a guard that always fails cannot satisfy the
    # cases above.
    $cleanReadmeTree = Join-Path $readmeTempRoot "clean"
    New-Item -ItemType Directory -Path (Join-Path $cleanReadmeTree "docs\screenshots") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $readmeTempRoot "docs\screenshots\shot.png") `
        -Destination (Join-Path $cleanReadmeTree "docs\screenshots")
    Set-Content -LiteralPath (Join-Path $cleanReadmeTree "README.md") `
        -Value "# Title`n`n![shot](docs/screenshots/shot.png)`n" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $cleanReadmeTree "README_zh_CN.md") `
        -Value "# Title`n`n![shot](docs/screenshots/shot.png)`n" -Encoding UTF8
    $cleanReadmeExit = Invoke-Guard -Guard "check-readme-assets.ps1" -Arguments @("-Root", $cleanReadmeTree)
    if ($cleanReadmeExit -ne 0) {
        $failures.Add("check-readme-assets.ps1: reported a violation in a README whose images all resolve")
    } else {
        Write-Host "  green on the clean tree: check-readme-assets.ps1"
    }
} finally {
    Remove-Item -LiteralPath $readmeTempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    Write-Error (($failures | ForEach-Object { "- $_" }) -join [Environment]::NewLine) -ErrorAction Continue
    exit 1
}

Write-Host "Guard self-test passed: every rule went red when broken, and green when intact."
