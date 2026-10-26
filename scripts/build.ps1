param([switch]$Probe)
. (Join-Path $PSScriptRoot 'Common.ps1')
$sketchName = if ($Probe) { 'JiggyStealthProbe' } else { 'JiggyStealth' }
$artifactName = if ($Probe) { 'Jiggy-Stealth-Probe' } else { 'Jiggy-Stealth' }
$sketch = if ($Probe) { Join-Path $ProjectRoot 'diagnostics\JiggyStealthProbe' } else { Join-Path $ProjectRoot 'firmware\JiggyStealth' }
$libraryRoot = Join-Path $ProjectRoot '.deps\libraries'
if (-not (Test-Path -LiteralPath $libraryRoot)) { throw 'Run .\scripts\setup.ps1 first.' }
$buildRoot = Join-Path $ProjectRoot "build\$sketchName"
$libraryArguments = @()
foreach ($library in $Toolchain.libraries) {
    $archiveName = [IO.Path]::GetFileName(([uri]$library.url).AbsolutePath)
    $path = Join-Path $libraryRoot ([IO.Path]::GetFileNameWithoutExtension($archiveName))
    $libraryArguments += @('--library', $path)
}
# Keep personal directories and the workspace name out of __FILE__/debug strings
# in public binaries. Specific project mappings follow the broader user mapping.
$prefixMaps = @()
foreach ($entry in @(
    @{ Path = $env:USERPROFILE; Replacement = 'user' },
    @{ Path = $ProjectRoot; Replacement = '.' }
)) {
    if (-not $entry.Path) { continue }
    foreach ($pathForm in @($entry.Path, $entry.Path.Replace('\','/')) | Select-Object -Unique) {
        $prefixMaps += ('"-ffile-prefix-map=' + $pathForm + '=' + $entry.Replacement + '"')
    }
}
$mapFlags = $prefixMaps -join ' '
$cppFlags = $mapFlags
$buildArguments = @(
    '--build-property', "compiler.c.extra_flags=$mapFlags",
    '--build-property', "compiler.S.extra_flags=$mapFlags"
)
if (-not $Probe) {
    $cppFlags += ' -DUSB_IDENTITY_WRAPPING=1'
    $buildArguments += @('--build-property', 'compiler.c.elf.extra_flags=-Wl,--wrap=tud_descriptor_device_cb,--wrap=tud_descriptor_string_cb')
}
$buildArguments += @('--build-property', "compiler.cpp.extra_flags=$cppFlags")
Invoke-ArduinoCli compile --fqbn $Toolchain.fqbn @libraryArguments @buildArguments --build-path $buildRoot --warnings all --jobs 4 $sketch
$distRoot = Join-Path $ProjectRoot 'dist'
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $buildRoot "$sketchName.ino.uf2") -Destination (Join-Path $distRoot "$artifactName.uf2")
$uf2Hash = (Get-FileHash -LiteralPath (Join-Path $distRoot "$artifactName.uf2") -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath (Join-Path $distRoot "$artifactName.sha256") -Value "$uf2Hash  $artifactName.uf2" -Encoding ascii
Write-Host "Built dist\$artifactName.uf2"
