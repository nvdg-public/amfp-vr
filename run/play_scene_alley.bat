@echo off
rem Head-move test 2: 04_alley. Walk to the spectacles and false teeth on the table (about 20 m ahead, slightly
rem right of the start) and knock or drop one of them: the "Hitchcock zoom" lifts and pushes your view (1.5 m up,
rem 1 m forward) for 3 s and returns it. Afterwards: turning your head must turn in place.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "04_alley.map" -StartPos "MainStart" -VR %*
pause
