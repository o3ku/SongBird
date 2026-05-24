param(
    [string]$SourceRoot = "src"
)

$ErrorActionPreference = "Stop"

function Invoke-Rg {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    $output = & rg @Arguments 2>$null
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed with exit code $LASTEXITCODE"
    }
    return @($output)
}

$root = Resolve-Path $SourceRoot
$violations = New-Object System.Collections.Generic.List[string]

$legacyBackendDirs = @(
    (Join-Path $root "runtime/core/xray"),
    (Join-Path $root "runtime/core/singbox")
)
foreach ($dir in $legacyBackendDirs) {
    if (Test-Path $dir) {
        $violations.Add("Legacy concrete backend directory must stay out of runtime: $dir")
    }
}

$consumerDirs = @(
    (Join-Path $root "app"),
    (Join-Path $root "ui"),
    (Join-Path $root "services"),
    (Join-Path $root "runtime")
)
$existingConsumerDirs = $consumerDirs | Where-Object { Test-Path $_ }
if ($existingConsumerDirs.Count -gt 0) {
    $matches = Invoke-Rg --line-number --glob "*.h" --glob "*.cpp" '#include\s+"backends/' @existingConsumerDirs
    foreach ($match in $matches) {
        $violations.Add("Concrete backend include from common/application layer: $match")
    }
}

$backendDir = Join-Path $root "backends"
if (Test-Path $backendDir) {
    $matches = Invoke-Rg --line-number --glob "*.h" --glob "*.cpp" '#include\s+"(app|ui|services|platform)/' $backendDir
    foreach ($match in $matches) {
        $violations.Add("Backend may not include application/service/UI/platform layer: $match")
    }
}

if ($violations.Count -gt 0) {
    Write-Error (($violations | ForEach-Object { "- $_" }) -join [Environment]::NewLine)
    exit 1
}

Write-Host "Backend boundary check passed."
