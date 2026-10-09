# Harness for the "Evict superseded vcpkg caches" step in .github/workflows/release.yml.
#
# It extracts the step's real run block from the workflow (never a retyped copy),
# substitutes the GitHub expressions, and runs it as a pwsh 7 process against a
# canned cache list.
#
# Emulated rather than measured, and stated here so the output is not over-trusted:
#   - `gh` is a PowerShell *function* shim (this sandbox cannot spawn cmd.exe, so a
#     gh.cmd shim on PATH is not an option). The shim serves the canned JSON and
#     records DELETE calls, so the parsing, grouping, staleness and selection logic
#     is the step's own; the process boundary and gh's own argument handling are not.
#   - GitHub's own cache eviction, and which entry `actions/cache/restore` picks,
#     are platform behaviour. Only the shape they depend on is asserted, and those
#     checks are labelled STRUCTURAL in the output.
#
# Safety: the substituted script can only ever talk to the shim. The repo slug is
# rewritten to harness/repo, GH_TOKEN is set to a value the real API rejects, and
# the run fails loudly if any `o3ku` reference survives. Nothing here can delete a
# real cache entry.

$ErrorActionPreference = 'Stop'

# Locate the repository root from this script's own location rather than a hard-coded path, so the
# harness runs from any checkout. It used to name D:\sss\v2rayq, which only worked on one machine.
$repoRoot = $PSScriptRoot
while ($repoRoot -and -not (Test-Path (Join-Path $repoRoot '.github/workflows/release.yml'))) {
  $parent = Split-Path -Parent $repoRoot
  if (-not $parent -or $parent -eq $repoRoot) { $repoRoot = $null; break }
  $repoRoot = $parent
}
if (-not $repoRoot) {
  Write-Output 'cannot locate the repository root (no .github/workflows/release.yml above this script)'
  exit 1
}
$workflowPath = Join-Path $repoRoot '.github/workflows/release.yml'
$work = Join-Path $env:TEMP 'verify-vcpkg-evict'
if (Test-Path $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Item -ItemType Directory -Force -Path $work | Out-Null

$pwshExe = 'C:\Program Files\PowerShell\7\pwsh.exe'
if (-not (Test-Path $pwshExe)) {
  $found = Get-Command pwsh -ErrorAction SilentlyContinue
  if (-not $found) { Write-Output "pwsh 7 not found, so the step's shell: pwsh cannot be emulated"; exit 1 }
  $pwshExe = $found.Source
}

$pass = 0
$fail = 0
function Check([string]$Name, [bool]$Ok, [string]$Detail) {
  if ($Ok) { $script:pass++; Write-Host ("  PASS  " + $Name) }
  else { $script:fail++; Write-Host ("  FAIL  " + $Name + "  -> " + $Detail) }
}
function Section([string]$Name) { Write-Host ""; Write-Host "== $Name ==" }

Write-Host "host pwsh : $($PSVersionTable.PSVersion)"
Write-Host "emulated  : shell pwsh 7 on a windows runner; gh is a function shim"
Write-Host "workflow  : $workflowPath"

# ---------------------------------------------------------------- extract
$yaml = Get-Content -Raw $workflowPath
$m = [regex]::Match($yaml, "(?ms)^      - name: Evict superseded vcpkg caches.*?\r?\n        run: \|\r?\n(.*?)(?=\r?\n      - name: )")
if (-not $m.Success) { Write-Output 'EXTRACT FAILED'; exit 1 }
$body = (($m.Groups[1].Value -split "\r?\n") | ForEach-Object { $_ -replace '^          ', '' }) -join "`n"

$subs = [ordered]@{
  '${{ env.VCPKG_TRIPLET }}'         = 'x64-windows-static-md'
  '${{ runner.os }}'                 = 'Windows'
  '${{ github.repository }}'         = 'harness/repo'
  '${{ steps.image.outputs.image }}' = '20261004.326.1'
  '${{ github.run_id }}'             = '37785646479'
  '${{ github.run_attempt }}'        = '1'
}
foreach ($k in $subs.Keys) { $body = $body.Replace($k, $subs[$k]) }
if ($body -match '\$\{\{') { Write-Output 'SUBSTITUTION FAILED: an expression survived'; exit 1 }
if ($body -match 'o3ku') { Write-Output 'SUBSTITUTION FAILED: the real repository survived'; exit 1 }

$prefix = 'vcpkg-qt5-x64-windows-static-md-Windows-'
$savedKey = $prefix + '20261004.326.1-37785646479-1'

# ---------------------------------------------------------------- shim preamble
$preamble = @'
$ErrorActionPreference = 'Stop'
function gh {
  $global:LASTEXITCODE = 0
  $a = @($args)
  if ($a -contains '-X' -and $a -contains 'DELETE') {
    $url = $a[-1]
    $id = $url.Substring($url.LastIndexOf('/') + 1)
    Add-Content -LiteralPath $env:FAKE_GH_DELETES -Value $id
    if ($env:FAKE_GH_FAIL_ID -eq $id) { $global:LASTEXITCODE = 1 }
    return
  }
  if ($env:FAKE_GH_LIST_EXIT -and $env:FAKE_GH_LIST_EXIT -ne '0') {
    $global:LASTEXITCODE = [int]$env:FAKE_GH_LIST_EXIT
    return
  }
  if ($env:FAKE_GH_LIST_MISSING -eq '1') { throw 'fake gh: no list configured' }
  Get-Content -Raw -LiteralPath $env:FAKE_GH_LIST
}
'@

$env:GH_TOKEN = 'harness-token-that-the-real-api-would-reject'
$env:GITHUB_OUTPUT = Join-Path $work 'github_output.txt'
$env:GITHUB_ENV = Join-Path $work 'github_env.txt'
Set-Content -LiteralPath $env:GITHUB_OUTPUT -Value ''
Set-Content -LiteralPath $env:GITHUB_ENV -Value ''

$now = [datetime]::UtcNow
function Ts([int]$HoursAgo) { $now.AddHours(-$HoursAgo).ToString('yyyy-MM-ddTHH:mm:ss') + 'Z' }

function New-Entry {
  param(
    [string]$Key, [long]$Id, [string]$Created, [string]$Accessed,
    [long]$Size = 1000000000, [string]$Ref = 'refs/heads/main'
  )
  [pscustomobject]@{
    id = $Id; ref = $Ref; key = $Key
    created_at = $Created; last_accessed_at = $Accessed; size_in_bytes = $Size
  }
}

function Invoke-Case {
  param(
    [string]$Name,
    [object[]]$Entries,
    [int]$Pages = 1,
    [string]$ListExit = '0',
    [string]$FailId = '',
    [string]$ListMissing = '0'
  )
  $listFile = Join-Path $work "$Name.list.json"
  $deleteFile = Join-Path $work "$Name.deletes.txt"
  Set-Content -LiteralPath $deleteFile -Value ''

  $per = [math]::Ceiling($Entries.Count / [math]::Max($Pages, 1))
  if ($per -lt 1) { $per = 1 }
  # Not $pages: PowerShell parameter names are case-insensitive, so that would
  # collide with this function's [int]$Pages parameter.
  $pageList = @()
  for ($i = 0; $i -lt $Entries.Count; $i += $per) {
    $slice = @($Entries | Select-Object -Skip $i -First $per)
    $pageList += [pscustomobject]@{ total_count = $Entries.Count; actions_caches = $slice }
  }
  if ($pageList.Count -eq 0) { $pageList = @([pscustomobject]@{ total_count = 0; actions_caches = @() }) }
  Set-Content -LiteralPath $listFile -Value (ConvertTo-Json -InputObject $pageList -Depth 6 -Compress)

  # A fixture that does not round-trip would test the wrong thing, so say so. The Where-Object is
  # load-bearing, not cosmetic: an empty actions_caches array comes back from ConvertFrom-Json as a
  # single $null under Windows PowerShell 5.1 and as nothing at all under pwsh 7, so counting the
  # raw pipeline reports "1 of 0" on 5.1 for the empty-list case (section G) and the harness dies
  # there. It was written and only ever run under pwsh 7, which is why that went unnoticed.
  $back = @(Get-Content -Raw $listFile | ConvertFrom-Json | ForEach-Object { $_.actions_caches } |
      Where-Object { $_ })
  if ($back.Count -ne $Entries.Count) { throw "fixture $Name did not round-trip: $($back.Count) of $($Entries.Count)" }

  $scriptPath = Join-Path $work "$Name.ps1"
  Set-Content -LiteralPath $scriptPath -Value ($preamble + "`n" + $body)

  $env:FAKE_GH_LIST = $listFile
  $env:FAKE_GH_DELETES = $deleteFile
  $env:FAKE_GH_LIST_EXIT = $ListExit
  $env:FAKE_GH_FAIL_ID = $FailId
  $env:FAKE_GH_LIST_MISSING = $ListMissing

  $out = & $pwshExe -NoProfile -ExecutionPolicy Bypass -File $scriptPath *>&1
  $code = $LASTEXITCODE
  $deleted = @(Get-Content -LiteralPath $deleteFile -ErrorAction SilentlyContinue | Where-Object { $_ })
  [pscustomobject]@{ Output = ($out -join "`n"); Exit = $code; Deleted = $deleted }
}

function Ids([object[]]$Entries) { @($Entries | ForEach-Object { [string]$_.id }) }
function SameSet([object[]]$A, [object[]]$B) {
  $null -eq (Compare-Object ([string[]]$A) ([string[]]$B))
}

# ================================================================ case A
Section 'A - the real 11-entry list from 2026-10-08'
$raw = Get-Content -Raw (Join-Path $repoRoot 'scripts/fixtures/vcpkg-cache-list-2026-10-08.json') | ConvertFrom-Json
$real = @($raw | ForEach-Object { $_.actions_caches })
Check 'the fixture is the 11 real entries' ($real.Count -eq 11) "count=$($real.Count)"

# Normalise the clock: keep every gap, move the newest access to an hour ago, so
# the staleness rule cannot change this expectation as the fixture ages.
$maxAccess = ($real | ForEach-Object { [datetime]($_.last_accessed_at -replace 'Z$', '') } | Sort-Object -Descending)[0]
$delta = $maxAccess - ([datetime]::UtcNow.AddHours(-1))
$entriesA = @($real | ForEach-Object {
  $c = [datetime]($_.created_at -replace 'Z$', '')
  $a = [datetime]($_.last_accessed_at -replace 'Z$', '')
  [pscustomobject]@{
    id = $_.id; ref = $_.ref; key = $_.key; size_in_bytes = $_.size_in_bytes
    created_at = $c.Subtract($delta).ToString('yyyy-MM-ddTHH:mm:ss') + 'Z'
    last_accessed_at = $a.Subtract($delta).ToString('yyyy-MM-ddTHH:mm:ss') + 'Z'
  }
})

# Expected by hand from the fixture: one namespaced entry, nine pre-namespace
# saves that all collapse into the bare prefix, and the retired fixed key.
$expectKeptA = @(8659696160, 8620222698, 8020409145)
$expectDeletedA = @(8616244596, 8603102370, 8602241702, 8568015277, 8561509063, 8513735352, 8360582052, 8346380624)

$r = Invoke-Case -Name 'A' -Entries $entriesA
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'deleted exactly the 8 superseded entries' (SameSet $expectDeletedA $r.Deleted) `
  ("expected=" + ((Ids $expectDeletedA) -join ',') + " actual=" + ($r.Deleted -join ','))
Check 'kept the newest pre-namespace save' ($r.Deleted -notcontains '8620222698') 'deleted 8620222698'
Check 'kept the namespaced entry' ($r.Deleted -notcontains '8659696160') 'deleted 8659696160'
Check 'kept the retired fixed key (it is its own namespace)' ($r.Deleted -notcontains '8020409145') 'deleted 8020409145'
Check 'reported 3 namespaces' ($r.Output -match 'in 3 namespace\(s\)') 'namespace count wrong'
Check 'reported 8 deleted' ($r.Output -match 'deleted 8,') 'deleted count wrong'
$expectFreed = [math]::Round((($entriesA | Where-Object { $expectDeletedA -contains [long]$_.id } | Measure-Object size_in_bytes -Sum).Sum) / 1MB)
Check "reported the freed size ($expectFreed MB)" ($r.Output -match ("freed " + $expectFreed + " MB")) 'freed size wrong'

# ================================================================ case B
Section 'B - two pages, a superseded sibling, another image, a tag ref'
$entriesB = @(
  (New-Entry -Key $savedKey -Id 1001 -Created (Ts 2) -Accessed (Ts 1))
  (New-Entry -Key ($prefix + '20261004.326.1-37700000000-1') -Id 1002 -Created (Ts 50) -Accessed (Ts 40))
  (New-Entry -Key ($prefix + '20260927.320.1-37500000000-1') -Id 1003 -Created (Ts 60) -Accessed (Ts 30))
  (New-Entry -Key ($prefix + '20261004.326.1-37711111111-1') -Id 1004 -Created (Ts 70) -Accessed (Ts 5) -Ref 'refs/tags/v2.4.4')
)
$r = Invoke-Case -Name 'B' -Entries $entriesB -Pages 2
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'deleted only the superseded sibling' (SameSet @(1002) $r.Deleted) ("actual=" + ($r.Deleted -join ','))
Check 'kept the other image newest entry' ($r.Deleted -notcontains '1003') 'deleted 1003'
Check 'kept the tag-scoped entry (different ref, own group)' ($r.Deleted -notcontains '1004') 'deleted 1004'
Check 'read both pages' ($r.Output -match '4 entry/entries') 'pagination lost entries'

# ================================================================ case C
Section 'C - the saved-key guard, which also catches "$prefix2026..." being read as one variable'
$entriesC = @(
  (New-Entry -Key ($prefix + '20261004.326.1-37800000000-1') -Id 2001 -Created (Ts 1) -Accessed (Ts 1))
  (New-Entry -Key $savedKey -Id 2002 -Created (Ts 3) -Accessed (Ts 2))
)
$r = Invoke-Case -Name 'C' -Entries $entriesC
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'never deleted the key this run saved' ($r.Deleted -notcontains '2002') ("deleted=" + ($r.Deleted -join ','))
Check 'deleted nothing at all' ($r.Deleted.Count -eq 0) ("deleted=" + ($r.Deleted -join ','))
Check 'reported 2 kept' ($r.Output -match 'kept 2,') 'kept count wrong'

# ================================================================ case D
Section 'D - staleness, including the boundary and a missing timestamp'
$entriesD = @(
  (New-Entry -Key ($prefix + '20260101.1.1-40000000001-1') -Id 3001 -Created (Ts 800) -Accessed (Ts 480))
  (New-Entry -Key ($prefix + '20260102.1.1-40000000002-1') -Id 3002 -Created (Ts 400) -Accessed (Ts 324))
  (New-Entry -Key ($prefix + '20260103.1.1-40000000003-1') -Id 3003 -Created (Ts 500) -Accessed (Ts 348))
  (New-Entry -Key ($prefix + '20260104.1.1-40000000004-1') -Id 3004 -Created (Ts 600) -Accessed '')
)
$r = Invoke-Case -Name 'D' -Entries $entriesD
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'dropped a namespace idle 20.0 days' ($r.Deleted -contains '3001') ("deleted=" + ($r.Deleted -join ','))
Check 'dropped a namespace idle 14.5 days' ($r.Deleted -contains '3003') ("deleted=" + ($r.Deleted -join ','))
Check 'kept a namespace idle 13.5 days' ($r.Deleted -notcontains '3002') ("deleted=" + ($r.Deleted -join ','))
Check 'kept a namespace with no last-access timestamp' ($r.Deleted -notcontains '3004') ("deleted=" + ($r.Deleted -join ','))
Check 'reported the stale drops' ($r.Output -match 'as stale') 'no stale line'

# ================================================================ case E
Section 'E - the cache listing exits non-zero'
$r = Invoke-Case -Name 'E' -Entries @((New-Entry -Key $savedKey -Id 5001 -Created (Ts 1) -Accessed (Ts 1))) -ListExit '1'
Write-Host $r.Output
Check 'exit code is still 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'deleted nothing' ($r.Deleted.Count -eq 0) ("deleted=" + ($r.Deleted -join ','))
Check 'said so' ($r.Output -match 'vcpkg cache eviction:') 'no eviction message'

# ================================================================ case E2
Section 'E2 - the cache listing throws instead of exiting non-zero'
$r = Invoke-Case -Name 'E2' -Entries @((New-Entry -Key $savedKey -Id 5002 -Created (Ts 1) -Accessed (Ts 1))) -ListMissing '1'
Write-Host $r.Output
Check 'exit code is still 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'deleted nothing' ($r.Deleted.Count -eq 0) ("deleted=" + ($r.Deleted -join ','))

# ================================================================ case F
Section 'F - one delete fails, the rest still go'
$entriesF = @(
  (New-Entry -Key $savedKey -Id 6001 -Created (Ts 1) -Accessed (Ts 1))
  (New-Entry -Key ($prefix + '20261004.326.1-37700000001-1') -Id 6002 -Created (Ts 30) -Accessed (Ts 20))
  (New-Entry -Key ($prefix + '20261004.326.1-37700000002-1') -Id 6003 -Created (Ts 40) -Accessed (Ts 25))
)
$r = Invoke-Case -Name 'F' -Entries $entriesF -FailId '6002'
Write-Host $r.Output
Check 'exit code is still 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'still attempted every delete' (SameSet @(6002, 6003) $r.Deleted) ("actual=" + ($r.Deleted -join ','))
Check 'reported 2 kept and 1 deleted' (($r.Output -match 'kept 2,') -and ($r.Output -match 'deleted 1,')) 'summary wrong'
Check 'named the entry it could not delete' ($r.Output -match 'could not delete 6002') 'no warning'

# ================================================================ case G
Section 'G - no caches at all'
$r = Invoke-Case -Name 'G' -Entries @()
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'deleted nothing' ($r.Deleted.Count -eq 0) ("deleted=" + ($r.Deleted -join ','))
Check 'reported zero' ($r.Output -match '0 entry/entries') 'zero line missing'

# ================================================================ case H
Section 'H - keys under our prefix that match neither shape'
$entriesH = @(
  (New-Entry -Key ($prefix + 'v2') -Id 7001 -Created (Ts 900) -Accessed (Ts 800))
  (New-Entry -Key ($prefix + 'v3') -Id 7002 -Created (Ts 30) -Accessed (Ts 20))
)
$r = Invoke-Case -Name 'H' -Entries $entriesH
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'kept the fresh unclassifiable key' ($r.Deleted -notcontains '7002') ("deleted=" + ($r.Deleted -join ','))
Check 'dropped the unclassifiable key that went idle' ($r.Deleted -contains '7001') ("deleted=" + ($r.Deleted -join ','))

# ================================================================ case I
Section 'I - entries that are not ours'
$entriesI = @(
  (New-Entry -Key 'vcpkg-qt5-arm64-windows-static-md-Windows-20261004.326.1-1-1' -Id 8001 -Created (Ts 900) -Accessed (Ts 900))
  (New-Entry -Key 'vcpkg-qt5-x64-windows-static-md-Linux-20261004.326.1-1-1' -Id 8002 -Created (Ts 900) -Accessed (Ts 900))
  (New-Entry -Key 'node-cache-linux' -Id 8003 -Created (Ts 900) -Accessed (Ts 900))
  (New-Entry -Key $savedKey -Id 8004 -Created (Ts 1) -Accessed (Ts 1))
)
$r = Invoke-Case -Name 'I' -Entries $entriesI
Write-Host $r.Output
Check 'exit code is 0' ($r.Exit -eq 0) "exit=$($r.Exit)"
Check 'deleted nothing outside our prefix' ($r.Deleted.Count -eq 0) ("deleted=" + ($r.Deleted -join ','))
Check 'counted only our prefix' ($r.Output -match '1 entry/entries') 'prefix filter wrong'

# ================================================================ structural
Section 'STRUCTURAL - shapes the platform owns'
$saveIdx = $yaml.IndexOf('- name: Save vcpkg cache')
$evictIdx = $yaml.IndexOf('- name: Evict superseded vcpkg caches')
$msvcIdx = $yaml.IndexOf('- name: Set up MSVC')
Check 'the eviction runs after the save it must not undo' ($saveIdx -ge 0 -and $saveIdx -lt $evictIdx) "save=$saveIdx evict=$evictIdx"
Check 'the eviction runs before the build steps' ($evictIdx -lt $msvcIdx) "evict=$evictIdx msvc=$msvcIdx"
Check 'the step is gated on always()' ($yaml -match "(?ms)- name: Evict superseded vcpkg caches.*?\n        if: always\(\)") 'no always()'
Check 'the run block ends in exit 0' ($body -match "(?m)^exit 0\s*$") 'no exit 0'
# Read the permissions block by position: a regex like "(  .*\n)*" over the whole
# file with (?s) active backtracks catastrophically, which is how the first
# version of this harness died here with no output.
$permStart = $yaml.IndexOf("`npermissions:")
$permEnd = $yaml.IndexOf("`n`n", $permStart)
$permBlock = $yaml.Substring($permStart, $permEnd - $permStart)
Check 'the workflow grants actions: write' ($permBlock -match '(?m)^  actions: write\s*$') "block=$permBlock"
Check 'no GitHub expression survived substitution' ($body -notmatch '\$\{\{') 'expression left in the script'
Check 'the script never names the real repository' ($body -notmatch 'o3ku') 'real repo in the script'
# Assert on the `uses:` line, not on the text: the step's own comment explains why
# it is not ilammy/msvc-dev-cmd, so a substring search over the step would fail.
$msvcBlock = $yaml.Substring($msvcIdx, $yaml.IndexOf("`n      - name: ", $msvcIdx) - $msvcIdx)
$msvcUses = [regex]::Match($msvcBlock, '(?m)^\s*uses:\s*(\S+)').Groups[1].Value
Check 'the MSVC step uses the maintained action' ($msvcUses -eq 'TheMrMilchmann/setup-msvc-dev@v4') "uses=$msvcUses"
Check 'no step still uses the Node 20 action' ($yaml -notmatch '(?m)^\s*uses:\s*ilammy/') 'ilammy still in a uses: line'
Write-Host '  (the node24 claim for that action was read from its published metadata, not asserted here)'

Write-Host ""
Write-Host "passed $pass, failed $fail"
# ctest reads the exit code. A run that asserted nothing (a regex that stopped matching the workflow,
# say) must not look like a pass, and neither must one that found a broken case.
if ($pass + $fail -eq 0) { Write-Output 'no checks ran'; exit 1 }
if ($fail -gt 0) { exit 1 }
exit 0
