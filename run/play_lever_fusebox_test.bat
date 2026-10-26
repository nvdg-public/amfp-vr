@echo off
rem Fuse box lever test in VR (11_engine_room): starts at "StartTestControl", about 3 m from "FuseBoxLever". In the story
rem it is locked until the fuses are out, so this test unlocks stuck levers when you touch them (-FreeLevers), and
rem ScriptDebugOn skips the intro. Touch the lever, hold the trigger and move it with your hand (moving it to its
rem other end triggers its story event: the deadly steam stops).
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "11_engine_room.map" -StartPos "StartTestControl" -ScriptDebug -FreeLevers -VR %*
pause
