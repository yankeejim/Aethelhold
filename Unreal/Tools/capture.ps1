# Saves a screenshot of the running Aethelhold window (even when it is behind other windows).
#   powershell -File Tools\capture.ps1 [output.png]
param([string]$Out = "$PSScriptRoot\..\Saved\capture.png")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$proc = Get-Process UnrealEditor -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like "*Aethelhold*" } |
    Select-Object -First 1
if (-not $proc) { Write-Error "No Aethelhold window found"; exit 1 }

$rect = New-Object Win+RECT
[Win]::GetClientRect($proc.MainWindowHandle, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left; $h = $rect.Bottom - $rect.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
# 3 = client area only, and render full content (needed for DirectX windows)
[Win]::PrintWindow($proc.MainWindowHandle, $dc, 3) | Out-Null
$g.ReleaseHdc($dc)
$bmp.Save((Resolve-Path -LiteralPath (Split-Path $Out) | Join-Path -ChildPath (Split-Path $Out -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "saved $Out"
