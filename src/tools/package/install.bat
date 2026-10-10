@echo off
rem Installs Amnesia: A Machine for Pigs VR into the game's folder (as a VR subfolder). Double-click to run.
rem A game folder can also be given: install.bat "D:\Games\AmnesiaAMFP"
if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Game "%~1"
)
