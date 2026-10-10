@echo off
rem Head-move test 4 (synthetic, works anywhere): 02_mansion_02 with a test that repeats a scripted head move like the
rem cutscenes use: 10 s free, then your view moves 0.3 m right, 0.4 m up and 0.8 m forward and stays there 15 s, then
rem goes back. Look around in both phases: turning your head must always turn in place. Change the offset below.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
set "HPL_VR_TEST_HEADPOS=0.3 0.4 -0.8"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "02_mansion_02.map" -StartPos "MainStart" -VR %*
pause
