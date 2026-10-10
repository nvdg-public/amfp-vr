@echo off
rem Runs our aamfp.exe against the Epic game data without writing to the game folder.
cd /d "C:\Program Files\Epic Games\AmnesiaAMFP"
start "" "%~dp0aamfp.exe" %*
