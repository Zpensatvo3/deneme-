param([string]$AsmClasspath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$AsmClasspath = & (Join-Path $PSScriptRoot 'Resolve-AsmClasspath.ps1') -AsmClasspath $AsmClasspath
$classRoot = Join-Path $projectRoot 'assets\skybridge'
New-Item -ItemType Directory -Path $classRoot -Force | Out-Null
& javac --release 17 -encoding UTF-8 -g:none -cp $AsmClasspath -d $classRoot `
    (Join-Path $projectRoot 'src\bridge\SkyBridge.java') `
    (Join-Path $projectRoot 'src\bridge\SkyFogBridge.java') `
    (Join-Path $projectRoot 'src\bridge\SkyTransformer.java')
if ($LASTEXITCODE -ne 0) { throw 'SkyBridge compilation failed.' }
