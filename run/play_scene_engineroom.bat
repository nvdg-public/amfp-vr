@echo off
rem Head-move test 3: 11_engine_room, start "InBigRoom", about 10 m from the trigger area "HitchcockZoomTrigger"
rem (zoom that pushes your view 1 m forward, then a fade out). Walk around the big room to find it. If a door blocks
rem the way, use play_scene_headoffset.bat instead.
rem Uses a separate "dev_user" profile; your main_settings.cfg is restored when the game closes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0play_enemy_test.ps1" -Map "11_engine_room.map" -StartPos "InBigRoom" -VR %*
pause
