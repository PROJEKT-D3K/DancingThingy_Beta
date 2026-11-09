# Screenshot helper for automated overlay verification.
# Captures the bottom-right corner of the primary screen (where the overlay
# parks itself) into a PNG file.
# Usage: powershell -ExecutionPolicy Bypass -File grab-corner.ps1 -Out stage1.png

param(
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$InsetX = 480,
    [int]$InsetY = 520,
    [int]$W = 460,
    [int]$H = 480
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$left  = $b.X + $b.Width  - $InsetX
$top   = $b.Y + $b.Height - $InsetY
if ($W -gt $b.Width)  { $W = $b.Width }
if ($H -gt $b.Height) { $H = $b.Height }

$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($left, $top, 0, 0, $bmp.Size)
$g.Dispose()

$ms = New-Object System.IO.MemoryStream
try {
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    [IO.File]::WriteAllBytes($Out, $ms.ToArray())
} finally {
    $ms.Dispose()
    $bmp.Dispose()
}

if ((Get-Item $Out).Length -le 0) { throw "capture failed: $Out" }
Write-Output "OK: $Out ($left,$top ${W}x${H})"
