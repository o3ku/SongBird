$ErrorActionPreference = 'Stop'

# Verifies the runner-image namespace on the vcpkg cache keys.
#
# Two kinds of check, and they are NOT equally strong - the output says which is which:
#   * the run: block of "Read runner image version" is extracted from the workflow and executed for
#     real under pwsh 7 (the shell the step declares), so the `image=<value>` line it feeds the
#     restore step is measured rather than assumed;
#   * the key / restore-keys composition is asserted by reading the workflow text. Which cache
#     GitHub actually selects for a prefix is GitHub's behaviour and cannot be executed here, so
#     those assertions only pin the shape that the selection depends on. They would not catch a
#     wrong belief about the selection rule itself.

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

# Same generic form as the other two harnesses: between `- name:` and `run:` there are keys
# (`id:`, `shell:`, `if:`), and they change.
$m = [regex]::Match($yaml, "(?ms)^      - name: Read runner image version.*?\r?\n        run: \|\r?\n(.*?)(?=\r?\n      - name: )")
if (-not $m.Success) { Write-Output 'EXTRACT FAILED'; exit 1 }
$dedented = (($m.Groups[1].Value -split "\r?\n") | ForEach-Object { $_ -replace '^          ', '' }) -join "`n"

# The run block is executed verbatim, so a GitHub expression inside it would be executed as
# literal text and every case below would silently prove the wrong thing.
if ($dedented -match '\$\{\{') { throw 'the extracted run block contains a ${{ }} expression, which cannot be executed' }

$pwshExe = 'C:\Program Files\PowerShell\7\pwsh.exe'
if (-not (Test-Path $pwshExe)) {
    $found = Get-Command pwsh -ErrorAction SilentlyContinue
    if (-not $found) { throw "pwsh 7 not found, so the step's shell: pwsh cannot be emulated" }
    $pwshExe = $found.Source
}

Write-Output "=== HOST / SHELL ==="
Write-Output "harness host : $($PSVersionTable.PSEdition) $($PSVersionTable.PSVersion)"
Write-Output "step shell   : $pwshExe"
Write-Output ''
Write-Output "=== EXTRACTED run: BLOCK (Read runner image version) ==="
Write-Output $dedented
Write-Output ''

$root = Join-Path $env:TEMP 'vcpkg-image-verify'
if (Test-Path $root) { Remove-Item -Recurse -Force $root }
New-Item -ItemType Directory -Force -Path $root | Out-Null

$pass = 0
$fail = 0

function Invoke-Step {
    param([string]$Name, [string]$ImageVersion, [string]$OutputFile, [switch]$Unset)
    $script = "`$ErrorActionPreference = 'Stop'`n" + $dedented
    $path = Join-Path $root "$Name.ps1"
    Set-Content -Path $path -Value $script -Encoding utf8
    # The step reads the variable from its environment, so the fixture has to live there. Removing
    # it (rather than assigning '') is the case that mirrors a non-hosted runner.
    if ($Unset) { Remove-Item Env:ImageVersion -ErrorAction SilentlyContinue }
    else { $env:ImageVersion = $ImageVersion }
    $env:GITHUB_OUTPUT = $OutputFile
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

function Assert-Behaviour {
    param([string]$Name, [pscustomobject]$Result, [string]$OutputFile, [string]$WantLine, [int]$WantLineCount = 1)
    $ok = $true
    $why = @()
    $text = Render $Result
    if ($Result.Thrown) { $ok = $false; $why += 'harness could not run the step' }
    if ($Result.ExitCode -ne 0) { $ok = $false; $why += "exit $($Result.ExitCode), want 0" }
    $lines = @()
    if (Test-Path $OutputFile) { $lines = @(Get-Content -Path $OutputFile) }
    # A [string] parameter that the caller omits is '' rather than $null, so this has to test
    # truthiness: comparing against the default would demand an empty line in the file.
    if ($WantLine -and ($lines -notcontains $WantLine)) { $ok = $false; $why += "GITHUB_OUTPUT lacks '$WantLine'" }
    if ($lines.Count -ne $WantLineCount) { $ok = $false; $why += "GITHUB_OUTPUT has $($lines.Count) line(s), want $WantLineCount" }
    if ($ok) { $script:pass++; Write-Output "PASS  $Name" }
    else { $script:fail++; Write-Output "FAIL  $Name  ($($why -join '; '))"; Write-Output $text; Write-Output ("  [output file] " + (($lines -join ' | '))) }
}

function Assert-Structural {
    param([string]$Name, [bool]$Condition, [string]$Detail)
    if ($Condition) { $script:pass++; Write-Output "PASS  $Name" }
    else { $script:fail++; Write-Output "FAIL  $Name  ($Detail)" }
}

# ---------------------------------------------------------------- behaviour

$o1 = Join-Path $root 'out-b1.txt'
$r = Invoke-Step -Name 'b1' -ImageVersion '20261004.326.1' -OutputFile $o1
Assert-Behaviour -Name 'B1 image version set -> image=<value>' -Result $r -OutputFile $o1 -WantLine 'image=20261004.326.1'

$o2 = Join-Path $root 'out-b2.txt'
$r = Invoke-Step -Name 'b2' -Unset -OutputFile $o2
Assert-Behaviour -Name 'B2 variable absent -> image=unknown' -Result $r -OutputFile $o2 -WantLine 'image=unknown'

$o3 = Join-Path $root 'out-b3.txt'
$r = Invoke-Step -Name 'b3' -ImageVersion '' -OutputFile $o3
Assert-Behaviour -Name 'B3 variable empty -> image=unknown' -Result $r -OutputFile $o3 -WantLine 'image=unknown'

# -Append is what lets several steps share one output file; without it this step would truncate
# whatever ran before it. Two runs into one file is the cheapest way to see it.
$o4 = Join-Path $root 'out-b4.txt'
$r = Invoke-Step -Name 'b4a' -ImageVersion '20260927.320.1' -OutputFile $o4
Assert-Behaviour -Name 'B4a first write' -Result $r -OutputFile $o4 -WantLine 'image=20260927.320.1'
$r = Invoke-Step -Name 'b4b' -ImageVersion '20261004.326.1' -OutputFile $o4
Assert-Behaviour -Name 'B4b second write appends, does not truncate' -Result $r -OutputFile $o4 -WantLineCount 2
$kept = @(Get-Content -Path $o4)
Assert-Structural -Name 'B4c both lines survive' -Condition ($kept -contains 'image=20260927.320.1' -and $kept -contains 'image=20261004.326.1') -Detail ($kept -join ' | ')

# ---------------------------------------------------------------- structure

$restore = [regex]::Match($yaml, "(?ms)^      - name: Restore vcpkg cache.*?(?=\r?\n      - name: )").Value
$save = [regex]::Match($yaml, "(?ms)^      - name: Save vcpkg cache.*?(?=\r?\n      - name: |\z)").Value
if (-not $restore -or -not $save) { throw 'could not isolate the restore/save steps' }

$restoreKey = [regex]::Match($restore, "(?m)^\s+key: (.+)$").Groups[1].Value.Trim()
$saveKey = [regex]::Match($save, "(?m)^\s+key: (.+)$").Groups[1].Value.Trim()
$rkBlock = [regex]::Match($restore, "(?ms)^\s+restore-keys: \|\r?\n((?:[ \t]+.*\r?\n?)+)").Groups[1].Value
$rk = @(($rkBlock -split "\r?\n") | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })

Write-Output ''
Write-Output '=== KEY SHAPE (informational; selection itself is GitHub''s) ==='
Write-Output "restore key      : $restoreKey"
Write-Output "restore-keys     : $($rk[0])"
Write-Output "                   $($rk[1])"
Write-Output "save key         : $saveKey"
Write-Output ''

Assert-Structural -Name 'S1 the image step is declared before the restore step' `
    -Condition ($yaml.IndexOf('- name: Read runner image version') -lt $yaml.IndexOf('- name: Restore vcpkg cache')) `
    -Detail 'image step must run first or steps.image.outputs.image is empty'

Assert-Structural -Name 'S2 restore key is namespaced by image and still carries the run id' `
    -Condition ($restoreKey -match 'steps\.image\.outputs\.image' -and $restoreKey -match 'github\.run_id') `
    -Detail $restoreKey

Assert-Structural -Name 'S3 restore-keys has exactly two entries, scoped first then unscoped' `
    -Condition ($rk.Count -eq 2 -and $rk[0] -match 'steps\.image\.outputs\.image' -and $rk[1] -notmatch 'steps\.image\.outputs\.image') `
    -Detail ($rk -join ' || ')

Assert-Structural -Name 'S4 save key is the restore key plus the attempt number' `
    -Condition ($saveKey -eq ($restoreKey + '-${{ github.run_attempt }}')) `
    -Detail "save=$saveKey restore=$restoreKey"

Assert-Structural -Name 'S5 the unscoped fallback is a prefix of the scoped one' `
    -Condition ($rk[0].StartsWith($rk[1])) `
    -Detail "scoped=$($rk[0]) unscoped=$($rk[1])"

# The transition: caches written before this change are keyed without an image version. If the
# fallback stopped being exactly that legacy prefix, the first run after the change would find
# nothing and rebuild Qt5 from source.
Assert-Structural -Name 'S6 the unscoped fallback still matches pre-namespace saves' `
    -Condition ($rk[1] -eq 'vcpkg-qt5-${{ env.VCPKG_TRIPLET }}-${{ runner.os }}-') `
    -Detail $rk[1]

Write-Output ''
Write-Output "=== SUMMARY: $pass passed, $fail failed ==="
# ctest reads the exit code. A run that asserted nothing (a regex that stopped matching the workflow,
# say) must not look like a pass, and neither must one that found a broken case.
if ($pass + $fail -eq 0) { Write-Output 'no checks ran'; exit 1 }
if ($fail -gt 0) { exit 1 }
exit 0
