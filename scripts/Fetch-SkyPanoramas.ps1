$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$projectRoot = Split-Path -Parent $PSScriptRoot
$assetRoot = Join-Path $projectRoot 'assets\skies'
New-Item -ItemType Directory -Path $assetRoot -Force | Out-Null
$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
foreach ($assetId in @('kloppenheim_02_puresky','rogland_clear_night','kloppenheim_06_puresky')) {
    $metadata = Invoke-RestMethod ('https://api.polyhaven.com/files/' + $assetId)
    $rawPath = Join-Path ([IO.Path]::GetTempPath()) ($assetId + '-' + [guid]::NewGuid().ToString('N') + '.jpg')
    Invoke-WebRequest $metadata.tonemapped.url -OutFile $rawPath
    $source = [System.Drawing.Image]::FromFile($rawPath)
    $image = New-Object System.Drawing.Bitmap 4096,2048
    $graphics = [System.Drawing.Graphics]::FromImage($image)
    try {
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.DrawImage($source,0,0,4096,2048)
        $parameters = New-Object System.Drawing.Imaging.EncoderParameters 1
        $parameters.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality),([long]90)
        try { $image.Save((Join-Path $assetRoot ($assetId + '.jpg')),$jpeg,$parameters) }
        finally { $parameters.Dispose() }
    }
    finally {
        $graphics.Dispose(); $image.Dispose(); $source.Dispose()
        Remove-Item -LiteralPath $rawPath
    }
    Write-Output ($assetId + ': 4096x2048 JPEG')
}
