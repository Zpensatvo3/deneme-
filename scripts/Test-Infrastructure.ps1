$ErrorActionPreference = 'Stop'
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command python3 -ErrorAction SilentlyContinue }
if (-not $python) { throw 'Python 3 bulunamadi.' }

& $python.Source (Join-Path $PSScriptRoot 'Test-Infrastructure.py')
if ($LASTEXITCODE -ne 0) { throw 'Altyapi kontrolu basarisiz oldu.' }
