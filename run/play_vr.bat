@echo off
rem VR mode (SteamVR / Valve Index). Same game data, saves and settings as play.bat.
rem VR options (render resolution, menu panel, recenter key, mirror window) are in vr_settings.cfg
rem next to this file. Environment variables HPL_VR_SCALE / HPL_VR_MIRROR override it for quick tests.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_pending.ps1"
set "HPL_CACHE_OVERLAY=%~dp0..\data_overlay"
set "HPL_VR=1"
cd /d "C:\Program Files\Epic Games\AmnesiaAMFP"
start "" "%~dp0aamfp.exe" %*
