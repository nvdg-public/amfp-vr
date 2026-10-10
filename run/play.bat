@echo off
rem Starts our source-built aamfp.exe against the Epic game data.
rem The working directory is the game folder (only read from). FBX animation caches that the Epic data
rem lacks are read from data_overlay (see src\HPL2\core\sources\impl\MeshLoaderFBXCache.cpp).
set "HPL_CACHE_OVERLAY=%~dp0..\data_overlay"
cd /d "C:\Program Files\Epic Games\AmnesiaAMFP"
start "" "%~dp0aamfp.exe" %*
