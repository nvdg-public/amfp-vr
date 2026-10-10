@echo off
rem Enemy animation test in VR: starts our build in 13_pigline next to a patrolling wretch.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -VR %*
pause
