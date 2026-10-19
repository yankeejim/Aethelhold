@echo off
rem Builds the Aethelhold editor (C++). Close the editor first.
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AethelholdEditor Win64 Development -Project="%~dp0Aethelhold.uproject" -WaitMutex
