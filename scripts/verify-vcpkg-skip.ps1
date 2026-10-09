$ErrorActionPreference = 'Stop'

# Verifies the "Prune vcpkg cache to this run's packages" step's skip verdict by running the step's
# real run: block against fixtures.
#
# The script is extracted from the workflow rather than retyped, so the set comparison under test is
# the one that will run in CI. Substitutions, all disclosed in the output:
#   1. the ${{ ... }} GitHub expressions become literals (they are not PowerShell);
#   2. $env:VCPKG_INSTALLATION_ROOT points at a fixture root holding a hand-written installed/vcpkg/status;
#   3. $env:GITHUB_OUTPUT points at a temp file, so the `skip=` line can be read back.
#
# What each case pins down is the *verdict* (skip true/false) plus what survived in the store. The
# verdict is what decides whether the save step re-uploads ~1 GB, so a wrong one is either a wasted
# upload every run or - far worse - a store that never gets saved.

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

$m = [regex]::Match($yaml, "(?ms)^      - name: Prune vcpkg cache to this run's packages.*?\r?\n        run: \|\r?\n(.*?)(?=\r?\n      - name: )")
if (-not $m.Success) { Write-Output 'EXTRACT FAILED'; exit 1 }
$dedented = (($m.Groups[1].Value -split "\r?\n") | ForEach-Object { $_ -replace '^          ', '' }) -join "`n"

$pwshExe = 'C:\Program Files\PowerShell\7\pwsh.exe'
if (-not (Test-Path $pwshExe)) {
    $found = Get-Command pwsh -ErrorAction SilentlyContinue
    if (-not $found) { throw "pwsh 7 not found, so the step's shell: pwsh cannot be emulated" }
    $pwshExe = $found.Source
}

Write-Output "extracted $((($dedented -split "`n").Count)) line(s) from the workflow"
Write-Output "step shell: $pwshExe (harness host $($PSVersionTable.PSEdition) $($PSVersionTable.PSVersion))"

# Substitutions must not be re-read as group references, so every one goes through an evaluator.
function Sub {
    param([string]$Text, [string]$Pattern, [string]$Value)
    $ev = [System.Text.RegularExpressions.MatchEvaluator] { param($mm) $Value }
    return [regex]::Replace($Text, $Pattern, $ev)
}

$root = Join-Path $env:TEMP 'vcpkg-skip-verify'
if (Test-Path $root) { Remove-Item -Recurse -Force $root }
New-Item -ItemType Directory -Force -Path $root | Out-Null

# A vcpkg archive name is the package's ABI hash: 64 lowercase hex characters.
function Abi {
    param([string]$Prefix, [int]$Index)
    $name = ($Prefix * 60) + ('{0:d4}' -f $Index)
    if ($name.Length -ne 64) { throw "fixture bug: '$name' is $($name.Length) chars" }
    return $name
}
function AbiRange {
    param([string]$Prefix, [int]$Count)
    return @(1..$Count | ForEach-Object { Abi -Prefix $Prefix -Index $_ })
}

$storeFilter = '^[0-9a-f]{64}\.zip$'
function New-Case {
    # $AfterInstall and $StatusAbis are base names - the '.zip' suffix is appended here, so a decoy
    # like 'notahash' lands on disk as 'notahash.zip'.
    param([string]$Name, [string[]]$AfterInstall, [string[]]$StatusAbis, [string]$Before, [switch]$NoStatus)
    $ws = Join-Path $root $Name
    $store = Join-Path $ws 'vcpkg_cache'
    New-Item -ItemType Directory -Force -Path $store | Out-Null
    foreach ($a in $AfterInstall) {
        New-Item -ItemType File -Force -Path (Join-Path $store "$a.zip") | Out-Null
    }
    $vroot = Join-Path $ws 'vcpkgroot'
    if (-not $NoStatus) {
        $dir = Join-Path $vroot 'installed/vcpkg'
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        # Shaped like the real file: stanzas carry Abi, the feature pseudo-packages do not.
        $lines = @()
        $i = 0
        foreach ($a in $StatusAbis) {
            $i++
            $lines += "Package: fixture$i"
            $lines += 'Version: 1.0'
            $lines += 'Architecture: x64-windows-static-md'
            $lines += "Abi: $a"
            $lines += ''
            $lines += "Feature: fixture$i-core"
            $lines += 'Architecture: x64-windows-static-md'
            $lines += ''
        }
        # ascii, not utf8: Set-Content -Encoding utf8 writes a BOM, which would break a
        # '(?m)^Abi: ' match on the very first line.
        Set-Content -Path (Join-Path $dir 'status') -Value $lines -Encoding ascii
    }
    return [pscustomobject]@{ Name = $Name; Workspace = $ws; Store = $store; VcpkgRoot = $vroot }
}

$outFile = Join-Path $root 'github-output.txt'

function Invoke-Prune {
    param([pscustomobject]$Case, [string]$Before)
    $s = $dedented
    $s = Sub $s '\$\{\{\s*github\.workspace\s*\}\}' $Case.Workspace
    $s = Sub $s '\$\{\{\s*steps\.qt5\.outputs\.store-before\s*\}\}' $Before
    if ($s -match '\$\{\{') { throw 'a GitHub expression was left unsubstituted' }
    $path = Join-Path $root "$($Case.Name).ps1"
    # GitHub's pwsh wrapper prepends this, and a fresh process starts with the default 'Continue'.
    Set-Content -Path $path -Value ("`$ErrorActionPreference = 'Stop'`n" + $s) -Encoding utf8
    if (Test-Path $outFile) { Remove-Item -Force $outFile }

    # $env: is inherited by the child process, which is how the step's two environment inputs arrive.
    $prevVcpkg = $env:VCPKG_INSTALLATION_ROOT
    $prevOut = $env:GITHUB_OUTPUT
    $env:VCPKG_INSTALLATION_ROOT = $Case.VcpkgRoot
    $env:GITHUB_OUTPUT = $outFile
    # Run it on the shell the step declares, and as a process so `exit 0` is a real exit code.
    # *>&1 merges the child's stderr into this 5.1 host, where that merge plus the Stop preference
    # is itself a terminating error - a harness concern, so relax it around the call only.
    $prevPref = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $out = $null
    $thrown = $null
    try { $out = & $pwshExe -NoProfile -ExecutionPolicy Bypass -File $path *>&1 }
    catch { $thrown = $_ }
    finally { $ErrorActionPreference = $prevPref }
    $exit = $LASTEXITCODE
    $env:VCPKG_INSTALLATION_ROOT = $prevVcpkg
    $env:GITHUB_OUTPUT = $prevOut

    $skip = $null
    if (Test-Path $outFile) {
        $line = [regex]::Match((Get-Content -Raw $outFile), '(?m)^skip=(.*)$')
        if ($line.Success) { $skip = $line.Groups[1].Value.Trim() }
    }
    return [pscustomobject]@{
        Case = $Case; Skip = $skip; Out = @($out); Thrown = $thrown; ExitCode = $exit
    }
}

function Get-StoreArchives {
    param([string]$Store)
    return @(Get-ChildItem -Path $Store -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match $storeFilter } |
        ForEach-Object { $_.BaseName } | Sort-Object)
}
function Get-StoreFiles {
    param([string]$Store)
    return @(Get-ChildItem -Path $Store -Recurse -File -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Name } | Sort-Object)
}

$pass = 0
$fail = 0

function Assert-Case {
    param(
        [string]$Name, [pscustomobject]$Result,
        [string]$WantSkip, [string]$WantWarn, [int]$WantArchiveCount = -1, [string[]]$WantArchives,
        [string[]]$WantFiles, [string[]]$AbsentFiles
    )
    $ok = $true
    $why = @()
    $text = "CASE $Name exit=$($Result.ExitCode) skip=$($Result.Skip)`n"
    if ($Result.Thrown) { $text += "THROWN $($Result.Thrown.Exception.GetType().Name): $($Result.Thrown.Exception.Message)`n" }
    foreach ($o in $Result.Out) { $text += "  [out] $o`n" }
    if ($Result.Thrown) { $ok = $false; $why += 'harness could not run the step' }
    # Every path through this step is a success path, including the one that gives up on the prune
    # and exits early, so a non-zero exit is a failure however the verdict came out.
    if ($Result.ExitCode -ne 0) { $ok = $false; $why += "exit $($Result.ExitCode), want 0" }
    if ($WantSkip -ne $Result.Skip) { $ok = $false; $why += "skip='$($Result.Skip)' want '$WantSkip'" }
    if ($WantWarn) {
        if ($text -notmatch [regex]::Escape($WantWarn)) { $ok = $false; $why += "missing '$WantWarn'" }
    }
    $archives = Get-StoreArchives -Store $Result.Case.Store
    if ($WantArchiveCount -ge 0 -and $archives.Count -ne $WantArchiveCount) {
        $ok = $false; $why += "store has $($archives.Count) archive(s), want $WantArchiveCount"
    }
    if ($WantArchives -ne $null) {
        $diff = @(Compare-Object -ReferenceObject ($WantArchives | Sort-Object) -DifferenceObject $archives)
        if ($diff.Count -ne 0) { $ok = $false; $why += "archive set differs by $($diff.Count)" }
    }
    $files = Get-StoreFiles -Store $Result.Case.Store
    # Guard on truthiness rather than wrapping in @(): @($null).Count is 1, not 0, so an omitted
    # [string[]] parameter would iterate once with $f = $null and report "file '' is gone" for every
    # case that does not pass the list.
    if ($WantFiles) {
        foreach ($f in $WantFiles) {
            if ($files -notcontains $f) { $ok = $false; $why += "file '$f' is gone" }
        }
    }
    if ($AbsentFiles) {
        foreach ($f in $AbsentFiles) {
            if ($files -contains $f) { $ok = $false; $why += "file '$f' survived" }
        }
    }
    if ($ok) { $script:pass++; Write-Output "PASS  $Name" }
    else { $script:fail++; Write-Output "FAIL  $Name  ($($why -join '; '))"; Write-Output $text }
}

$old = AbiRange -Prefix 'a' -Count 35
$new = AbiRange -Prefix 'b' -Count 35

# C1: the steady state after the restore fix - every archive reused, nothing pruned. Re-uploading
# would be a ~1 GB no-op, so the save must be skipped.
$c = New-Case -Name 'c1-full-hit' -AfterInstall $old -StatusAbis $old -Before ($old -join ',')
$r = Invoke-Prune -Case $c -Before ($old -join ',')
Assert-Case -Name 'C1 full hit, store identical -> skip' -Result $r -WantSkip 'true' -WantArchiveCount 35

# C2: a runner-image drift. vcpkg rebuilt all 35 under new hashes; the store holds both generations
# until the prune drops the old one. The store changed even though the count never left 35 - the
# exact case a count would call "unchanged".
$c = New-Case -Name 'c2-drift' -AfterInstall ($old + $new) -StatusAbis $new -Before ($old -join ',')
$r = Invoke-Prune -Case $c -Before ($old -join ',')
Assert-Case -Name 'C2 full drift, 35 -> 35 new names -> save' -Result $r -WantSkip 'false' -WantArchives $new

# C3: cold cache. Nothing to restore, vcpkg built everything. Must save, or every run stays cold.
$c = New-Case -Name 'c3-cold' -AfterInstall $old -StatusAbis $old -Before ''
$r = Invoke-Prune -Case $c -Before ''
Assert-Case -Name 'C3 cold build, empty before -> save' -Result $r -WantSkip 'false' -WantArchives $old

# C4: nothing was restored and nothing was built (vcpkg failed early, or a dry run). An empty store
# is not worth an entry.
$c = New-Case -Name 'c4-nothing' -AfterInstall @() -StatusAbis $old -Before ''
$r = Invoke-Prune -Case $c -Before ''
Assert-Case -Name 'C4 empty store, empty before -> skip' -Result $r -WantSkip 'true' -WantArchiveCount 0

# C5: the status file cannot be read, so the prune refuses to run. It must not guess "unchanged",
# because the store may have grown, and it must not have deleted anything.
$c = New-Case -Name 'c5-nostatus' -AfterInstall $old -StatusAbis @() -Before ($old -join ',') -NoStatus
$r = Invoke-Prune -Case $c -Before ($old -join ',')
Assert-Case -Name 'C5 unreadable status -> warn, save, store intact' -Result $r -WantSkip 'false' `
    -WantWarn 'skipping the prune' -WantArchives $old

# C6: the before set arrives in a different order than the directory listing returns it. The
# comparison has to be order-insensitive or every run re-uploads the store.
$reversed = @($old[($old.Count - 1)..0])
$c = New-Case -Name 'c6-order' -AfterInstall $old -StatusAbis $old -Before ($reversed -join ',')
$r = Invoke-Prune -Case $c -Before ($reversed -join ',')
Assert-Case -Name 'C6 before set out of order -> still skip' -Result $r -WantSkip 'true' -WantArchiveCount 35

# C7: three packages drifted. The store holds 38 archives, the prune drops the 3 superseded ones, and
# the verdict must be "save" - the new names are not in the cache yet.
$partial = @(AbiRange -Prefix 'a' -Count 32) + @(AbiRange -Prefix 'b' -Count 3)
$c = New-Case -Name 'c7-partial' -AfterInstall ($old + @(AbiRange -Prefix 'b' -Count 3)) `
    -StatusAbis $partial -Before ($old -join ',')
$r = Invoke-Prune -Case $c -Before ($old -join ',')
Assert-Case -Name 'C7 partial drift -> save' -Result $r -WantSkip 'false' -WantArchives $partial

# C8: files that are not archives, and a half-uploaded temp file. None of them belong to the set, so
# none may flip the verdict, and the temp file is the one thing that should be cleaned up.
$c = New-Case -Name 'c8-decoys' -AfterInstall ($old + @('notahash', ('c' * 63))) `
    -StatusAbis $old -Before ($old -join ',')
New-Item -ItemType File -Force -Path (Join-Path $c.Store "$($old[0]).zip.7777") | Out-Null
$r = Invoke-Prune -Case $c -Before ($old -join ',')
Assert-Case -Name 'C8 decoys and temp file -> verdict unchanged, temp cleaned' -Result $r -WantSkip 'true' `
    -WantArchiveCount 35 -WantFiles @('notahash.zip', (('c' * 63) + '.zip')) `
    -AbsentFiles @("$($old[0]).zip.7777")

# C9: the before set names an archive that is no longer on disk. The store differs from what we
# inherited, so it gets saved - saving a subset is harmless, skipping is not.
$short = @($old[0..33])
$c = New-Case -Name 'c9-shrunk' -AfterInstall $short -StatusAbis $short -Before ($old -join ',')
$r = Invoke-Prune -Case $c -Before ($old -join ',')
Assert-Case -Name 'C9 store lost an archive -> save' -Result $r -WantSkip 'false' -WantArchives $short

Write-Output "=== SUMMARY: $pass passed, $fail failed ==="
# ctest reads the exit code. A run that asserted nothing (a regex that stopped matching the workflow,
# say) must not look like a pass, and neither must one that found a broken case.
if ($pass + $fail -eq 0) { Write-Output 'no checks ran'; exit 1 }
if ($fail -gt 0) { exit 1 }
exit 0
