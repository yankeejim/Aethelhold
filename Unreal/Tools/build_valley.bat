@echo off
rem Rebuilds the founding map (Content/Maps/Valley) from Tools/build_valley.py.
rem The editor opens, builds, saves and closes by itself. Close any open editor first.
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0..\Aethelhold.uproject" -ExecutePythonScript="%~dp0build_valley.py" -NoSplash -Unattended
