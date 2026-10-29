param(
    [ValidateSet('1.21.11','26.1','26.1.1','26.1.2')]
    [string]$MinecraftVersion = '1.21.11'
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $dll = Join-Path $PSScriptRoot "build\local-test\$MinecraftVersion\Onestep-TEST-NO-LAUNCHER.dll"
    if (Test-Path -LiteralPath $dll) {
        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
        Copy-Item -LiteralPath $dll -Destination "$dll.previous-$stamp"
    }
    & (Join-Path $PSScriptRoot 'scripts/Build-Ninja.ps1') -MinecraftVersion $MinecraftVersion -Configuration Debug -LocalTest
    if (!(Test-Path -LiteralPath $dll)) { throw 'Test DLL was not produced.' }
    Write-Host "Local test DLL (Fabric $MinecraftVersion): $dll"
} finally { Pop-Location }
