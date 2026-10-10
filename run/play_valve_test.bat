@echo off
rem Valve test in VR: starts our build in 09_bilge at "WheelStart", about 2 m from the valve
rem "BilgeMachinePowerWheel" (hold the trigger on it and turn it with your hand).
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "09_bilge.map" -StartPos "WheelStart" -VR %*
pause
