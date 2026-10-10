# VR self-test without input: starts the enemy-test launcher in VR mode on a map, lets it run, collects
# the VR log lines (init, sizes, compositor timing), takes a mirror screenshot and closes the game.
param([string]$Map = "02_mansion_02.map", [int]$Seconds = 45, [string]$Scale = "", [string]$Tag = "vr")

$Run = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $Run
$Log = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Amnesia\Pig\hpl.log"
if (Get-Process aamfp -ErrorAction SilentlyContinue) { throw "aamfp.exe is already running" }

if ($Scale -ne "") { $env:HPL_VR_SCALE = $Scale } else { Remove-Item Env:HPL_VR_SCALE -ErrorAction SilentlyContinue }
Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Run 'play_enemy_test.ps1'),'-Map',$Map,'-StartPos','""','-VR' -WindowStyle Minimized

Start-Sleep 5
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt 180) {
    Start-Sleep 2
    $l = Get-Content $Log -ErrorAction SilentlyContinue
    if (($l -match "Loading map '$([regex]::Escape($Map))'") -and ($l -match 'Loading complete')) { break }
    if ((((Get-Date) - $t0).TotalSeconds -gt 25) -and -not (Get-Process aamfp -ErrorAction SilentlyContinue)) { "game ended early"; break }
}
Start-Sleep $Seconds

. (Join-Path $Root "src\tools\test\gamectl.ps1")
$Global:ShotDir = Join-Path $Root "phase2\screens"
New-Item -ItemType Directory -Force $Global:ShotDir | Out-Null
try { Focus; Start-Sleep 1; Shot "mirror_$Tag" | Out-Null } catch { "no screenshot: $_" }

New-Item -ItemType Directory -Force (Join-Path $Root "phase2\logs") | Out-Null
Copy-Item $Log (Join-Path $Root "phase2\logs\hpl.$Tag.log") -Force
Select-String $Log -Pattern 'Recommended|render size|Display frequency|VR timing|VR: first|FATAL' | ForEach-Object { $_.Line.Trim() }

$p = Get-Process aamfp -ErrorAction SilentlyContinue
if ($p) { $p.CloseMainWindow() | Out-Null; if (-not $p.WaitForExit(15000)) { $p.Kill() } }
Start-Sleep 3
"settings: " + ((Select-String (Join-Path (Split-Path $Log) "main_settings.cfg") -Pattern 'ShowMenu="\w+"').Matches.Value)
