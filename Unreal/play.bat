@echo off
rem Plays the valley in its own window, without the editor.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0Aethelhold.uproject" -game -windowed -ResX=1920 -ResY=1080 %*
