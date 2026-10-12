@echo off
rem Builds the game (full rebuild), the package in dist\AmnesiaAMFP_VR and the release zip
rem dist\amfp-vr-v<yyyyMMdd-HHmm>.zip. The zip always gets an empty vr_package.cfg (no remembered game folder).
rem Usage: double-click, or: src\make_release.bat
setlocal
for %%i in ("%~dp0..") do set "ROOT=%%~fi"

call "%~dp0build.bat" all rebuild || goto failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package\make_package.ps1" || goto failed

powershell -NoProfile -Command ^
  "$ErrorActionPreference = 'Stop';" ^
  "$dist = Join-Path '%ROOT%' 'dist';" ^
  "$zip = Join-Path $dist ('amfp-vr-v' + (Get-Date -Format 'yyyyMMdd-HHmm') + '.zip');" ^
  "$stage = Join-Path $env:TEMP ('amfp-vr-release-' + [guid]::NewGuid());" ^
  "Copy-Item (Join-Path $dist 'AmnesiaAMFP_VR') (Join-Path $stage 'AmnesiaAMFP_VR') -Recurse;" ^
  "Copy-Item '%~dp0tools\package\vr_package.cfg' (Join-Path $stage 'AmnesiaAMFP_VR') -Force;" ^
  "if (Test-Path $zip) { Remove-Item $zip };" ^
  "Compress-Archive (Join-Path $stage 'AmnesiaAMFP_VR') $zip;" ^
  "Remove-Item $stage -Recurse -Force;" ^
  "'Release zip: {0} ({1:N1} MB)' -f $zip, ((Get-Item $zip).Length / 1MB)" || goto failed

pause
exit /b 0

:failed
echo.
echo Release build failed, no zip made.
pause
exit /b 1
