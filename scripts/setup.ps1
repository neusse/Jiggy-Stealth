. (Join-Path $PSScriptRoot 'Common.ps1')
$cli = Find-ArduinoCli
$installed = (& $cli core list --format json | ConvertFrom-Json).platforms
if (-not ($installed | Where-Object { $_.id -eq 'rp2040:rp2040' -and $_.installed_version -eq '4.5.3' })) {
    Invoke-ArduinoCli core update-index --additional-urls $Toolchain.boardIndex
    Invoke-ArduinoCli core install $Toolchain.core --additional-urls $Toolchain.boardIndex
}

$cacheRoot = Join-Path $ProjectRoot '.deps\downloads'
$libraryRoot = Join-Path $ProjectRoot '.deps\libraries'
New-Item -ItemType Directory -Force -Path $cacheRoot, $libraryRoot | Out-Null
foreach ($library in $Toolchain.libraries) {
    $archiveName = [IO.Path]::GetFileName(([uri]$library.url).AbsolutePath)
    $archive = Join-Path $cacheRoot $archiveName
    $expectedFolder = Join-Path $libraryRoot ([IO.Path]::GetFileNameWithoutExtension($archiveName))
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading $($library.name) $($library.version)"
        Invoke-WebRequest -Uri $library.url -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $library.sha256) {
        throw "Checksum mismatch for $archive. Remove this archive and rerun setup."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $expectedFolder 'library.properties'))) {
        Expand-Archive -LiteralPath $archive -DestinationPath $libraryRoot -Force
    }
    if (-not (Test-Path -LiteralPath (Join-Path $expectedFolder 'library.properties'))) {
        throw "Unexpected archive layout for $archiveName"
    }
}
Write-Host 'Pinned core and project-local libraries are ready.'
