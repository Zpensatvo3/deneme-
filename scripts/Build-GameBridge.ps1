$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$classRoot = Join-Path $projectRoot 'assets\hybrid'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -d $classRoot `
    (Join-Path $projectRoot 'src\bridge\GameBridge.java') `
    (Join-Path $projectRoot 'src\bridge\UtilityBridge.java') `
    (Join-Path $projectRoot 'src\bridge\ItemBridge.java') `
    (Join-Path $projectRoot 'src\bridge\InventoryBridge.java') `
    (Join-Path $projectRoot 'src\bridge\WorldBridge.java') `
    (Join-Path $projectRoot 'src\bridge\ClientTickRelay.java')
if ($LASTEXITCODE -ne 0) { throw 'GameBridge compilation failed.' }
& python -X utf8 (Join-Path $PSScriptRoot 'Check-GameBridgeContracts.py')
if ($LASTEXITCODE -ne 0) { throw 'Native / Java bridge contracts do not match.' }
Write-Host 'Built five OneStep game bridges and the shared client tick relay (no Minecraft/Fabric compile dependency).'
