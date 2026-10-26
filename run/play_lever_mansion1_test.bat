@echo off
rem Secret-door handle test in VR (01_mansion_01, the office). Starts at "CheckpointStart_2": the map script always
rem moves you to its own start, so the launcher moves you back there right after it (-Teleport), with ScriptDebugOn
rem so the intro is skipped. About 3 m away: the rifle on the wall ("GunLever") opens the office secret door; inside,
rem the small wooden handle "BackAreaLever1" (about 6 m) opens/closes the same door, and "BackAreaLever2" (about 14 m,
rem further in) opens the hall secret door. Touch a handle, hold the trigger and lift/lower your hand.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "01_mansion_01.map" -StartPos "CheckpointStart_2" -Teleport "CheckpointStart_2" -ScriptDebug -VR %*
pause
