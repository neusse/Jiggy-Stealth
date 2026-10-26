. (Join-Path $PSScriptRoot 'Common.ps1')
$output = Join-Path $ProjectRoot 'build\tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$sources = @(
    (Join-Path $ProjectRoot 'tests\test_main.cpp'),
    (Join-Path $ProjectRoot 'firmware\JiggyStealth\HidMouseParser.cpp'),
    (Join-Path $ProjectRoot 'firmware\JiggyStealth\BridgeLogic.cpp'),
    (Join-Path $ProjectRoot 'firmware\JiggyStealth\UsbIdentity.cpp'),
    (Join-Path $ProjectRoot 'firmware\JiggyStealth\UsbIdentityDevice.cpp'),
    (Join-Path $ProjectRoot 'firmware\JiggyStealth\UsbDevice.cpp'),
    (Join-Path $ProjectRoot 'tests\test_usb_device.cpp')
)
$includes = Join-Path $ProjectRoot 'firmware\JiggyStealth'
$stubs = Join-Path $ProjectRoot 'tests\stubs'
$compiler = Get-Command g++ -ErrorAction SilentlyContinue
foreach ($profile in @('Production', 'Diagnostics')) {
    $diagnosticFlag = if ($profile -eq 'Diagnostics') { 1 } else { 0 }
    $defines = @('-DUSB_IDENTITY_WRAPPING=1', '-DUSB_STARTUP_WRAPPING=1', "-DJIGGY_DIAGNOSTICS=$diagnosticFlag")
    $executable = Join-Path $output "tests-$profile.exe"
    if ($compiler) {
        & $compiler.Source -std=c++17 -Wall -Wextra -Werror -O2 @defines '-I' $stubs '-I' $includes @sources -o $executable
        if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Tests need MSVC Build Tools or g++ on PATH.' }
        $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $installation) { throw 'Install the MSVC C++ Build Tools workload or g++.' }
        $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
        # Keep the batch helper small; cmd is needed only to initialize MSVC.
        $quotedSources = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $defineFlags = ($defines | ForEach-Object { $_.Replace('-D','/D') }) -join ' '
        $batch = "@echo off`r`ncall `"$vcvars`" >nul`r`nif errorlevel 1 exit /b 1`r`ncd /d `"$output`"`r`ncl /nologo /std:c++17 /EHsc /W4 /WX /O2 $defineFlags /I`"$stubs`" /I`"$includes`" $quotedSources /Fe:`"$executable`"`r`nexit /b %errorlevel%`r`n"
        $batchPath = Join-Path $output 'compile-tests.cmd'
        Set-Content -LiteralPath $batchPath -Value $batch -Encoding ascii
        & $env:ComSpec /d /c $batchPath
        if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
