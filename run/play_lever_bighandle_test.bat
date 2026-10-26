@echo off
rem Big lever test in VR (06_factory): starts at "AtPistionLevers", about 3 m from three large lever handles
rem ("Lever1".."Lever3", tesla_dressing_handle, the same type as the 10_reactor main levers) that start the pistons.
rem In the story they are locked until the furnaces are lit, so this test unlocks stuck levers when you touch them
rem (-FreeLevers). Touch one, hold the trigger and move it with your hand around its hinge.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "06_factory.map" -StartPos "AtPistionLevers" -FreeLevers -VR %*
pause
