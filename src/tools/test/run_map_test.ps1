# Starts our build straight into a map (no menu, no input needed), waits until the map has loaded and
# a few seconds have passed, saves the log, then closes the game window.
# Main settings must already have ShowMenu/ShowPreMenu = false (see phase1b report).
param([string]$Map = "02_mansion_02.map", [int]$SettleSeconds = 15, [string]$Tag = "test", [string]$Overlay = "")

$Root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$Game = "C:\Program Files\Epic Games\AmnesiaAMFP"
$Log = "$([Environment]::GetFolderPath("MyDocuments"))\Amnesia\Pig\hpl.log"

if (Get-Process aamfp -ErrorAction SilentlyContinue) { throw "aamfp.exe is already running; not starting a test" }

if ($Overlay -eq "") { $Overlay = "$Root\data_overlay" }
$env:HPL_CACHE_OVERLAY = $Overlay
$proc = Start-Process -FilePath "$Root\run\aamfp.exe" -ArgumentList "-map $Map" -WorkingDirectory $Game -PassThru

$t0 = Get-Date
$loaded = $false
while (((Get-Date) - $t0).TotalSeconds -lt 180 -and -not $proc.HasExited) {
    Start-Sleep -Seconds 2
    $lines = Get-Content $Log -ErrorAction SilentlyContinue
    if ($lines -match "Loading map '$([regex]::Escape($Map))'" -and ($lines -match 'Loading complete')) { $loaded = $true; break }
}
if ($loaded) { Start-Sleep -Seconds $SettleSeconds }
"map=$Map loaded=$loaded exited=$($proc.HasExited) after $([int]((Get-Date) - $t0).TotalSeconds)s"

Copy-Item $Log "$Root\phase1b\logs\hpl.$Tag.log" -Force
if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    if (-not $proc.WaitForExit(20000)) { $proc.Kill() }
}
Copy-Item $Log "$Root\phase1b\logs\hpl.$Tag.log" -Force
