param(
    [ValidateSet('1.21.11','26.1','26.1.1','26.1.2')]
    [string]$MinecraftVersion = '1.21.11',
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$LocalTest,
    [switch]$Clean,
    [switch]$ProtectionMap,
    [ValidateRange(1,256)][int]$Jobs = [Math]::Max(1, [Math]::Min(8, [Environment]::ProcessorCount))
)
$ErrorActionPreference = 'Stop'
# Visual Studio's CMake component bundles both tools, even when absent from PATH.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath | Select-Object -First 1
    if ($installation) {
        $env:PATH = (Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin') + ';' +
            (Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja') + ';' + $env:PATH
    }
}
foreach ($tool in @('cmake', 'ninja', 'cl')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool bulunamadi. Visual Studio C++ CMake tools kurun ve x64 Developer PowerShell kullanin."
    }
}
$root = Split-Path -Parent $PSScriptRoot
$flavor = if ($LocalTest) { "$Configuration-test" } else { $Configuration }
$cache = Join-Path $root "build/ninja/$MinecraftVersion/$flavor"
$output = if ($LocalTest) { Join-Path $root "build/local-test/$MinecraftVersion" }
    else { Join-Path $root "build/minecraft/$MinecraftVersion" }
$localFlag = if ($LocalTest) { 'ON' } else { 'OFF' }
$mapFlag = if ($ProtectionMap) { 'ON' } else { 'OFF' }
$watch = [Diagnostics.Stopwatch]::StartNew()
& cmake -S $root -B $cache -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DMINECRAFT_VERSION=$MinecraftVersion" "-DONESTEP_LOCAL_TEST=$localFlag" `
    "-DONESTEP_PROTECTION_MAP=$mapFlag" "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$output"
if ($LASTEXITCODE -ne 0) { throw 'CMake yapilandirmasi basarisiz.' }
$buildArgs = @('--build', $cache, '--parallel', $Jobs)
if ($Clean) { $buildArgs += '--clean-first' }
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Ninja derlemesi basarisiz.' }
$watch.Stop()
Write-Host ("Ninja: {0:N2} saniye. Ara dosyalar: {1}" -f $watch.Elapsed.TotalSeconds, $cache)
