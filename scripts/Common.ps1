$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Toolchain = Get-Content -LiteralPath (Join-Path $ProjectRoot 'toolchain.lock.json') -Raw | ConvertFrom-Json

function Find-ArduinoCli {
    $candidate = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($candidate) { return $candidate.Source }
    foreach ($location in @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'),
        (Join-Path $env:ProgramFiles 'Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe')
    )) {
        if (Test-Path -LiteralPath $location) { return $location }
    }
    throw 'Install Arduino CLI or Arduino IDE 2, then run this script again.'
}

function Invoke-ArduinoCli {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$CliArguments)
    & (Find-ArduinoCli) @CliArguments
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI failed with exit code $LASTEXITCODE" }
}
