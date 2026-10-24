# Puts back the main_settings.cfg saved by a test launcher run (play_enemy_test.ps1) that did not get to restore it
# after the game closed (console window closed while the game ran, game killed, ...). Called by play.bat,
# play_vr.bat, launch.bat and restore_settings.bat; does nothing when there is nothing to restore.
param([switch]$Verbose)

$Run = Split-Path -Parent $MyInvocation.MyCommand.Path
$Pending = Join-Path $Run "settings_backup\PENDING_RESTORE.txt"
$Main = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Amnesia\Pig\main_settings.cfg"

if (-not (Test-Path $Pending)) { if ($Verbose) { Write-Host "Nothing to restore." }; exit 0 }
# The game writes main_settings.cfg when it exits, so restoring while it runs would be overwritten
if (Get-Process aamfp -ErrorAction SilentlyContinue) { Write-Host "The game is still running: close it, then start again to restore your settings."; exit 1 }

$Backup = (Get-Content $Pending -Raw).Trim()
if (Test-Path $Backup) {
    Copy-Item $Backup $Main -Force
    Write-Host "Restored your settings from an interrupted test run ($Backup)"
}
Remove-Item $Pending
