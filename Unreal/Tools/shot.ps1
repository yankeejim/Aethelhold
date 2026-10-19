# Launches the game with the camera at a given spot, waits, saves a screenshot and closes it.
#   powershell -File Tools\shot.ps1 -Cam "1100,1008,180,60" -Out Saved\shots\near.png [-Wait 40]
# Cam is X,Y,Yaw,Distance in meters and degrees (see AStewardCamera).
param([string]$Cam, [string]$Out, [int]$Wait = 40)

$root = Split-Path $PSScriptRoot
$editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null

$p = Start-Process $editor -PassThru -ArgumentList @("`"$root\Aethelhold.uproject`"", "-game", "-windowed",
    "-ResX=1920", "-ResY=1080", "-cam=$Cam")
for ($i = 0; $i -lt 120; $i++) {
    $p.Refresh()
    if ($p.MainWindowTitle -like "*Aethelhold*") { break }
    Start-Sleep 2
}
Start-Sleep $Wait
& "$PSScriptRoot\capture.ps1" $Out
Stop-Process -Id $p.Id -Force
