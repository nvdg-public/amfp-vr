# Copies the package (dist\AmnesiaAMFP_VR, from make_package.ps1) into the game folder as a VR subfolder and
# makes a desktop shortcut (plan.md Phase 6b). Only <game>\VR is created or replaced; no game file is touched.
# A vr_settings.cfg already in <game>\VR is kept. Missing animations are converted from the game's FBX files. Undo: delete <game>\VR and the shortcut.
param([string]$Game = 'C:\Program Files\Epic Games\AmnesiaAMFP')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$pkg = Join-Path $root 'dist\AmnesiaAMFP_VR'
if (-not (Test-Path (Join-Path $pkg 'aamfp_vr.exe'))) { throw "Run make_package.ps1 first ($pkg)" }
if (-not (Test-Path (Join-Path $Game 'resources.cfg'))) { throw "Not the game folder: $Game" }

$dest = Join-Path $Game 'VR'
$keep = $null
if (Test-Path (Join-Path $dest 'vr_settings.cfg')) { $keep = Join-Path $env:TEMP 'amfp_vr_settings_keep.cfg'; Copy-Item (Join-Path $dest 'vr_settings.cfg') $keep -Force }
if (Test-Path $dest) { Get-ChildItem $dest -Force | ForEach-Object { Remove-Item $_.FullName -Recurse -Force } }
else { New-Item -ItemType Directory $dest | Out-Null }

Get-ChildItem $pkg -Force | Where-Object { $_.Name -ne 'vr_package.cfg' } | Copy-Item -Destination $dest -Recurse
Copy-Item (Join-Path $PSScriptRoot 'vr_package.cfg') $dest   # empty: the game is found as the parent folder
if ($keep) { Move-Item $keep (Join-Path $dest 'vr_settings.cfg') -Force }

# Convert the animations the game has no .anm for now (the exe would otherwise do it on its first start)
$overlay = Join-Path $dest 'data_overlay'
$jobs = foreach ($rel in (Get-Content (Join-Path $dest 'vr_animations.txt') | Where-Object { $_ -and -not $_.StartsWith('#') })) {
    $in = Join-Path $Game $rel
    $outFile = Join-Path $overlay ([IO.Path]::ChangeExtension($rel, '.anm'))
    if ((Test-Path $in) -and -not (Test-Path $outFile)) {
        New-Item -ItemType Directory -Force (Split-Path $outFile) | Out-Null
        "$in`t$outFile"
    }
}
if ($jobs) {
    $list = Join-Path $env:TEMP 'amfp_vr_animations.txt'
    Set-Content $list $jobs -Encoding Default
    & (Join-Path $dest 'fbx2anm.exe') --list $list | Select-Object -Last 1
    Remove-Item $list
} else { 'Animations: all present' }

$lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Amnesia AMFP VR.lnk'
$sc = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$sc.TargetPath = Join-Path $dest 'aamfp_vr.exe'
$sc.WorkingDirectory = $dest
$sc.IconLocation = (Join-Path $Game 'aamfp.exe') + ',0'
$sc.Save()
"Deployed to $dest (settings kept: $([bool]$keep)), shortcut: $lnk"
