@echo off
rem Runs our aamfp.exe against the Epic game data without writing to the game folder.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_pending.ps1"
cd /d "C:\Program Files\Epic Games\AmnesiaAMFP"
start "" "%~dp0aamfp.exe" %*
