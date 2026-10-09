$ErrorActionPreference = 'Stop'

# Verifies the "Install Qt5 (static)" guard by running the step's real run: block against fixtures.
#
# The script is extracted from the workflow rather than retyped, so the parse and the branch
# conditions under test are the ones that will run in CI. Two substitutions are unavoidable and
# are disclosed in the output:
#   1. the ${{ ... }} GitHub expressions become literals (they are not PowerShell);
#   2. the native vcpkg invocation becomes `cmd.exe /c type <payload>`, so the emitted reuse line
#      is controlled. The `2>&1 | Tee-Object -FilePath $log` tail is left intact, which is the
#      part worth exercising: it is what $LASTEXITCODE has to survive.
#
# The generated script is run through pwsh 7 - the shell the step declares - rather than through
# this harness's own host, because the two hosts disagree about the very thing the last case
# tests. Windows PowerShell 5.1 turns a native command's stderr into a *terminating* error when
# $ErrorActionPreference is Stop and the stream is merged with 2>&1; PowerShell 7 does not, because
# $PSNativeCommandUseErrorActionPreference exists there and defaults to $false. Measured on this
# machine: 5.1.19041.1682 throws for `cmd /c 'echo err 1>&2' 2>&1` under Stop, 7.5.4 does not. A
# harness on 5.1 would therefore report a failure the runner can never see.
#
# Running it as a process also means `exit $installExit` is the real exit code, so the exit path is
# covered here instead of only on CI.

# Locate the repository root from this script's own location rather than a hard-coded path, so the
# harness runs from any checkout. It used to name D:\sss\v2rayq, which only worked on one machine.
$repo = $PSScriptRoot
while ($repo -and -not (Test-Path (Join-Path $repo '.github/workflows/release.yml'))) {
    $parent = Split-Path -Parent $repo
    if (-not $parent -or $parent -eq $repo) { $repo = $null; break }
    $repo = $parent
}
if (-not $repo) {
    Write-Output 'cannot locate the repository root (no .github/workflows/release.yml above this script)'
    exit 1
}
$yaml = Get-Content -Raw -Path (Join-Path $repo '.github/workflows/release.yml')

# Between `- name:` and `run:` there are keys (`id:`, `shell:`, `if:`), and they change. Matching
# them explicitly made this harness break the moment `id: qt5` was added to the step.
$m = [regex]::Match($yaml, "(?ms)^      - name: Install Qt5 \(static\).*?\r?\n        run: \|\r?\n(.*?)(?=\r?\n      - name: )")
if (-not $m.Success) { Write-Output 'EXTRACT FAILED'; exit 1 }
$dedented = (($m.Groups[1].Value -split "\r?\n") | ForEach-Object { $_ -replace '^          ', '' }) -join "`n"

$pwshExe = 'C:\Program Files\PowerShell\7\pwsh.exe'
if (-not (Test-Path $pwshExe)) {
    $found = Get-Command pwsh -ErrorAction SilentlyContinue
    if (-not $found) { throw "pwsh 7 not found, so the step's shell: pwsh cannot be emulated" }
    $pwshExe = $found.Source
}

Write-Output "=== HOST / SHELL ==="
Write-Output "harness host : $($PSVersionTable.PSEdition) $($PSVersionTable.PSVersion)"
Write-Output "step shell   : $pwshExe"
Write-Output "native-pref  : $(if (Get-Variable PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) { (Get-Variable PSNativeCommandUseErrorActionPreference).Value } else { '<absent>' })"

# Substitutions must not be re-read as group references, so every one goes through an evaluator.
function Sub {
    param([string]$Text, [string]$Pattern, [string]$Value)
    $ev = [System.Text.RegularExpressions.MatchEvaluator] { param($mm) $Value }
    return [regex]::Replace($Text, $Pattern, $ev)
}

$root = Join-Path $env:TEMP 'vcpkg-guard-verify'
if (Test-Path $root) { Remove-Item -Recurse -Force $root }
New-Item -ItemType Directory -Force -Path $root | Out-Null

# The install step now also records the store's archive set for the prune step to consume. With
# $env:GITHUB_OUTPUT unset, Out-File binds a null -FilePath, which is a parameter-binding error and
# kills the step before it does anything, so every case would fail for the wrong reason.
$env:GITHUB_OUTPUT = Join-Path $root 'github-output.txt'

function New-Store {
    param([string]$Name, [int]$Archives)
    $ws = Join-Path $root $Name
    $d = Join-Path $ws 'vcpkg_cache'
    New-Item -ItemType Directory -Force -Path $d | Out-Null
    for ($i = 1; $i -le $Archives; $i++) {
        New-Item -ItemType File -Force -Path (Join-Path $d (('a' * 60) + ('{0:d4}' -f $i) + '.zip')) | Out-Null
    }
    return $ws
}

function Build-Script {
    param([string]$Store, [string]$TempDir, [string]$Payload, [int]$ExitCode, [switch]$ToStderr)
    $s = $dedented
    $s = Sub $s '\$\{\{\s*steps\.vcpkg-cache\.outputs\.cache-matched-key\s*\}\}' 'FAKE-KEY'
    $s = Sub $s '\$\{\{\s*github\.workspace\s*\}\}' $Store
    $s = Sub $s '\$\{\{\s*runner\.temp\s*\}\}' $TempDir
    $tail = if ($ExitCode -ne 0) { " & exit /b $ExitCode" } else { '' }
    $redir = if ($ToStderr) { ' 1>&2' } else { '' }
    $stub = '& cmd.exe /c ''type "' + $Payload + '"' + $redir + $tail + ''' 2>&1 | Tee-Object -FilePath $log'
    $s = Sub $s '(?s)& "\$env:VCPKG_INSTALLATION_ROOT\\vcpkg\.exe" install .*?Tee-Object -FilePath \$log' $stub
    if ($s -notmatch 'cmd\.exe /c') { throw 'the native invocation was not substituted' }
    # GitHub's pwsh wrapper prepends this, and a fresh process starts with the default 'Continue',
    # so it has to be emulated or every case proves the wrong thing.
    return ("`$ErrorActionPreference = 'Stop'`n" + $s)
}

function Invoke-Step {
    param([string]$Script, [string]$Name)
    $path = Join-Path $root "$Name.ps1"
    Set-Content -Path $path -Value $Script -Encoding utf8
    # *>&1 merges the child's stderr into this 5.1 host, where that merge plus the Stop preference
    # is itself a terminating error - so relax it around the call, or the harness dies before it can
    # report anything. This is a harness concern only; the child runs with the preference it needs.
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $out = $null
    $thrown = $null
    try { $out = & $pwshExe -NoProfile -ExecutionPolicy Bypass -File $path *>&1 }
    catch { $thrown = $_ }
    finally { $ErrorActionPreference = $prev }
    $exit = $LASTEXITCODE
    return [pscustomobject]@{ Name = $Name; Thrown = $thrown; Out = @($out); ExitCode = $exit }
}

function Render {
    param($r)
    $txt = "CASE $($r.Name) exit=$($r.ExitCode)`n"
    if ($r.Thrown) { $txt += "THROWN $($r.Thrown.Exception.GetType().Name): $($r.Thrown.Exception.Message)`n" }
    foreach ($o in $r.Out) { $txt += "  [out] $o`n" }
    return $txt
}

$pass = 0
$fail = 0

function Assert-Case {
    param([string]$Name, [pscustomobject]$Result, [bool]$WantWarn, [string]$WantFragment, [int]$WantExit = -1)
    $ok = $true
    $why = @()
    $text = Render $Result
    if ($Result.Thrown) { $ok = $false; $why += 'harness could not run the step' }
    if ($WantExit -ge 0 -and $Result.ExitCode -ne $WantExit) {
        $ok = $false; $why += "exit $($Result.ExitCode), want $WantExit"
    }
    if ($WantWarn -and ($text -notmatch 'reused none of')) { $ok = $false; $why += 'warning missing' }
    if (-not $WantWarn -and ($text -match 'reused none of')) { $ok = $false; $why += 'warning unexpected' }
    if ($WantFragment -and ($text -notmatch [regex]::Escape($WantFragment))) { $ok = $false; $why += "missing '$WantFragment'" }
    if ($ok) { $script:pass++; Write-Output "PASS  $Name" }
    else { $script:fail++; Write-Output "FAIL  $Name  ($($why -join '; '))"; Write-Output $text }
}

$payload = Join-Path $root 'payload.txt'

Set-Content -Path $payload -Value 'Restored 35 package(s) from D:\x in 53 s. Use --debug to see more details.' -Encoding ascii
$s1 = New-Store -Name 's1' -Archives 35
$r = Invoke-Step -Name 's1' -Script (Build-Script -Store $s1 -TempDir $root -Payload $payload -ExitCode 0)
Assert-Case -Name 'S1 hit: 35/35, no warning' -Result $r -WantWarn $false -WantFragment 'vcpkg reused 35 package(s) from a store of 35 archive(s)' -WantExit 0

Set-Content -Path $payload -Value 'Restored 0 package(s) from D:\x in 425 us. Use --debug to see more details.' -Encoding ascii
$s2 = New-Store -Name 's2' -Archives 68
$r = Invoke-Step -Name 's2' -Script (Build-Script -Store $s2 -TempDir $root -Payload $payload -ExitCode 0)
Assert-Case -Name 'S2 miss: 0/68 -> warning' -Result $r -WantWarn $true -WantFragment 'rebuilds Qt5 from source' -WantExit 0

$s3 = New-Store -Name 's3' -Archives 0
$r = Invoke-Step -Name 's3' -Script (Build-Script -Store $s3 -TempDir $root -Payload $payload -ExitCode 0)
Assert-Case -Name 'S3 empty store: no warning' -Result $r -WantWarn $false -WantFragment 'vcpkg reused 0 package(s) from a store of 0 archive(s)' -WantExit 0

Set-Content -Path $payload -Value 'Installing 1/35 vcpkg-cmake:x64-windows@2025-08-07...' -Encoding ascii
$s4 = New-Store -Name 's4' -Archives 5
$r = Invoke-Step -Name 's4' -Script (Build-Script -Store $s4 -TempDir $root -Payload $payload -ExitCode 0)
Assert-Case -Name 'S4 unparsable: says so, does not fail' -Result $r -WantWarn $false -WantFragment 'printed no reuse line' -WantExit 0

$s5 = New-Store -Name 's5' -Archives 3
$s5cache = Join-Path $s5 'vcpkg_cache'
New-Item -ItemType File -Force -Path (Join-Path $s5cache (('b' * 64) + '.zip.1234')) | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $s5cache 'notahash.zip') | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $s5cache (('c' * 63) + '.zip')) | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $s5cache 'nested') | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $s5cache ('nested\' + ('d' * 64) + '.zip')) | Out-Null
Set-Content -Path $payload -Value 'Restored 4 package(s) from D:\x in 1 s.' -Encoding ascii
$r = Invoke-Step -Name 's5' -Script (Build-Script -Store $s5 -TempDir $root -Payload $payload -ExitCode 0)
Assert-Case -Name 'S5 decoys: counts 4 real, ignores 3 decoys' -Result $r -WantWarn $false -WantFragment 'from a store of 4 archive(s)' -WantExit 0

Set-Content -Path $payload -Value 'Restored 0 package(s) from D:\x in 425 us.' -Encoding ascii
$s6 = New-Store -Name 's6' -Archives 35
$r = Invoke-Step -Name 's6' -Script (Build-Script -Store $s6 -TempDir $root -Payload $payload -ExitCode 7)
Assert-Case -Name 'S6 failing vcpkg: exit 7 survives, still warns' -Result $r -WantWarn $true -WantExit 7

$s7 = New-Store -Name 's7' -Archives 35
$r = Invoke-Step -Name 's7' -Script (Build-Script -Store $s7 -TempDir $root -Payload $payload -ExitCode 0 -ToStderr)
Assert-Case -Name 'S7 stderr from vcpkg does not terminate the merge' -Result $r -WantWarn $true -WantExit 0

Write-Output "=== SUMMARY: $pass passed, $fail failed ==="
# ctest reads the exit code. A run that asserted nothing (a regex that stopped matching the workflow,
# say) must not look like a pass, and neither must one that found a broken case.
if ($pass + $fail -eq 0) { Write-Output 'no checks ran'; exit 1 }
if ($fail -gt 0) { exit 1 }
exit 0
