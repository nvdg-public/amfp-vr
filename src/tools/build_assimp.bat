@echo off
rem Fetches Assimp (BSD-3) into tools\thirdparty and builds a static Win32 Release library with only the FBX importer.
setlocal
set "ROOT=%~dp0"
set "SRC=%ROOT%thirdparty\assimp"
set "BLD=%ROOT%thirdparty\assimp-build"
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1

if not exist "%SRC%\CMakeLists.txt" git clone --depth 1 --branch v6.0.5 https://github.com/assimp/assimp "%SRC%" || exit /b 1

cmake -S "%SRC%" -B "%BLD%" -G "Visual Studio 18 2026" -A Win32 ^
  -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_BUILD_SAMPLES=OFF ^
  -DASSIMP_INSTALL=OFF -DASSIMP_BUILD_ZLIB=ON -DASSIMP_WARNINGS_AS_ERRORS=OFF ^
  -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF -DASSIMP_BUILD_FBX_IMPORTER=ON ^
  -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF -DASSIMP_NO_EXPORT=ON ^
  -DUSE_STATIC_CRT=OFF || exit /b 1
cmake --build "%BLD%" --config Release --parallel || exit /b 1
echo === Assimp OK
endlocal
