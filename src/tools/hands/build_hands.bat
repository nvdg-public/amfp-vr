@echo off
rem Rebuilds the VR hand meshes in run\vr_assets\hands from assets_src\hands (Godot XR Tools hands, CC0).
rem Needs Python 3. The material (vr_hand.mat) is hand-written and kept in run\vr_assets\hands.
set "ROOT=%~dp0..\..\.."
set "SRC=%ROOT%\assets_src\hands\godot-xr-tools"
set "OUT=%ROOT%\run\vr_assets\hands"
if not exist "%OUT%" mkdir "%OUT%"
python -I "%~dp0gltf2msh.py" "%SRC%\model\hand_r.gltf" "%OUT%\vr_hand_r.msh" vr_hand.mat || exit /b 1
python -I "%~dp0gltf2msh.py" "%SRC%\model\hand_l.gltf" "%OUT%\vr_hand_l.msh" vr_hand.mat || exit /b 1
copy /y "%SRC%\textures\african_baseColor.png" "%OUT%\vr_hand_diffuse.png" >nul
copy /y "%SRC%\textures\hands_normal.png" "%OUT%\vr_hand_nrm.png" >nul
copy /y "%SRC%\License.md" "%OUT%\License.md" >nul
echo Hands written to %OUT%
