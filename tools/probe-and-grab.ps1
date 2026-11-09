# Finds the DanceOverlay window (a WS_EX_TOOLWINDOW layered window) by
# scanning top-level windows owned by the process, then captures that region
# plus a margin into a PNG. Prints the rect it used.
# Usage: powershell -ExecutionPolicy Bypass -File probe-and-grab.ps1 -Out shot.png

param(
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Margin = 60
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public class WinProbe {
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lp);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
'@

$procs = @(Get-Process DanceOverlay -ErrorAction Stop)
$ids = $procs | ForEach-Object { [uint32]$_.Id }

$found = [System.Collections.Generic.List[object]]::new()
$cb = [WinProbe+EnumWindowsProc]{
    param($hWnd, $lp)
    if ([WinProbe]::IsWindowVisible($hWnd)) {
        $pidOut = 0
        [void][WinProbe]::GetWindowThreadProcessId($hWnd, [ref]$pidOut)
        if ($ids -contains $pidOut) {
            $r = New-Object WinProbe+RECT
            [void][WinProbe]::GetWindowRect($hWnd, [ref]$r)
            if (($r.Right - $r.Left) -gt 0) { $found.Add($r) }
        }
    }
    return $true
}
[void][WinProbe]::EnumWindows($cb, [IntPtr]::Zero)

if ($found.Count -eq 0) { throw "no DanceOverlay window found" }

# biggest window = the overlay canvas
$best = $found | Sort-Object { ($_.Right - $_.Left) * ($_.Bottom - $_.Top) } -Descending | Select-Object -First 1

$left  = $best.Left  - $Margin
$top   = $best.Top   - $Margin
$w = ($best.Right - $best.Left)  + 2 * $Margin
$h = ($best.Bottom - $best.Top) + 2 * $Margin

$bmp = New-Object System.Drawing.Bitmap $w, $h
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

Write-Output ("OK: {0} rect=({1},{2})-({3},{4})" -f $Out, $left, $top, ($left + $w), ($top + $h))
