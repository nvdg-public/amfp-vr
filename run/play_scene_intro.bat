@echo off
rem Head-move test 1: a new game in 01_mansion_01. The wake-up intro plays (lying in bed, the script moves your head
rem sideways and forward and back very slowly). After it you can walk around: turning your head must turn in place.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "01_mansion_01.map" -StartPos "MainStart" -VR %*
pause
