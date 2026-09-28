param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [string]$Executable = ""
)

$ErrorActionPreference = "Stop"

function Stop-CheckWithError {
    param([string]$Message)

    Write-Error $Message -ErrorAction Continue
    exit 1
}

# Why this check exists
# ---------------------
# SongBird ships as a single executable: the .qm files have to be embedded, and until 2026-09-28
# they were not. The embedding step lived inside `if(SONGBIRD_HAS_LINGUISTTOOLS)` in
# src/CMakeLists.txt, and the Qt that CI builds against (vcpkg's static qt5-base) has no
# LinguistTools, so every released binary came out English-only -- while the build stayed green.
# The bug survived at least two releases because nothing checked the outcome, only the inputs.
#
# So this check looks at what the build actually produced rather than at what was configured:
#
#   * src/translations.qrc exists and names the .qm files that must be embedded
#   * every .qm it names exists on disk and really is a compiled translation
#   * the linked executable contains those .qm files (only checked when -Executable is passed)
#
# The QM header test is the magic followed by 0xa7000000. The bare 16-byte magic is NOT enough:
# a static Qt links QTranslator's own `static const uchar magic[16]` comparison constant into the
# binary, which matches the magic on its own. That false positive was observed on the released
# v2.4.3 executable, which had no .qm at all yet still matched the raw magic once.

$latin1 = [System.Text.Encoding]::GetEncoding(28591)

# Magic of Qt's compiled translation format, followed by the first header word.
$qmHeader = $latin1.GetString([byte[]]@(
    0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95, 0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd,
    0xa7, 0x00, 0x00, 0x00
))

function Get-BytePatternCount {
    param([string]$Haystack, [string]$Needle)

    if (-not $Needle) { return 0 }

    $count = 0
    $index = $Haystack.IndexOf($Needle, [System.StringComparison]::Ordinal)
    while ($index -ge 0) {
        $count++
        $index = $Haystack.IndexOf($Needle, $index + 1, [System.StringComparison]::Ordinal)
    }

    return $count
}

function Read-AsLatin1 {
    param([string]$Path)

    return $latin1.GetString([System.IO.File]::ReadAllBytes($Path))
}

# Release builds of SongBird.exe are UPX-compressed when upx is on PATH
# (SONGBIRD_ENABLE_UPX defaults to ON). A packed binary stores its resources compressed, so the
# .qm data is not searchable and a naive inspection would report a false failure on a perfectly
# good build. Unpack a throwaway copy first; if upx is unavailable, say so loudly and inspect only
# the build artifacts.
function Resolve-UpxExecutable {
    $command = Get-Command upx -ErrorAction SilentlyContinue
    if ($command -and (Test-Path -LiteralPath $command.Source -PathType Leaf)) {
        return $command.Source
    }

    return $null
}

function Test-IsPacked {
    param([string]$Text)

    # UPX names its sections UPX0/UPX1/... and leaves its signature in the stub.
    return ($Text.Contains('UPX!') -and $Text.Contains('UPX0'))
}

# The .qm files that must end up inside the executable: the application's own translation and
# Qt's own (which localizes QFileDialog, QMessageBox and the rest of Qt's visible text).
$requiredQmNames = @('SongBird_zh_CN.qm', 'qt_zh_CN.qm')

$violations = New-Object System.Collections.Generic.List[string]

$qrcPath = Join-Path $BuildDirectory 'translations.qrc'
if (-not (Test-Path -LiteralPath $qrcPath -PathType Leaf)) {
    Stop-CheckWithError ("$qrcPath does not exist, so no translation is embedded in this build. " +
        "src/CMakeLists.txt generates it whenever it finds either Qt LinguistTools or a compiled " +
        ".qm under translations/compiled/.")
}

$qrcText = Get-Content -LiteralPath $qrcPath -Raw
$entries = [regex]::Matches($qrcText, '<file\s+alias="(?<alias>[^"]+)"\s*>(?<path>[^<]+)</file>')
if ($entries.Count -eq 0) {
    Stop-CheckWithError "$qrcPath lists no translation files; refusing to report a pass."
}

$aliases = @{}
foreach ($entry in $entries) {
    $aliases[$entry.Groups['alias'].Value] = $entry.Groups['path'].Value.Trim()
}

$validatedQmCount = 0
foreach ($name in $requiredQmNames) {
    if (-not $aliases.ContainsKey($name)) {
        $violations.Add("$qrcPath does not embed $name (found: $($aliases.Keys -join ', ')).")
        continue
    }

    $qmPath = $aliases[$name]
    if (-not (Test-Path -LiteralPath $qmPath -PathType Leaf)) {
        $violations.Add("$qrcPath points $name at $qmPath, which does not exist.")
        continue
    }

    $qmBytes = [System.IO.File]::ReadAllBytes($qmPath)
    if ($qmBytes.Length -lt $qmHeader.Length) {
        $violations.Add("$qmPath is too small ($($qmBytes.Length) bytes) to be a compiled translation.")
        continue
    }

    if ((Get-BytePatternCount ($latin1.GetString($qmBytes)) $qmHeader) -ne 1) {
        $violations.Add("$qmPath does not start with a Qt .qm header, so Qt would refuse to load it.")
        continue
    }

    $validatedQmCount++
}

if ($Executable) {
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
        $violations.Add("Executable not found: $Executable")
    } else {
        $inspectPath = $Executable
        $unpackedPath = $null

        if (Test-IsPacked (Read-AsLatin1 $Executable)) {
            $upx = Resolve-UpxExecutable
            if (-not $upx) {
                Write-Host "NOTE: $Executable is UPX-packed and upx was not found, so the linked"
                Write-Host "      binary was NOT inspected. The .qrc and .qm checks above still ran."
            } else {
                $unpackedPath = [System.IO.Path]::Combine(
                    [System.IO.Path]::GetTempPath(),
                    "songbird-unpacked-$([System.Guid]::NewGuid().ToString('N')).exe"
                )
                try {
                    $null = & $upx -d -o $unpackedPath $Executable 2>&1
                    if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $unpackedPath)) {
                        $inspectPath = $unpackedPath
                        Write-Host "Unpacked the UPX-compressed executable for inspection."
                    } else {
                        $violations.Add("$Executable is UPX-packed and could not be unpacked " +
                            "(upx exit code $LASTEXITCODE), so its embedded translations could not " +
                            "be verified.")
                        $inspectPath = $null
                    }
                } finally {
                    # Inspected below, before the file is removed.
                }
            }
        }

        if ($inspectPath) {
            $executableText = Read-AsLatin1 $inspectPath
            $embeddedCount = Get-BytePatternCount $executableText $qmHeader

            # One header per embedded .qm. Anything less means the .qrc was generated but the
            # resources were never linked into the binary -- which is exactly how the released
            # builds broke.
            if ($embeddedCount -lt $requiredQmNames.Count) {
                $violations.Add("$Executable contains $embeddedCount embedded .qm file(s) but " +
                    "$($requiredQmNames.Count) are required; the UI will not be localized.")
            }

            # A concrete string from Qt's own translation, so the check also proves the embedded
            # data is readable text and not a header with nothing behind it.
            $okButton = $latin1.GetString([System.Text.Encoding]::BigEndianUnicode.GetBytes('确定'))
            if ((Get-BytePatternCount $executableText $okButton) -eq 0) {
                $violations.Add("$Executable does not contain the Chinese label for OK, so the " +
                    "embedded translation is empty or corrupt.")
            }

            Write-Host ("Executable: $([System.IO.File]::ReadAllBytes($inspectPath).Length) bytes " +
                "scanned, $embeddedCount embedded .qm header(s) found.")
        }

        if ($unpackedPath -and (Test-Path -LiteralPath $unpackedPath)) {
            Remove-Item -LiteralPath $unpackedPath -Force
        }
    }
} else {
    Write-Host "NOTE: no -Executable was given, so the linked binary was not inspected. Only the"
    Write-Host "      generated .qrc and the .qm files it references were validated."
}

if ($violations.Count -gt 0) {
    foreach ($violation in $violations) {
        Write-Host "  - $violation"
    }
    Stop-CheckWithError "Translation embedding is incomplete ($($violations.Count) problem(s))."
}

# Report what was scanned: a check that prints nothing is indistinguishable from one that did
# nothing, which is the failure mode this whole file exists to avoid.
Write-Host ("Checked translation embedding: $qrcPath lists $($entries.Count) file(s); " +
    "validated $validatedQmCount required .qm file(s).")
Write-Host "Translation embedding check passed."
