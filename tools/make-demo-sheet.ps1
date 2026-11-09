# Generates a Fruity Dance-format demo sprite sheet WITHOUT any third-party
# assets: "Demo.png" (8 cells x 3 rows: Demo / Waiting / Held) + "Demo.txt".
# Layout per FL's manual: sheet is 8 cells wide, every row is an 8-frame
# dance; "Waiting" is this project's idle-row extension, "Held" is the drag pose.
#
# Usage:  powershell -ExecutionPolicy Bypass -File make-demo-sheet.ps1 [-OutDir <folder>]
# Default OutDir = %APPDATA%\DanceOverlay\Characters  (the overlay scans it).

param(
    [string]$OutDir = (Join-Path $env:APPDATA "DanceOverlay\Characters")
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

$cellW = 128
$cellH = 160
$cols  = 8
$rows  = 3

$sheet = New-Object System.Drawing.Bitmap ($cellW * $cols), ($cellH * $rows)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

# transparent background (PNG alpha carries it into the overlay)
$g.Clear([System.Drawing.Color]::Transparent)

$shadowBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(70, 0, 0, 0))
$stockPen    = New-Object System.Drawing.Pen      ([System.Drawing.Color]::FromArgb(255, 252, 252, 244)), 8
$shoeBrush   = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 79, 129, 189))
$dressBrush  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 111, 168, 220))
$trimPen     = New-Object System.Drawing.Pen      ([System.Drawing.Color]::FromArgb(255, 253, 247, 234)), 7
$skinBrush   = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 247, 223, 200))
$hairBrush   = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 233, 188, 87))
$eyeBrush    = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 91, 70, 50))
$blushBrush  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(55, 232, 138, 138))
$facePen     = New-Object System.Drawing.Pen      ([System.Drawing.Color]::FromArgb(255, 91, 70, 50)), 3

function Draw-Demo {
    param($graphics, $ox, $oy, $phase, $held, $waiting)

    $cx = $ox + $cellW / 2
    $groundY = $oy + $cellH - 22

    if ($held) {
        # "held": arms up, no bounce
        $bodyY = $groundY - 52
        $tilt  = 3
    } elseif ($waiting) {
        # "waiting": gentle breathing, arms at rest
        $bodyY = $groundY - 52 - 3 * [Math]::Abs([Math]::Sin($phase * [Math]::PI * 2))
        $tilt  = 3 * [Math]::Sin($phase * [Math]::PI * 2)
    } else {
        # dance: one squash/stretch bounce across the 8 cells
        $swing = [Math]::Sin($phase * [Math]::PI * 2)
        $bodyY = $groundY - 52 - 20 * [Math]::Abs([Math]::Cos($phase * [Math]::PI * 2))
        $tilt  = 8 * $swing
    }

    # shadow
    $graphics.FillEllipse($shadowBrush, ($cx - 23), ($groundY - 8), 46, 8)

    # legs + shoes
    foreach ($dx in (-10, 10)) {
        $graphics.DrawLine($stockPen, ($cx + $dx), ($bodyY + 22), ($cx + $dx + $tilt), ($groundY - 4))
        $goods = $groundY - 4
        $graphics.FillEllipse($shoeBrush, ($cx + $dx + $tilt - 9), ($goods - 5), 18, 10)
    }

    # dress (trapezoid)
    $p1 = New-Object System.Drawing.PointF ($cx - 16), ($bodyY - 30)
    $p2 = New-Object System.Drawing.PointF ($cx + 16), ($bodyY - 30)
    $p3 = New-Object System.Drawing.PointF ($cx + 26), ($bodyY + 24)
    $p4 = New-Object System.Drawing.PointF ($cx - 26), ($bodyY + 24)
    $pts = New-Object 'System.Drawing.PointF[]' 4
    $pts[0] = $p1; $pts[1] = $p2; $pts[2] = $p3; $pts[3] = $p4
    $graphics.FillPolygon($dressBrush, $pts)

    # arms
    if ($held) {
        $aL = 2.4; $aR = -2.4
    } elseif ($waiting) {
        $aL = 0.35 + 0.25 * [Math]::Sin($phase * [Math]::PI * 2)
        $aR = -0.35 - 0.25 * [Math]::Sin($phase * [Math]::PI * 2)
    } else {
        $aL = 0.9 + 1.1 * [Math]::Sin($phase * [Math]::PI * 2)
        $aR = -0.9 - 1.1 * [Math]::Sin($phase * [Math]::PI * 2)
    }
    foreach ($pair in @(@(-14, $aL, -1), @(14, $aR, 1))) {
        $sx = $cx + $pair[0]; $sy = $bodyY - 20
        $ex = $sx + 30 * [Math]::Sin($pair[1]) * $pair[2]
        $ey = $sy + 26 * [Math]::Cos($pair[1])
        $graphics.DrawLine($trimPen, $sx, $sy, $ex, $ey)
        $graphics.FillEllipse($skinBrush, ($ex - 7), ($ey - 7), 14, 14)
    }

    # head: hair + face
    $headY = $bodyY - 42
    $graphics.FillEllipse($hairBrush, ($cx - 20), ($headY - 26), 40, 46)
    $graphics.FillEllipse($skinBrush, ($cx - 16), ($headY - 18), 32, 32)

    # face
    if ($held) {
        $graphics.DrawLine($facePen, ($cx - 10), ($headY - 4), ($cx - 4), ($headY - 4))
        $graphics.DrawLine($facePen, ($cx + 4),  ($headY - 4), ($cx + 10), ($headY - 4))
    } else {
        foreach ($dx in (-7, 7)) {
            $graphics.FillEllipse($eyeBrush, ($cx + $dx - 3), ($headY - 6), 6, 8)
        }
    }
    foreach ($dx in (-12, 12)) {
        $graphics.FillEllipse($blushBrush, ($cx + $dx - 5), ($headY + 2), 10, 6)
    }
    $graphics.DrawLine($facePen, ($cx - 5), ($headY + 6), ($cx + 5), ($headY + 6))
}

# row 0 "Demo" (dance), row 1 "Waiting" (idle breath), row 2 "Held" (drag pose)
for ($c = 0; $c -lt $cols; $c++) {
    Draw-Demo -graphics $g -ox ($c * $cellW) -oy 0 -phase ($c / $cols) -held $false -waiting $false
}
for ($c = 0; $c -lt $cols; $c++) {
    Draw-Demo -graphics $g -ox ($c * $cellW) -oy $cellH -phase ($c / $cols) -held $false -waiting $true
}
for ($c = 0; $c -lt $cols; $c++) {
    Draw-Demo -graphics $g -ox ($c * $cellW) -oy (2 * $cellH) -phase ($c / $cols) -held $true -waiting $false
}

$g.Dispose()

$pngPath = Join-Path $OutDir "Demo.png"

$ms = New-Object System.IO.MemoryStream
try {
    $sheet.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    [IO.File]::WriteAllBytes($pngPath, $ms.ToArray())
} finally {
    $ms.Dispose()
    $sheet.Dispose()
}

if ((Get-Item $pngPath).Length -le 0) {
    throw "Save produced a 0-byte file: $pngPath"
}

$txtPath = Join-Path $OutDir "Demo.txt"
Set-Content -Path $txtPath -Value @("Demo", "Waiting", "Held") -Encoding UTF8

Write-Output "OK: $pngPath"
Write-Output "OK: $txtPath"
