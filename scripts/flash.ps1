param([Parameter(Mandatory = $true)][string]$Port, [switch]$Probe)
. (Join-Path $PSScriptRoot 'Common.ps1')
$sketchName = if ($Probe) { 'JiggyStealthProbe' } else { 'JiggyStealth' }
$sketch = if ($Probe) { Join-Path $ProjectRoot 'diagnostics\JiggyStealthProbe' } else { Join-Path $ProjectRoot 'firmware\JiggyStealth' }
$buildRoot = Join-Path $ProjectRoot "build\$sketchName"
if (-not (Test-Path -LiteralPath (Join-Path $buildRoot "$sketchName.ino.uf2"))) {
    throw 'Build this sketch before flashing it.'
}
Write-Host "Uploading $sketchName to $Port. Jiggy-Stealth always starts with jiggle off."
Invoke-ArduinoCli upload --fqbn $Toolchain.fqbn --port $Port --input-dir $buildRoot $sketch
