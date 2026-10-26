@echo off
rem Bath handle test in VR (02_mansion_02): starts at "CheckpointStart_3", about 6 m from "BathLever", the small wooden
rem handle that turns the rotating bath (secret passage). Touch it, hold the trigger and lift/lower your hand; the bath
rem turns, and the handle can be used again after about 5 seconds.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "02_mansion_02.map" -StartPos "CheckpointStart_3" -VR %*
pause
