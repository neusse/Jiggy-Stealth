param([string]$Ref = 'HEAD')
. (Join-Path $PSScriptRoot 'Common.ps1')

# Release archives come from an exact commit, never from caches or local logs.
$taskChanges = & git -C $ProjectRoot status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0) { throw 'Packaging requires a Git checkout.' }
if ($taskChanges) { throw 'Commit tracked changes before packaging a release.' }
$taskCommit = & git -C $ProjectRoot rev-parse --verify ($Ref + '^{commit}')
if ($LASTEXITCODE -ne 0) { throw "Cannot resolve source ref: $Ref" }
$taskCommit = $taskCommit.Trim()
$taskConfig = (& git -C $ProjectRoot show ($taskCommit + ':firmware/JiggyStealth/Config.h')) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot read the committed firmware configuration.' }
$taskVersionMatch = [regex]::Match($taskConfig, '\bVersion\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?)"')
if (-not $taskVersionMatch.Success) { throw 'The committed firmware version is missing or invalid.' }
$taskVersion = $taskVersionMatch.Groups[1].Value
$taskTag = 'v' + $taskVersion
$taskPrefix = "Jiggy-Stealth-$taskTag/"
$taskOutput = Join-Path $ProjectRoot "build\releases\$taskTag"
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskZipPath = Join-Path $taskOutput "Jiggy-Stealth-$taskTag-source.zip"
& git -C $ProjectRoot archive --format=zip "--prefix=$taskPrefix" "--output=$taskZipPath" $taskCommit
if ($LASTEXITCODE -ne 0) { throw 'Source archive creation failed.' }

Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchive = [IO.Compression.ZipFile]::OpenRead($taskZipPath)
try {
    foreach ($required in @('README.md','LICENSE','NOTICE','LICENSING.md','THIRD_PARTY_NOTICES.md',
        'toolchain.lock.json','scripts/setup.ps1','scripts/build.ps1','scripts/test.ps1',
        'firmware/JiggyStealth/JiggyStealth.ino','docs/USER_GUIDE.md',
        'docs/licenses/arduino-pico.txt','docs/licenses/adafruit-neopixel.txt',
        'firmware/JiggyStealth/UsbDevice.cpp',
        'dist/Jiggy-Stealth.uf2','dist/Jiggy-Stealth.sha256',
        'dist/Jiggy-Stealth-Diagnostics.uf2','dist/Jiggy-Stealth-Diagnostics.sha256')) {
        if (-not $taskArchive.GetEntry($taskPrefix + $required)) { throw "Source package is missing $required" }
    }
    foreach ($entry in $taskArchive.Entries) {
        $relative = $entry.FullName.Substring($taskPrefix.Length)
        if ($relative -match '(^|/)(\.git|\.deps|\.local|build)(/|$)|(^|/)\.env($|\.)') {
            throw "Unexpected local file in package: $relative"
        }
    }
    $taskFirmwarePaths = foreach ($taskName in @('Jiggy-Stealth', 'Jiggy-Stealth-Diagnostics')) {
        $taskFirmwarePath = Join-Path $taskOutput "$taskName.uf2"
        $taskSourceStream = $taskArchive.GetEntry($taskPrefix + "dist/$taskName.uf2").Open()
        $taskDestination = [IO.File]::Create($taskFirmwarePath)
        try { $taskSourceStream.CopyTo($taskDestination) }
        finally { $taskDestination.Dispose(); $taskSourceStream.Dispose() }
        $taskReader = [IO.StreamReader]::new($taskArchive.GetEntry($taskPrefix + "dist/$taskName.sha256").Open())
        try { $taskExpectedHash = ($taskReader.ReadToEnd().Trim() -split '\s+')[0] }
        finally { $taskReader.Dispose() }
        if ((Get-FileHash -LiteralPath $taskFirmwarePath -Algorithm SHA256).Hash -ne $taskExpectedHash) {
            throw "Packaged $taskName firmware does not match its committed checksum."
        }
        $taskFirmwarePath
    }
} finally { $taskArchive.Dispose() }

$taskFlashPath = Join-Path $taskOutput 'FLASHING.txt'
$taskFlashText = @"
Jiggy-Stealth $taskTag
Source commit: $taskCommit
Board: Adafruit Feather RP2040 USB Host, product 5723.

INSTALL THE UF2 (no compiler or development tools required)
1. Connect the board's USB-C port to your computer with a data cable.
2. Hold Boot (near USB-A), press and release Reset (near USB-C).
3. Release Boot when the RPI-RP2 drive appears.
4. Copy ONE chosen UF2 onto RPI-RP2. The board restarts automatically.
5. Plug the paired mouse receiver or supported mouse into the USB-A port.

CHOOSE A BUILD
Jiggy-Stealth.uf2: Production. Mouse + keyboard HID, no USB serial/COM port.
Jiggy-Stealth-Diagnostics.uf2: The same full bridge plus diagnostic USB serial.
Both support all mouse and jiggle features and forward the donor USB identity.
Production also omits CDC while discovering a receiver or showing fallback HID.

UF2 updates do not require serial. Repeat the Boot/Reset steps above to switch
builds or update production. The RPI-RP2 drive is provided by the RP2040 ROM
bootloader independently of the application. Automatic COM-port uploads and
scripts/capture.py require diagnostics; production has no serial connection.

Movement is the default mode. Jiggle starts OFF after every restart.
Short-press Boot to toggle jiggle: green = Movement on, dim blue = off.
Leave the mouse idle for 30-60 seconds to see a small excursion and return.
Hold Boot for two seconds to switch Movement/Shift; the selected mode stays
off until another short press. Amber = Shift enabled.
Pair wireless receivers directly on the computer before using this bridge.

SOURCE AND BUILD
Jiggy-Stealth-$taskTag-source.zip contains the complete tagged project,
prebuilt firmware, build scripts, pinned dependency manifest, docs, and licenses.
Extract it and follow docs/USER_GUIDE.md to build on Windows. Setup downloads
the pinned Arduino core and libraries; those toolchains are not bundled.
SHA256SUMS.txt contains checksums for both UF2s, the source ZIP, and this guide.

LICENSE
PolyForm Noncommercial License 1.0.0:
https://polyformproject.org/licenses/noncommercial/1.0.0
Required Notice: Copyright (c) 2026 neusse (https://github.com/neusse)
Commercial use outside the license's permissions requires a separate written
license. Third-party components retain their own licenses. Full terms,
third-party notices and license texts are included in the source package.
https://github.com/neusse/Jiggy-Stealth/blob/$taskTag/LICENSING.md
"@
[IO.File]::WriteAllText($taskFlashPath, ($taskFlashText -replace "`r`n","`n").TrimEnd() + "`n", [Text.UTF8Encoding]::new($false))
$taskChecksumLines = foreach ($asset in (@($taskFirmwarePaths) + @($taskZipPath, $taskFlashPath) | Sort-Object)) {
    $hash = (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant()
    $hash + '  ' + [IO.Path]::GetFileName($asset)
}
[IO.File]::WriteAllText((Join-Path $taskOutput 'SHA256SUMS.txt'), ($taskChecksumLines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Packaged Jiggy-Stealth $taskTag from $taskCommit"
Write-Host "Release assets: $taskOutput"
