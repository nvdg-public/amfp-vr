@echo off
rem Lever test in VR: starts our build in 10_reactor at "AtRodControls", within 2 m of a row of nine switch
rem levers (RodSwitch_1..9; 1, 6, 7 and 8 do something in the story). Touch one with the controller tip, hold the
rem trigger and pull/push it with your hand.
rem (01_mansion_01 does not work for this: its script always runs the intro and moves you away from the start spot.)
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "10_reactor.map" -StartPos "AtRodControls" -VR %*
pause
