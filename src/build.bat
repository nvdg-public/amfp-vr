@echo off
rem Builds the HPL2 engine library and aamfp.exe (Release, Win32) with Visual Studio 2026 (toolset v145).
rem Usage: build.bat [engine|game|all] [rebuild]   (default: all)
setlocal
set "ROOT=%~dp0"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=all"

rem vcvarsall in VS 2026 calls vswhere.exe from PATH
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
for /f "usebackq delims=" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo Visual Studio with C++ tools not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1

if not exist "%ROOT%HPL2\dependencies\lib\win32" (
  echo Unpacking HPL2\dependencies.zip
  powershell -NoProfile -Command "Expand-Archive -Force '%ROOT%HPL2\dependencies.zip' '%ROOT%HPL2\dependencies'" || exit /b 1
)

if not exist "%ROOT%tools\thirdparty\openvr\headers\openvr.h" (
  echo Fetching OpenVR SDK v2.15.6
  git clone --depth 1 --branch v2.15.6 https://github.com/ValveSoftware/openvr "%ROOT%tools\thirdparty\openvr" || exit /b 1
)

set "MSB=/m /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32"

rem Keep the build folder out of the binaries: the exe stores only the .pdb file name, and __FILE__ strings
rem start at the project folder (src\...) instead of the full path. cl.exe and link.exe read _CL_ / _LINK_.
for %%i in ("%ROOT%..") do set "PROJ=%%~fi\"
set "_CL_=/d1trimfile:%PROJ%"
set "_LINK_=/PDBALTPATH:%%_PDB%%"
rem Second argument "rebuild" rebuilds everything (needed after changing compiler options)
if /i "%~2"=="rebuild" set "MSB=%MSB% /t:Rebuild"

if /i "%TARGET%"=="game" goto game
rem The prebuilt angelscript.lib is VS2010-built and does not link with v145; rebuild it into HPL2\lib
rem (searched before dependencies\lib\win32).
echo === Building AngelScript 2.24.1
msbuild "%ROOT%HPL2\dependencies\sources\angelscript\angelscript.vcxproj" %MSB% /p:PlatformToolset=v145 "/p:OutDir=%ROOT%HPL2\lib\\" "/p:IntDir=%ROOT%HPL2\dependencies\sources\angelscript\Intermediate\\" || exit /b 1
echo === Building HPL2 engine
msbuild "%ROOT%HPL2\core\_HPL2_2010.vcxproj" %MSB% "/p:SolutionDir=%ROOT%HPL2\core\\" || exit /b 1
if /i "%TARGET%"=="engine" goto done

:game
echo === Building aamfp.exe
msbuild "%ROOT%Game\src\game\Lux.vcxproj" %MSB% "/p:SolutionDir=%ROOT%Game\src\game\\" || exit /b 1

:done
echo === Build OK
endlocal
