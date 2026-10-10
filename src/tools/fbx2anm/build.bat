@echo off
rem Builds fbx2anm.exe (Win32 Release). Needs tools\build_assimp.bat and src\build.bat engine to have run first.
setlocal
set "HERE=%~dp0"
set "TP=%HERE%..\thirdparty"
set "HPL=%HERE%..\..\HPL2"
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1

if not exist "%HERE%obj" mkdir "%HERE%obj"
cl /nologo /EHsc /MD /O2 /std:c++17 /W3 /wd4244 /wd4267 /wd4305 /wd4996 ^
  /DWIN32 /DNDEBUG /D_WINDOWS /DUSE_SDL2 /D_HAS_AUTO_PTR_ETC=1 /DWINDOWS_IGNORE_PACKING_MISMATCH /D_CRT_SECURE_NO_WARNINGS ^
  /I"%TP%\assimp\include" /I"%TP%\assimp-build\include" /I"%TP%\assimp\code" /I"%TP%\assimp\code\AssetLib\FBX" ^
  /I"%HPL%\core\include" /I"%HPL%\dependencies\include" ^
  /Fo"%HERE%obj\\" /Fe"%HERE%fbx2anm.exe" "%HERE%fbx2anm.cpp" ^
  /link /SUBSYSTEM:CONSOLE /LIBPATH:"%HPL%\lib" /LIBPATH:"%HPL%\dependencies\lib\win32" ^
  "%TP%\assimp-build\lib\Release\assimp-vc145-mt.lib" "%TP%\assimp-build\contrib\zlib\Release\zlibstatic.lib" ^
  HPL2_2010.lib angelscript.lib SDL2.lib DevIL.lib glew32.lib zlibwapi.lib OpenAL32.lib alut.lib newton.lib ^
  libogg.lib libvorbis.lib libvorbisfile.lib libtheora.lib opengl32.lib glu32.lib ^
  user32.lib gdi32.lib shell32.lib advapi32.lib ole32.lib winmm.lib comctl32.lib || exit /b 1
echo === fbx2anm OK
endlocal
