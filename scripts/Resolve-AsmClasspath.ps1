param([string]$AsmClasspath)
$ErrorActionPreference = 'Stop'
if ($AsmClasspath) { return $AsmClasspath }
$libraryRoot = Join-Path $env:APPDATA 'ModrinthApp\meta\libraries\org\ow2\asm'
$asmJar = Get-ChildItem -LiteralPath (Join-Path $libraryRoot 'asm') -Filter 'asm-*.jar' -Recurse |
    Sort-Object { [version]$_.Directory.Name } -Descending | Select-Object -First 1
$treeJar = Get-ChildItem -LiteralPath (Join-Path $libraryRoot 'asm-tree') -Filter 'asm-tree-*.jar' -Recurse |
    Sort-Object { [version]$_.Directory.Name } -Descending | Select-Object -First 1
if (!$asmJar -or !$treeJar) { throw 'Pass -AsmClasspath with ASM and ASM Tree 9.x jars.' }
return $asmJar.FullName + ';' + $treeJar.FullName
