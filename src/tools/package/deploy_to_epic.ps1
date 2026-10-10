# Installs the freshly built package (dist\AmnesiaAMFP_VR, from make_package.ps1) into the game folder as a VR
# subfolder, using the package's own installer (install.ps1): copies the files (keeps an existing vr_settings.cfg),
# copies the game's engine DLLs, converts the animations and makes a desktop shortcut (plan.md Phase 6b).
# No game file is touched. Undo: delete <game>\VR and the shortcut.
param([string]$Game = 'C:\Program Files\Epic Games\AmnesiaAMFP')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$installer = Join-Path $root 'dist\AmnesiaAMFP_VR\install.ps1'
if (-not (Test-Path $installer)) { throw "Run make_package.ps1 first ($installer is missing)" }
& $installer -Game $Game -NoPause
