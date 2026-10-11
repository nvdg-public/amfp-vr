# Builds the portable VR package in dist\AmnesiaAMFP_VR (plan.md Phase 6b).
# Takes the freshly built exe from src\Game\redist, the launcher, openvr_api.dll and VR files from run\ (no game DLLs), the converted
# animation list and converter (fbx2anm) and the x86 Visual C++ runtime from Visual Studio. Contains no game data.
# Usage: powershell -ExecutionPolicy Bypass -File src\tools\package\make_package.ps1
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$run = Join-Path $root 'run'
$out = Join-Path $root 'dist\AmnesiaAMFP_VR'

$exe = Join-Path $root 'src\Game\redist\aamfp.exe'
if (-not (Test-Path $exe)) { throw "Build first: $exe is missing (src\build.bat)" }

# x86 Visual C++ runtime, app-local
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
$crt = Get-ChildItem "$vs\VC\Redist\MSVC\*\x86\Microsoft.VC*.CRT" -Directory | Sort-Object FullName | Select-Object -Last 1
if (-not $crt) { throw 'Visual C++ x86 redistributable folder not found in Visual Studio' }

if (Test-Path $out) {
    # Keep the remembered game folder of an existing package build
    Get-ChildItem $out -Force | Where-Object { $_.Name -ne 'vr_package.cfg' } | ForEach-Object { Remove-Item $_.FullName -Recurse -Force }
} else {
    New-Item -ItemType Directory -Force $out | Out-Null
}

# The launcher copies the game's engine DLLs (SDL2, OpenAL, ...) from the player's own game folder on start:
# the package ships none of them, only our exes and Valve's openvr_api.dll
$launcher = Join-Path $root 'src\Game\redist\aamfp_vr.exe'
if (-not (Test-Path $launcher)) { throw "Build first: $launcher is missing (src\build.bat game)" }
Copy-Item $launcher (Join-Path $out 'aamfp_vr.exe')
Copy-Item $exe (Join-Path $out 'aamfp_vr_game.exe')
Copy-Item (Join-Path $run 'openvr_api.dll') $out
foreach ($d in 'msvcp140.dll','vcruntime140.dll') { Copy-Item (Join-Path $crt.FullName $d) $out }
foreach ($f in 'vr_actions.json','vr_bindings_knuckles.json','vr_bindings_oculus_touch.json','vr_bindings_vive_controller.json','vr_bindings_frame_controller.json','vr_settings.cfg') { Copy-Item (Join-Path $run $f) $out }
Copy-Item (Join-Path $run 'vr_assets') $out -Recurse
# No converted animations in the package (they are made from game files): the exe converts them on first start
Copy-Item (Join-Path $root 'src\tools\fbx2anm\fbx2anm.exe') $out
Copy-Item (Join-Path $PSScriptRoot 'vr_animations.txt') $out
Copy-Item (Join-Path $PSScriptRoot 'README.txt') $out
foreach ($f in 'install.bat','install.ps1') { Copy-Item (Join-Path $PSScriptRoot $f) $out }
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $out 'LICENSE.txt')
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.txt') $out
if (-not (Test-Path (Join-Path $out 'vr_package.cfg'))) { Copy-Item (Join-Path $PSScriptRoot 'vr_package.cfg') $out }

$size = (Get-ChildItem $out -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
"Package: $out ({0:N1} MB, runtime from $($crt.FullName))" -f $size
