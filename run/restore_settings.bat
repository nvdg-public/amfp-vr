@echo off
rem Puts back the main_settings.cfg saved by the last test launcher run (play_*_test.bat), if that run was interrupted.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_pending.ps1" -Verbose
pause
