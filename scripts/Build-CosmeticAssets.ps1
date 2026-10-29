param(
    [Parameter(Mandatory)][string]$Dragon,
    [Parameter(Mandatory)][string]$Angel,
    [Parameter(Mandatory)][string]$Butterfly,
    [Parameter(Mandatory)][string]$Phoenix,
    [Parameter(Mandatory)][string]$Astral
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$cosmeticAssetDir=Join-Path (Split-Path -Parent $PSScriptRoot) 'assets\cosmetics'
New-Item -ItemType Directory -Path $cosmeticAssetDir -Force | Out-Null
$cosmeticAssetJobs=@(
    @{Source=$Dragon; Name='wing-dragon.png'; Width=640; Height=640; Alpha=$true},
    @{Source=$Angel; Name='wing-angel.png'; Width=640; Height=640; Alpha=$true},
    @{Source=$Butterfly; Name='wing-butterfly.png'; Width=640; Height=640; Alpha=$true},
    @{Source=$Phoenix; Name='cape-phoenix.png'; Width=512; Height=768; Alpha=$false},
    @{Source=$Astral; Name='cape-astral.png'; Width=512; Height=768; Alpha=$false}
)
foreach ($cosmeticAssetJob in $cosmeticAssetJobs) {
    $cosmeticAssetSource=[System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $cosmeticAssetJob.Source).Path)
    $cosmeticAssetOutput=[System.Drawing.Bitmap]::new($cosmeticAssetJob.Width,$cosmeticAssetJob.Height,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $cosmeticAssetGraphics=[System.Drawing.Graphics]::FromImage($cosmeticAssetOutput)
    try {
        if ($cosmeticAssetJob.Alpha -and $cosmeticAssetSource.GetPixel(0,0).A -gt 20) { throw 'Expected a genuinely transparent wing texture.' }
        # Mechanical production resize only; generated artwork/silhouette/alpha
        # are otherwise preserved. Original imagegen outputs remain untouched.
        $cosmeticAssetGraphics.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $cosmeticAssetGraphics.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $cosmeticAssetGraphics.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $cosmeticAssetGraphics.DrawImage($cosmeticAssetSource,0,0,$cosmeticAssetJob.Width,$cosmeticAssetJob.Height)
        $cosmeticAssetOutput.Save((Join-Path $cosmeticAssetDir $cosmeticAssetJob.Name),[System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $cosmeticAssetGraphics.Dispose(); $cosmeticAssetOutput.Dispose(); $cosmeticAssetSource.Dispose() }
}
Get-ChildItem -LiteralPath $cosmeticAssetDir -Filter '*.png' | Select-Object Name,Length
