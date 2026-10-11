@echo off
rem Pigman test in VR: starts our build in 07_tunnels at "CheckPointCooler", about 5 m from where the wretch
rem "wretch_5" sleeps. Walk about 10 m toward the pig's spot and past it (trigger area "AreaPig5Start"): the lamps flicker, the pig wakes up and chases you, always knowing where you are.
rem If that spot doesn't work, try the cage room pig instead (a short scripted walk-by):
rem   play_pigman_test.bat AtCageRoomStart
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
set START=%~1
if "%START%"=="" set START=CheckPointCooler
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "07_tunnels.map" -StartPos "%START%" -VR
pause
