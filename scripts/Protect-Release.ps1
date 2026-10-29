param(
    [Parameter(Mandatory)][string]$InputDll,
    [Parameter(Mandatory)][string]$ProjectFile,
    [string]$VMProtectPath = ''
)
$ErrorActionPreference = 'Stop'
$authRoot = Split-Path -Parent $PSScriptRoot
$authInput = (Resolve-Path -LiteralPath $InputDll).Path
$authProject = (Resolve-Path -LiteralPath $ProjectFile).Path
if ([IO.Path]::GetExtension($authInput) -ne '.dll' -or [IO.Path]::GetExtension($authProject) -ne '.vmp') {
    throw 'Gercek bir DLL ve VMProtect GUI ile kaydedilmis .vmp projesi gereklidir.'
}
if (-not $VMProtectPath) {
    $authTool = Get-Command VMProtect_Con.exe -ErrorAction SilentlyContinue
    if ($authTool) { $VMProtectPath = $authTool.Source }
}
if (-not $VMProtectPath -or -not (Test-Path -LiteralPath $VMProtectPath -PathType Leaf)) {
    throw 'VMProtect console surumu bulunamadi. Lisansli Professional/Ultimate kurulum yolunu belirtin.'
}
$authToolPath = (Resolve-Path -LiteralPath $VMProtectPath).Path
# Every run has its own output; the original and earlier protected DLLs remain available.
$authOutputDir = Join-Path $authRoot ('build\protected\' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6))
New-Item -ItemType Directory -Path $authOutputDir | Out-Null
$authOutput = Join-Path $authOutputDir 'Onestep.pending.dll'
& $authToolPath $authInput $authOutput '-pf' $authProject '-we'
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $authOutput -PathType Leaf)) {
    throw 'VMProtect basarili bir cikti uretmedi. DLL korumali olarak isaretlenmedi.'
}
if ((Get-Item $authOutput).Length -lt 1024 -or
    (Get-FileHash $authInput -Algorithm SHA256).Hash -eq (Get-FileHash $authOutput -Algorithm SHA256).Hash) {
    throw 'VMProtect ciktisi degismemis veya gecersiz. Koruma ayarlarini kontrol edin.'
}
# The caller signs this final artifact after protection, never the input first.
$authFinal = Join-Path $authOutputDir 'Onestep.dll'
Move-Item -LiteralPath $authOutput -Destination $authFinal
Write-Output $authFinal
