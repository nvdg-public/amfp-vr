# Starts our build straight into 13_pigline at the "AtSpotlight" start, next to the trigger that
# activates "ControlWretch" (a wretch on a looping walk/idle patrol below the walkway).
#
# Leaves the player's own profile and saves alone:
#  - uses a separate profile "dev_user" (Documents\Amnesia\Pig\dev_user)
#  - temporarily turns off the intro/menu in main_settings.cfg and turns on the F1 debug menu,
#    then restores main_settings.cfg exactly when the game exits.
param([string]$Map = "13_pigline.map", [string]$StartPos = "AtSpotlight", [switch]$VR)

$ErrorActionPreference = "Stop"
$Run  = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $Run
$Game = "C:\Program Files\Epic Games\AmnesiaAMFP"
$Pig  = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Amnesia\Pig"
$Main = Join-Path $Pig "main_settings.cfg"

if (Get-Process aamfp -ErrorAction SilentlyContinue) { Write-Host "The game is already running. Close it first."; exit 1 }

# ---- back up main_settings.cfg
$BackupDir = Join-Path $Run "settings_backup"
New-Item -ItemType Directory -Force $BackupDir | Out-Null

# If an earlier run was cut off (console closed, game killed), restore that run's backup first.
$Pending = Join-Path $BackupDir "PENDING_RESTORE.txt"
if (Test-Path $Pending) {
    $old = (Get-Content $Pending -Raw).Trim()
    if (Test-Path $old) { Copy-Item $old $Main -Force; Write-Host "Restored settings left over from an interrupted run ($old)" }
    Remove-Item $Pending
}
$Backup = Join-Path $BackupDir ("main_settings.{0:yyyyMMdd_HHmmss}.cfg" -f (Get-Date))
Copy-Item $Main $Backup -Force
Set-Content $Pending $Backup

try {
    # ---- no intro/menu, dev profile, F1 debug menu
    $t = [IO.File]::ReadAllText($Main)
    $t = $t -replace 'ShowMenu="\w+"', 'ShowMenu="false"' -replace 'ShowPreMenu="\w+"', 'ShowPreMenu="false"'
    $t = $t -replace 'DefaultProfileName="[^"]*"', 'DefaultProfileName="dev_user"' -replace 'LoadDebugMenu="\w+"', 'LoadDebugMenu="true"'
    [IO.File]::WriteAllText($Main, $t)

    # ---- dev profile that starts the chosen map at the chosen start position
    $DevProfile = Join-Path $Pig "dev_user"
    New-Item -ItemType Directory -Force $DevProfile | Out-Null
    $UserCfg = Join-Path $DevProfile "user_settings.cfg"
    $src = if (Test-Path $UserCfg) { $UserCfg } else { Join-Path $Game "config\default_user_settings.cfg" }
    $u = [IO.File]::ReadAllText($src)
    $mapEl = "<Map File=`"$Map`" Folder=`"maps/`" StartPos=`"$StartPos`" />"
    if ($u -match '<Map [^>]*/>') { $u = $u -replace '<Map [^>]*/>', $mapEl } else { $u = $u.TrimEnd() + "`r`n" + $mapEl + "`r`n" }
    [IO.File]::WriteAllText($UserCfg, $u)

    # ---- run (game folder as working dir; FBX caches from data_overlay)
    $env:HPL_CACHE_OVERLAY = Join-Path $Root "data_overlay"
    if ($VR) { $env:HPL_VR = "1" } else { Remove-Item Env:HPL_VR -ErrorAction SilentlyContinue }
    Write-Host "Starting $Map at $StartPos ... (this window restores your settings when the game closes)"
    $proc = Start-Process -FilePath (Join-Path $Run "aamfp.exe") -WorkingDirectory $Game -PassThru
    $proc.WaitForExit()
}
finally {
    # ---- restore main_settings.cfg exactly, but only once the game has closed: it writes main_settings.cfg when
    # it exits, which would put the test settings back (e.g. when this window is closed while the game runs).
    # Then PENDING_RESTORE.txt stays and the next play*.bat / restore_settings.bat restores (restore_pending.ps1).
    if ($proc -and -not $proc.HasExited) {
        Write-Host "The game is still running: your settings are restored the next time you start a play*.bat."
    } else {
        Copy-Item $Backup $Main -Force
        Remove-Item $Pending -ErrorAction SilentlyContinue
        Write-Host "Restored $Main"
    }
}
