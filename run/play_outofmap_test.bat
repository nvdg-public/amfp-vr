@echo off
rem Out-of-map test in VR: 02_mansion_02 at MainStart. 15 s after each start or respawn you are put 3 m outside the
rem level (beside it, at your current height): the game must kill you right away and restart you at the last checkpoint (Body OutOfMapKill). Change the seconds below.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
set "HPL_VR_TEST_OUTOFMAP=15"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "02_mansion_02.map" -StartPos "MainStart" -VR %*
pause
