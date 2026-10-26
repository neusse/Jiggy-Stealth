param([string]$Port, [string]$Drive, [switch]$Diagnostics, [switch]$Probe)
. (Join-Path $PSScriptRoot 'Common.ps1')
if ($Diagnostics -and $Probe) { throw 'Choose -Diagnostics or -Probe, not both.' }
if ([bool]$Port -eq [bool]$Drive) { throw 'Specify either -Drive R:\ for UF2 copying or -Port COMx for an existing diagnostic serial port.' }
$sketchName = if ($Probe) { 'JiggyStealthProbe' } else { 'JiggyStealth' }
$artifactName = if ($Probe) { 'Jiggy-Stealth-Probe' } elseif ($Diagnostics) { 'Jiggy-Stealth-Diagnostics' } else { 'Jiggy-Stealth' }
$firmwarePath = Join-Path $ProjectRoot "dist\$artifactName.uf2"
if ($Drive) {
    $driveRoot = (Get-Item -LiteralPath $Drive -ErrorAction Stop).FullName
    if ($driveRoot.TrimEnd('\') -ne [IO.Path]::GetPathRoot($driveRoot).TrimEnd('\')) {
        throw 'Use the root of the RPI-RP2 drive, for example -Drive R:\.'
    }
    $infoPath = Join-Path $driveRoot 'INFO_UF2.TXT'
    if (-not (Test-Path -LiteralPath $infoPath) -or
        (Get-Content -LiteralPath $infoPath -Raw) -notmatch '(?m)^Board-ID:\s*RPI-RP2\b') {
        throw 'The selected drive is not an RP2040 UF2 bootloader. Hold Boot, press/release Reset, then wait for RPI-RP2.'
    }
    if (-not (Test-Path -LiteralPath $firmwarePath)) { throw "Missing $firmwarePath. Build or download this profile first." }
    $expectedHash = ((Get-Content -LiteralPath (Join-Path $ProjectRoot "dist\$artifactName.sha256") -Raw).Trim() -split '\s+')[0]
    if ((Get-FileHash -LiteralPath $firmwarePath -Algorithm SHA256).Hash -ne $expectedHash) { throw 'UF2 checksum mismatch.' }
    Copy-Item -LiteralPath $firmwarePath -Destination (Join-Path $driveRoot "$artifactName.uf2")
    Write-Host "Copied $artifactName.uf2. The board restarts with Movement selected and jiggle off."
    return
}
$sketch = if ($Probe) { Join-Path $ProjectRoot 'diagnostics\JiggyStealthProbe' } else { Join-Path $ProjectRoot 'firmware\JiggyStealth' }
$buildName = if ($Probe) { $sketchName } elseif ($Diagnostics) { 'JiggyStealth-Diagnostics' } else { 'JiggyStealth-Production' }
$buildRoot = Join-Path $ProjectRoot "build\$buildName"
if (-not (Test-Path -LiteralPath (Join-Path $buildRoot "$sketchName.ino.uf2"))) {
    throw 'Build this sketch before flashing it.'
}
Write-Host "Uploading $artifactName through $Port. Production has no COM port after reboot; use Boot/Reset and -Drive for later updates."
Invoke-ArduinoCli upload --fqbn $Toolchain.fqbn --port $Port --input-dir $buildRoot $sketch
