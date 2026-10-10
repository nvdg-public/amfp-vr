@echo off
rem Puts back the main_settings.cfg saved by the last play_enemy_test run, if that run was interrupted.
powershell -NoProfile -Command "$p='%~dp0settings_backup\PENDING_RESTORE.txt'; if (Test-Path $p) { $b=(Get-Content $p -Raw).Trim(); Copy-Item $b ([Environment]::GetFolderPath('MyDocuments')+'\Amnesia\Pig\main_settings.cfg') -Force; Remove-Item $p; 'Restored from ' + $b } else { 'Nothing to restore.' }"
pause
